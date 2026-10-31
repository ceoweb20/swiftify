/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "FakeCore.h"

#include "app/ErrorCenter.h"
#include "app/Settings.h"
#include "core/Async.h"
#include "library/LibraryStore.h"
#include "playback/PlaybackController.h"
#include "playback/DjMix.h"
#include "spotify/SpotifyJson.h"
#include "spotify/CredentialStore.h"
#include "spotify/OAuthFlow.h"
#include "spotify/SpotifySession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <array>
#include <utility>

using namespace swiftify;
using swiftify::testing::FakeCore;

namespace {

/// a token endpoint's answer, given when the test lets it go
class CannedReply : public QNetworkReply {
    Q_OBJECT

public:
    CannedReply(const QNetworkRequest &request, int status, QByteArray body, QObject *parent)
        : QNetworkReply(parent)
        , m_body(std::move(body))
    {
        setRequest(request);
        setUrl(request.url());
        setOperation(QNetworkAccessManager::PostOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status >= 400) {
            setError(QNetworkReply::ProtocolInvalidOperationError, QStringLiteral("HTTP %1").arg(status));
        }
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }

    void finishNow()
    {
        setFinished(true);
        emit metaDataChanged();
        emit readyRead();
        emit finished();
    }

    void setRetryAfter(const QByteArray &value) { setRawHeader("Retry-After", value); }

    void abort() override {}
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *data, qint64 maximum) override
    {
        const qint64 count = std::min<qint64>(maximum, m_body.size() - m_offset);
        memcpy(data, m_body.constData() + m_offset, size_t(count));
        m_offset += count;
        return count;
    }

private:
    QByteArray m_body;
    qint64 m_offset = 0;
};

/// spotify's accounts service, as far as tokens go: every refresh rotates
/// the refresh token, and spending one it already replaced revokes the whole
/// login it belonged to
class FakeAccounts : public QNetworkAccessManager {
    Q_OBJECT

public:
    static constexpr auto kHost = "accounts.test";

    int expiresIn = 3600;
    /// replies wait for release() rather than coming back on their own
    bool holding = false;
    int requests = 0;
    QStringList spent;
    QSet<QString> revokedLogins;
    /// the web api's answers by path. any other path is not found
    QHash<QString, QByteArray> api;
    QHash<QString, int> apiStatus;
    QHash<QString, QByteArray> apiRetryAfter;
    /// every web api call, as its method, path and query
    QStringList apiCalls;

    /// a new login, and its first refresh token
    QString login()
    {
        return issue(QStringLiteral("login%1").arg(++m_logins));
    }
    bool isLive(const QString &refreshToken) const
    {
        return m_live.contains(refreshToken) && !revokedLogins.contains(m_loginOf.value(refreshToken));
    }
    void release()
    {
        for (const QPointer<CannedReply> &reply : std::exchange(m_held, {})) {
            if (reply) {
                reply->finishNow();
            }
        }
    }

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *data) override
    {
        if (request.url().host() == QLatin1StringView("api.spotify.com")) {
            const QString query = request.url().query(QUrl::FullyDecoded);
            apiCalls << QStringLiteral("%1 %2%3").arg(
                request.attribute(QNetworkRequest::CustomVerbAttribute).toString(), request.url().path(),
                query.isEmpty() ? QString() : u'?' + query);
            const auto found = api.constFind(request.url().path());
            return found == api.cend() ? reply(request, 404, R"({"error":{"status":404,"message":"Not found"}})")
                                       : reply(request, apiStatus.value(request.url().path(), 200), *found);
        }
        if (request.url().host() != QLatin1StringView(kHost)) {
            return QNetworkAccessManager::createRequest(operation, request, data);
        }
        ++requests;
        const QUrlQuery form(QString::fromUtf8(data ? data->readAll() : QByteArray()));
        const QString grant = form.queryItemValue(QStringLiteral("grant_type"));
        if (grant == QLatin1StringView("authorization_code")) {
            if (form.queryItemValue(QStringLiteral("code")) != QLatin1StringView("approved")) {
                return reply(request, 400, R"({"error":"invalid_grant","error_description":"Invalid authorization code"})");
            }
            return issued(request, login());
        }
        const QString token = form.queryItemValue(QStringLiteral("refresh_token"));
        spent << token;
        const QString owner = m_loginOf.value(token);
        if (!isLive(token)) {
            // a token spotify already replaced: the login goes with it
            if (!owner.isEmpty()) {
                revokedLogins.insert(owner);
            }
            return reply(request, 400, R"({"error":"invalid_grant","error_description":"Refresh token revoked"})");
        }
        m_live.remove(token);
        return issued(request, issue(owner));
    }

private:
    QString issue(const QString &owner)
    {
        const QString token = QStringLiteral("refresh%1").arg(++m_tokens);
        m_live.insert(token);
        m_loginOf.insert(token, owner);
        return token;
    }

    QNetworkReply *issued(const QNetworkRequest &request, const QString &refreshToken)
    {
        const QJsonObject body {{QStringLiteral("access_token"), QStringLiteral("access%1").arg(++m_access)},
                                {QStringLiteral("refresh_token"), refreshToken},
                                {QStringLiteral("expires_in"), expiresIn}};
        return reply(request, 200, QJsonDocument(body).toJson(QJsonDocument::Compact));
    }

    QNetworkReply *reply(const QNetworkRequest &request, int status, const QByteArray &body)
    {
        auto *answer = new CannedReply(request, status, body, this);
        if (apiRetryAfter.contains(request.url().path())) {
            answer->setRetryAfter(apiRetryAfter.value(request.url().path()));
        }
        if (holding) {
            m_held.append(answer);
        } else {
            QTimer::singleShot(0, answer, [answer] { answer->finishNow(); });
        }
        return answer;
    }

    int m_logins = 0;
    int m_tokens = 0;
    int m_access = 0;
    QSet<QString> m_live;
    QHash<QString, QString> m_loginOf;
    QList<QPointer<CannedReply>> m_held;
};

class Gate : public QObject {
    Q_OBJECT

signals:
    void opened();
};

/// a keychain that can be made slow: a write waits until the test opens it
class SlowStore : public CredentialStore {
public:
    bool holding = false;
    QHash<Account, StoredCredentials> items;
    Gate gate;

    QCoro::Task<std::optional<StoredCredentials>> load(Account account) override
    {
        const auto found = items.constFind(account);
        co_return found == items.cend() ? std::nullopt : std::optional(*found);
    }
    QCoro::Task<> save(Account account, StoredCredentials credentials) override
    {
        if (holding) {
            co_await qCoro(&gate, &Gate::opened);
        }
        items.insert(account, credentials);
    }
    QCoro::Task<> removeAll() override
    {
        items.clear();
        co_return;
    }
};

class CountingStore : public MemoryCredentialStore {
public:
    std::array<int, 2> reads {};
    std::array<int, 2> writes {};
    int removals = 0;
    bool rejectWrite = false;
    QCoro::Task<std::optional<StoredCredentials>> load(Account account) override
    {
        ++reads[size_t(account)];
        co_return co_await MemoryCredentialStore::load(account);
    }
    QCoro::Task<> save(Account account, StoredCredentials credentials) override
    {
        ++writes[size_t(account)];
        if (rejectWrite) throw SpotifyError(SpotifyError::Kind::Credentials, QStringLiteral("Fixture denied write"));
        co_await MemoryCredentialStore::save(account, credentials);
    }
    QCoro::Task<> removeAll() override
    {
        ++removals;
        co_await MemoryCredentialStore::removeAll();
    }
};

QCoro::Task<> readCredentials(CredentialStore &store, CredentialStore::Account account,
                              std::optional<StoredCredentials> &result, bool &done)
{
    result = co_await store.load(account); done = true;
}

QCoro::Task<> writeCredentials(CredentialStore &store, CredentialStore::Account account,
                               StoredCredentials credentials, bool &success)
{
    try { co_await store.save(account, credentials); success = true; }
    catch (const SpotifyError &) { success = false; }
}

QCoro::Task<> clearCredentials(CredentialStore &store, bool &done)
{
    co_await store.removeAll(); done = true;
}

QCoro::Task<> fetchLibraryToken(SpotifySession &session, QStringList &tokens)
{
    try {
        tokens << co_await session.withLibraryAccessToken<QString>(
            [](QString accessToken) -> QCoro::Task<QString> { co_return accessToken; });
    } catch (const SpotifyError &error) {
        tokens << QStringLiteral("error: ") + error.message();
    }
}

template<typename T>
QString failureOf(QCoro::Task<T> task)
{
    try {
        QCoro::waitFor(std::move(task));
    } catch (const SpotifyError &error) {
        return error.message();
    }
    return {};
}

void settle()
{
    for (int pass = 0; pass < 10; ++pass) {
        QCoreApplication::processEvents();
    }
}

} // namespace

/// spotify rotates a refresh token every time one is spent and revokes the
/// login when one it already replaced is spent again. the session spends them
/// one at a time, as seldom as it can, keeps every one it is given, and signs
/// itself in again when spotify revokes a login all the same.
class SessionTest : public QObject {
    Q_OBJECT

private:
    struct Rig {
        ErrorCenter errors {nullptr};
        SlowStore store;
        FakeAccounts accounts;
        FakeCore core;
        SpotifySession session {&core, &store, &accounts, &errors};
        QList<QUrl> opened;

        Rig()
        {
            OAuthFlow::setTokenEndpoint(QUrl(QStringLiteral("http://%1/api/token").arg(FakeAccounts::kHost)));
            session.setBackgroundOpener([this](const QUrl &url) { opened << url; });
        }

        void storeLibrary(const QString &refreshToken, const QString &accessToken = {}, int minutesLeft = 0)
        {
            store.items.insert(CredentialStore::Account::Library,
                               {QStringLiteral("library-client"), refreshToken, OAuthFlow::kLibraryAuthorizationVersion,
                                {}, accessToken, QDateTime::currentDateTimeUtc().addSecs(minutesLeft * 60)});
        }
        void storePlayback(const QString &refreshToken, const QString &login = {})
        {
            store.items.insert(CredentialStore::Account::Playback,
                               {QString::fromLatin1(OAuthFlow::kPlaybackClientId), refreshToken,
                                OAuthFlow::kPlaybackAuthorizationVersion, login, {}, {}});
        }
        StoredCredentials stored(CredentialStore::Account account) const { return store.items.value(account); }
    };

    struct LibraryRig : Rig {
        QTemporaryDir directory;
        Settings settings {directory.filePath(QStringLiteral("swiftify.conf"))};
        LibraryStore library {&session, &settings, &errors};
    };

    void signIn(LibraryRig &rig)
    {
        rig.accounts.api.insert(QStringLiteral("/v1/me"), R"({"id":"me","display_name":"Me"})");
        rig.storeLibrary(rig.accounts.login(), QStringLiteral("access"), 60);
        rig.session.restoreSavedSessions();
        QTRY_COMPARE(rig.library.profileId(), QStringLiteral("me"));
        QTRY_VERIFY(rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("/v1/me/player/recently-played")));
        settle();
        rig.errors.clear();
        rig.accounts.apiCalls.clear();
    }

    static QByteArray albumJson()
    {
        return R"({"id":"6eUW0wxWtzkFdaEFsTJto6","uri":"spotify:album:6eUW0wxWtzkFdaEFsTJto6","name":"Whenever You Need Somebody"})";
    }

    static QString linkTo(const char *kind, const char *id)
    {
        return QStringLiteral("https://open.spotify.com/%1/%2").arg(QLatin1StringView(kind), QLatin1StringView(id));
    }

private slots:
    void webApiRateLimitBlocksOtherEndpointsWithoutRetrying()
    {
        FakeAccounts network;
        WebApi api(&network);
        const QString path = QStringLiteral("/v1/me/tracks");
        network.api.insert(path, R"({"error":{"status":429,"message":"Too many requests"}})");
        network.apiStatus.insert(path, 429);
        network.apiRetryAfter.insert(path, "30");
        try {
            QCoro::waitFor(api.savedSongs(QStringLiteral("token")));
            QFAIL("rate-limited request succeeded");
        } catch (const SpotifyError &error) {
            QCOMPARE(error.kind(), SpotifyError::Kind::RateLimited);
            QCOMPARE(error.retryAfterSeconds(), 30);
        }
        QCOMPARE(network.apiCalls.size(), 1);
        try {
            QCoro::waitFor(api.profile(QStringLiteral("token")));
            QFAIL("another endpoint ignored the cooldown");
        } catch (const SpotifyError &error) {
            QCOMPARE(error.kind(), SpotifyError::Kind::RateLimited);
            QVERIFY(error.retryAfterSeconds() > 0);
            QVERIFY(error.retryAfterSeconds() <= 30);
        }
        QCOMPARE(network.apiCalls.size(), 1);
    }

    void webApiRateLimitExpiresWithoutSchedulingRequests()
    {
        FakeAccounts network;
        WebApi api(&network);
        const QString path = QStringLiteral("/v1/me");
        network.api.insert(path, R"({"error":{"status":429,"message":"Too many requests"}})");
        network.apiStatus.insert(path, 429);
        network.apiRetryAfter.insert(path, "1");
        try {
            QCoro::waitFor(api.profile(QStringLiteral("token")));
            QFAIL("rate-limited request succeeded");
        } catch (const SpotifyError &error) {
            QCOMPARE(error.kind(), SpotifyError::Kind::RateLimited);
        }
        QTest::qWait(1100);
        QCOMPARE(network.apiCalls.size(), 1);
        network.apiStatus.remove(path);
        network.api.insert(path, R"({"id":"listener","display_name":"Listener"})");
        const auto profile = QCoro::waitFor(api.profile(QStringLiteral("token")));
        QCOMPARE(profile.id, QStringLiteral("listener"));
        QCOMPARE(network.apiCalls.size(), 2);
    }

    void libraryRateLimitDoesNotBlockAnAlreadyKnownTrack()
    {
        LibraryRig rig;
        rig.storePlayback({}, QStringLiteral("stored-login"));
        signIn(rig);
        const auto connection = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(connection);
        connection->succeed(QStringLiteral("stored-login"));
        QTRY_VERIFY(rig.session.isConnected());
        const QString path = QStringLiteral("/v1/me");
        rig.accounts.apiStatus.insert(path, 429);
        rig.accounts.apiRetryAfter.insert(path, "30");
        try {
            QCoro::waitFor(rig.session.webApi().profile(QStringLiteral("access")));
            QFAIL("rate-limited request succeeded");
        } catch (const SpotifyError &error) {
            QCOMPARE(error.kind(), SpotifyError::Kind::RateLimited);
        }
        const qsizetype requests = rig.accounts.apiCalls.size();
        PlaybackController player(&rig.core, &rig.session, &rig.settings, &rig.errors);
        rig.library.attachPlayer(&player);
        Song song;
        song.uri = QStringLiteral("spotify:track:known");
        song.name = QStringLiteral("Known track");
        rig.library.savedSongs()->setSongs({song});
        player.playFromModel(rig.library.savedSongs(), 0);
        settle();
        const auto track = rig.core.take(QStringLiteral("playTrack"));
        QVERIFY(track);
        track->succeed();
        QTRY_VERIFY(player.isPlaying());
        QVERIFY(rig.session.isConnected());
        QVERIFY(!rig.core.take(QStringLiteral("connectStored")));
        QCOMPARE(rig.accounts.apiCalls.size(), requests);
        rig.core.reset();
    }

    void djMixStartsAQueueFromTheConnectedLibrary()
    {
        LibraryRig rig;
        rig.storePlayback({}, QStringLiteral("stored-login"));
        rig.accounts.api.insert(QStringLiteral("/v1/me/player/recently-played"),
            R"({"items":[{"track":{"uri":"spotify:track:a","name":"A"}}]})");
        rig.accounts.api.insert(QStringLiteral("/v1/me/tracks"),
            R"({"items":[{"track":{"uri":"spotify:track:a","name":"A"}},{"track":{"uri":"spotify:track:b","name":"B"}}]})");
        signIn(rig);
        const auto connection = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(connection);
        connection->succeed(QStringLiteral("stored-login"));
        QTRY_VERIFY(rig.session.isConnected());
        PlaybackController player(&rig.core, &rig.session, &rig.settings, &rig.errors);
        DjMix mix(&rig.session, &rig.library, &player);
        mix.start();
        QTRY_VERIFY(!mix.busy());
        QVERIFY(mix.active());
        QVERIFY(player.hasSong());
        QCOMPARE(player.upNext()->count(), 1);
        QVERIFY(rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("limit=50")));
        const auto track = rig.core.take(QStringLiteral("playTrack"));
        QVERIFY(track);
        track->succeed();
        rig.core.reset();
    }

    void aManualSongChoiceCancelsAnUnfinishedDjMix()
    {
        LibraryRig rig;
        rig.storePlayback({}, QStringLiteral("stored-login"));
        rig.accounts.api.insert(QStringLiteral("/v1/me/player/recently-played"),
            R"({"items":[{"track":{"uri":"spotify:track:a","name":"A"}}]})");
        rig.accounts.api.insert(QStringLiteral("/v1/me/tracks"), R"({"items":[]})");
        signIn(rig);
        const auto connection = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(connection);
        connection->succeed(QStringLiteral("stored-login"));
        QTRY_VERIFY(rig.session.isConnected());
        PlaybackController player(&rig.core, &rig.session, &rig.settings, &rig.errors);
        DjMix mix(&rig.session, &rig.library, &player);
        rig.accounts.holding = true;
        mix.start();
        QVERIFY(mix.busy());
        Song chosen;
        chosen.uri = QStringLiteral("spotify:track:chosen");
        player.play(chosen, {chosen});
        QVERIFY(!mix.busy());
        rig.accounts.release();
        settle();
        QCOMPARE(player.currentSong().uri, chosen.uri);
        QVERIFY(!mix.active());
        rig.core.reset();
    }

    void profileIncludesSubscriptionWithoutAssumingPremium()
    {
        const auto premium = json::profile(QJsonObject{{QStringLiteral("id"), QStringLiteral("account")},
                                                      {QStringLiteral("product"), QStringLiteral("premium")}});
        QCOMPARE(premium.product, QStringLiteral("premium"));
        const auto free = json::profile(QJsonObject{{QStringLiteral("id"), QStringLiteral("account")},
                                                   {QStringLiteral("product"), QStringLiteral("free")}});
        QCOMPARE(free.product, QStringLiteral("free"));
        const auto unknown = json::profile(QJsonObject{{QStringLiteral("id"), QStringLiteral("account")}});
        QVERIFY(unknown.product.isEmpty());
    }

    void secureStoreCacheSkipsRepeatedReadsAndIdenticalWrites()
    {
        CountingStore backend;
        const auto account = CredentialStore::Account::Library;
        const auto index = size_t(account);
        StoredCredentials original {QStringLiteral("fixture-client"), QStringLiteral("fixture-refresh"), 2};
        backend.items.insert(account, original);
        CachedCredentialStore cache(&backend);
        std::optional<StoredCredentials> result;
        bool done = false;
        detach(readCredentials(cache, account, result, done)); QTRY_VERIFY(done);
        QVERIFY(result); QCOMPARE(backend.reads[index], 1);
        done = false;
        detach(readCredentials(cache, account, result, done)); QTRY_VERIFY(done);
        QCOMPARE(backend.reads[index], 1);
        QCOMPARE(result->refreshToken, original.refreshToken);
        bool success = false;
        detach(writeCredentials(cache, account, original, success)); QTRY_VERIFY(success);
        QCOMPARE(backend.writes[index], 0);
        auto updated = original; updated.refreshToken = QStringLiteral("fixture-rotated");
        success = false;
        detach(writeCredentials(cache, account, updated, success)); QTRY_VERIFY(success);
        QCOMPARE(backend.writes[index], 1);
        QCOMPARE(backend.items[account].refreshToken, updated.refreshToken);
        done = false;
        detach(readCredentials(cache, account, result, done)); QTRY_VERIFY(done);
        QCOMPARE(result->refreshToken, updated.refreshToken);
        QCOMPARE(backend.reads[index], 1);
        done = false; detach(clearCredentials(cache, done)); QTRY_VERIFY(done);
        QCOMPARE(backend.removals, 1);
        done = false;
        detach(readCredentials(cache, account, result, done)); QTRY_VERIFY(done);
        QVERIFY(!result); QCOMPARE(backend.reads[index], 1);
    }

    void secureStoreCacheDoesNotPretendAFailedWriteWasSaved()
    {
        CountingStore backend;
        const auto account = CredentialStore::Account::Playback;
        StoredCredentials old {QStringLiteral("fixture-client"), QStringLiteral("fixture-old"), 2};
        backend.items.insert(account, old);
        CachedCredentialStore cache(&backend);
        std::optional<StoredCredentials> result;
        bool done = false;
        detach(readCredentials(cache, account, result, done)); QTRY_VERIFY(done);
        backend.rejectWrite = true;
        auto replacement = old; replacement.refreshToken = QStringLiteral("fixture-new");
        bool success = true;
        detach(writeCredentials(cache, account, replacement, success));
        QTRY_VERIFY(!success);
        done = false;
        detach(readCredentials(cache, account, result, done)); QTRY_VERIFY(done);
        QCOMPARE(result->refreshToken, old.refreshToken);
        QCOMPARE(backend.items[account].refreshToken, old.refreshToken);
    }

    void reopeningAWindowDoesNotRestoreCredentialsAgain()
    {
        CountingStore store;
        ErrorCenter errors(nullptr);
        FakeAccounts network;
        FakeCore core;
        SpotifySession session(&core, &store, &network, &errors);
        session.restoreSavedSessions();
        session.restoreSavedSessions();
        QTRY_COMPARE(store.reads[0], 1);
        QCOMPARE(store.reads[1], 1);
        session.restoreSavedSessions();
        QCOMPARE(store.reads[0], 1);
        session.logOut();
        QTRY_COMPARE(store.removals, 1);
        session.restoreSavedSessions();
        QTRY_COMPARE(store.reads[0], 2);
        QCOMPARE(store.reads[1], 2);
    }

    void savedSongRequestsDisableBothControlsUntilCompletion()
    {
        Rig rig;
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("test.conf")));
        LibraryStore library(&rig.session, &settings, &rig.errors);
        rig.session.setInitialLibraryLoader({});
        rig.storeLibrary(rig.accounts.login(), QStringLiteral("valid-access"), 30);
        rig.session.restoreSavedSessions();
        QTRY_VERIFY(rig.session.hasLibraryToken());
        rig.accounts.api.insert(QStringLiteral("/v1/me/library"), {});
        rig.accounts.holding = true;
        Song song;
        song.uri = QStringLiteral("spotify:track:fixture");
        library.toggleSaved(song);
        QVERIFY(library.isSaved(song.uri));
        QVERIFY(library.isSavePending(song.uri));
        library.toggleSaved(song); // Duplicate click must not reverse a pending request.
        QVERIFY(library.isSaved(song.uri));
        rig.accounts.release();
        QTRY_VERIFY(!library.isSavePending(song.uri));
        QVERIFY(library.isSaved(song.uri));
        library.toggleSaved(song);
        QVERIFY(!library.isSaved(song.uri));
        QVERIFY(library.isSavePending(song.uri));
        rig.accounts.release();
        QTRY_VERIFY(!library.isSavePending(song.uri));
        QVERIFY(!library.isSaved(song.uri));
        rig.accounts.api.remove(QStringLiteral("/v1/me/library")); // Fixture rejects the next save.
        library.toggleSaved(song);
        QVERIFY(library.isSaved(song.uri));
        rig.accounts.release();
        QTRY_VERIFY(!library.isSavePending(song.uri));
        QVERIFY(!library.isSaved(song.uri)); // Failed writes roll back and re-enable the controls.
    }

    void currentTrackSavedStatusIsCheckedWhenPlaybackChanges()
    {
        Rig rig;
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("test.conf")));
        PlaybackController player(&rig.core, &rig.session, &settings, &rig.errors);
        LibraryStore library(&rig.session, &settings, &rig.errors);
        library.attachPlayer(&player);
        rig.session.setInitialLibraryLoader({});
        rig.accounts.api.insert(QStringLiteral("/v1/me/library/contains"), "[true]");
        rig.storeLibrary(rig.accounts.login(), QStringLiteral("valid-access"), 30);
        rig.storePlayback(rig.accounts.login(), QStringLiteral("stored-login"));
        rig.session.restoreSavedSessions();
        settle();
        const auto connect = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(connect);
        connect->succeed(QStringLiteral("stored-login"));
        QTRY_VERIFY(rig.session.isConnected());
        Song song;
        song.uri = QStringLiteral("spotify:track:fixture");
        song.name = QStringLiteral("Fixture");
        player.play(song, {song});
        QTRY_VERIFY(library.isSaved(song.uri));
        rig.core.reset();
    }

    void savedLoginsKeepWhatTheyHold()
    {
        const StoredCredentials credentials {QStringLiteral("client"), QStringLiteral("refresh"), 2,
                                             QStringLiteral("{\"username\":\"me\"}"), QStringLiteral("access"),
                                             QDateTime::fromMSecsSinceEpoch(1'800'000'000'000, QTimeZone::UTC)};
        const auto read = StoredCredentials::fromJson(credentials.toJson());
        QVERIFY(read);
        QCOMPARE(read->refreshToken, credentials.refreshToken);
        QCOMPARE(read->playbackLogin, credentials.playbackLogin);
        QCOMPARE(read->accessToken, credentials.accessToken);
        QCOMPARE(read->accessTokenExpiration, credentials.accessTokenExpiration);
        // what an older build wrote still reads
        const auto old = StoredCredentials::fromJson(R"({"clientID":"c","refreshToken":"r","authorizationVersion":2})");
        QVERIFY(old);
        QVERIFY(old->playbackLogin.isEmpty());
        QVERIFY(!old->accessTokenExpiration.isValid());
        // a playback login whose refresh token is gone still plays
        QVERIFY(StoredCredentials::fromJson(R"({"clientID":"c","refreshToken":"","playbackLogin":"x"})"));
        QVERIFY(!StoredCredentials::fromJson(R"({"clientID":"c","refreshToken":""})"));
    }

    void aLaunchSpendsNoRefreshTokenWhileItsLoginsHold()
    {
        Rig rig;
        rig.storeLibrary(rig.accounts.login(), QStringLiteral("still-good"), 30);
        rig.storePlayback(rig.accounts.login(), QStringLiteral("librespot-login"));
        rig.session.restoreSavedSessions();
        settle();
        // librespot's own login, no token
        const auto call = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(call);
        QCOMPARE(call->argument, QStringLiteral("librespot-login"));
        call->succeed(QStringLiteral("librespot-login"));
        QTRY_VERIFY(rig.session.isConnected());
        QVERIFY(rig.session.hasLibraryToken());
        QVERIFY(!rig.session.isEvaluatingStoredLogin());
        QCOMPARE(rig.accounts.requests, 0);
    }

    void playbackKeepsLibrespotsLoginAndReconnectsWithIt()
    {
        Rig rig;
        rig.storePlayback(rig.accounts.login());
        rig.session.restoreSavedSessions();
        settle();
        // no login of librespot's yet: the token, refreshed once
        auto call = rig.core.take(QStringLiteral("connect"));
        QVERIFY(call);
        QCOMPARE(rig.accounts.requests, 1);
        call->succeed(QStringLiteral("login-a"));
        QTRY_VERIFY(rig.session.isConnected());
        settle();
        const StoredCredentials kept = rig.stored(CredentialStore::Account::Playback);
        QCOMPARE(kept.playbackLogin, QStringLiteral("login-a"));
        QVERIFY(rig.accounts.isLive(kept.refreshToken));

        // a reconnect, as after a failed play or a new output, spends nothing
        bool done = false;
        const auto reconnect = [&](bool force) -> QCoro::Task<> {
            co_await rig.session.ensureFreshPlaybackConnection(force);
            done = true;
        };
        detach(reconnect(true));
        settle();
        call = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(call);
        call->succeed(QStringLiteral("login-a"));
        QTRY_VERIFY(done);
        QCOMPARE(rig.accounts.requests, 1);

        // and when librespot will not take its login, the access token it
        // still has does, unrefreshed
        done = false;
        detach(reconnect(true));
        settle();
        call = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(call);
        call->fail(QStringLiteral("Login failed with reason: Bad credentials"));
        settle();
        call = rig.core.take(QStringLiteral("connect"));
        QVERIFY(call);
        QCOMPARE(call->argument, QStringLiteral("access1"));
        call->succeed(QStringLiteral("login-b"));
        QTRY_VERIFY(done);
        QCOMPARE(rig.accounts.requests, 1);
        QTRY_COMPARE(rig.stored(CredentialStore::Account::Playback).playbackLogin, QStringLiteral("login-b"));
    }

    /// the revocations seen: a refresh spent the old token while the new one
    /// was still being written, as every request after a long sleep did
    void requestsDuringASlowWriteSpendTheNewToken()
    {
        Rig rig;
        // tokens that are always about to run out, so every request refreshes
        rig.accounts.expiresIn = 30;
        rig.storeLibrary(rig.accounts.login());
        rig.session.restoreSavedSessions();
        QTRY_VERIFY(rig.session.hasLibraryToken());
        QCOMPARE(rig.accounts.requests, 1);

        rig.store.holding = true;
        rig.accounts.holding = true;
        QStringList tokens;
        detach(fetchLibraryToken(rig.session, tokens));
        settle();
        QCOMPARE(rig.accounts.requests, 2);
        // spotify answers; its token is being written when more requests come
        rig.accounts.release();
        settle();
        for (int request = 0; request < 3; ++request) {
            detach(fetchLibraryToken(rig.session, tokens));
        }
        settle();
        rig.accounts.release();
        QTRY_COMPARE(tokens.size(), 4);
        for (const QString &token : std::as_const(tokens)) {
            QVERIFY2(token.startsWith(QLatin1StringView("access")), qPrintable(token));
        }
        QVERIFY(rig.accounts.revokedLogins.isEmpty());
        // never the same token twice
        QCOMPARE(QSet<QString>(rig.accounts.spent.begin(), rig.accounts.spent.end()).size(), rig.accounts.spent.size());

        // and what is written, once written, is the token in use
        rig.store.holding = false;
        for (int write = 0; write < 4; ++write) {
            emit rig.store.gate.opened();
            settle();
        }
        QVERIFY(rig.accounts.isLive(rig.stored(CredentialStore::Account::Library).refreshToken));
    }

    void quittingWaitsForARotatedTokenToBeKept()
    {
        Rig rig;
        rig.accounts.expiresIn = 30;
        rig.storeLibrary(rig.accounts.login());
        rig.session.restoreSavedSessions();
        QTRY_VERIFY(rig.session.hasLibraryToken());

        rig.accounts.holding = true;
        QStringList tokens;
        detach(fetchLibraryToken(rig.session, tokens));
        settle();
        // the app quits while spotify is answering
        QTimer::singleShot(50, &rig.accounts, [&] { rig.accounts.release(); });
        rig.session.flushCredentialSaves();
        QVERIFY(rig.accounts.isLive(rig.stored(CredentialStore::Account::Library).refreshToken));
    }

    void aRevokedLoginSignsItselfInAgain()
    {
        Rig rig;
        const QString revoked = rig.accounts.login();
        rig.accounts.revokedLogins.insert(QStringLiteral("login1"));
        rig.storeLibrary(revoked);
        rig.session.restoreSavedSessions();
        // the login page opens in the background, and nothing is shown or
        // asked meanwhile
        QTRY_COMPARE(rig.opened.size(), 1);
        QVERIFY(rig.session.isRepairingLogin());
        QVERIFY(!rig.session.isLoginInProgress());
        QVERIFY(rig.session.isEvaluatingStoredLogin());
        QVERIFY(rig.errors.message().isEmpty());

        // spotify, already approved, sends the browser straight back
        const QString state = QUrlQuery(rig.opened.first()).queryItemValue(QStringLiteral("state"));
        QTcpSocket browser;
        browser.connectToHost(QHostAddress::LocalHost, OAuthFlow::kRedirectPort);
        QVERIFY(browser.waitForConnected(2000));
        browser.write(QStringLiteral("GET /login?code=approved&state=%1 HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n")
                          .arg(state)
                          .toUtf8());
        browser.flush();
        QTRY_VERIFY(rig.session.isLibraryConnected());
        QVERIFY(!rig.session.isRepairingLogin());
        QVERIFY(!rig.session.isEvaluatingStoredLogin());
        QVERIFY(rig.errors.message().isEmpty());
        QTRY_VERIFY(rig.accounts.isLive(rig.stored(CredentialStore::Account::Library).refreshToken));
    }

    void aRevokedPlaybackTokenDoesNotStopPlay()
    {
        Rig rig;
        const QString token = rig.accounts.login();
        rig.accounts.revokedLogins.insert(QStringLiteral("login1"));
        rig.storePlayback(token, QStringLiteral("librespot-login"));
        rig.session.restoreSavedSessions();
        settle();
        const auto call = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(call);
        call->succeed(QStringLiteral("librespot-login"));
        QTRY_VERIFY(rig.session.isConnected());
        QCOMPARE(rig.accounts.requests, 0);
        QVERIFY(rig.opened.isEmpty());
    }

    void aPlaylistSomeoneElseMadeIsReadThroughThePlaybackSession()
    {
        Rig rig;
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        LibraryStore library(&rig.session, &settings, &rig.errors);
        const auto page = [](const char *uri, const char *name) {
            const QJsonObject track {{QStringLiteral("type"), QStringLiteral("track")},
                                     {QStringLiteral("uri"), QLatin1StringView(uri)},
                                     {QStringLiteral("name"), QLatin1StringView(name)}};
            const QJsonArray items {QJsonObject {{QStringLiteral("item"), track}}};
            return QJsonDocument(QJsonObject {{QStringLiteral("items"), items}}).toJson(QJsonDocument::Compact);
        };
        const auto names = [&library] {
            QStringList result;
            for (const Song &song : library.playlistSongs()->songs()) {
                result << song.name;
            }
            return result;
        };
        rig.accounts.api.insert(QStringLiteral("/v1/me"), R"({"id":"me","display_name":"Me"})");
        rig.accounts.api.insert(QStringLiteral("/v1/playlists/mine/items"), page("spotify:track:a", "From The Web Api"));
        rig.storeLibrary(rig.accounts.login(), QStringLiteral("access"), 60);
        rig.storePlayback(rig.accounts.login(), QStringLiteral("librespot-login"));
        rig.session.restoreSavedSessions();
        QTRY_COMPARE(library.profileId(), QStringLiteral("me"));
        settle();
        const auto connect = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(connect);
        connect->succeed(QStringLiteral("librespot-login"));
        QTRY_VERIFY(rig.session.isConnected());

        // the web api only gives its name and cover
        Playlist theirs;
        theirs.id = QStringLiteral("theirs");
        theirs.uri = QStringLiteral("spotify:playlist:theirs");
        theirs.ownerId = QStringLiteral("someone");
        library.selectPlaylist(theirs);
        settle();
        const auto read = rig.core.take(QStringLiteral("playlist"));
        QVERIFY(read);
        QCOMPARE(read->argument, theirs.uri);
        read->succeed(QString::fromUtf8(page("spotify:track:b", "Through The Session")));
        QTRY_COMPARE(names(), QStringList {QStringLiteral("Through The Session")});
        QVERIFY(!library.isLoadingSongs());

        Playlist mine = theirs;
        mine.id = QStringLiteral("mine");
        mine.uri = QStringLiteral("spotify:playlist:mine");
        mine.ownerId = QStringLiteral("me");
        library.selectPlaylist(mine);
        QTRY_COMPARE(names(), QStringList {QStringLiteral("From The Web Api")});
        QVERIFY(!rig.core.take(QStringLiteral("playlist")));
    }

    void anArtistsTopSongsAreReadThroughThePlaybackSession()
    {
        Rig rig;
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        LibraryStore library(&rig.session, &settings, &rig.errors);
        const auto names = [&library] {
            QStringList result;
            for (const Song &song : library.artistTopSongs()->songs()) {
                result << song.name;
            }
            return result;
        };
        rig.accounts.api.insert(QStringLiteral("/v1/me"), R"({"id":"me","display_name":"Me"})");
        rig.accounts.api.insert(QStringLiteral("/v1/artists/sade"),
                                "{\"id\":\"sade\",\"uri\":\"spotify:artist:sade\",\"name\":\"Sade\",\"images\":["
                                "{\"url\":\"https://images.test/sade-small.jpg\",\"width\":160},"
                                "{\"url\":\"https://images.test/sade.jpg\",\"width\":640}]}");
        rig.accounts.api.insert(QStringLiteral("/v1/artists/gone"),
                                "{\"id\":\"gone\",\"uri\":\"spotify:artist:gone\",\"name\":\"Gone\",\"images\":[]}");
        rig.accounts.api.insert(QStringLiteral("/v1/artists/sade/albums"),
                                R"({"items":[{"id":"diamond","name":"Diamond Life","uri":"spotify:album:diamond"}]})");
        rig.accounts.api.insert(QStringLiteral("/v1/artists/gone/albums"),
                                R"({"items":[{"id":"left","name":"Left Behind","uri":"spotify:album:left"}]})");
        rig.storeLibrary(rig.accounts.login(), QStringLiteral("access"), 60);
        rig.storePlayback(rig.accounts.login(), QStringLiteral("librespot-login"));
        rig.session.restoreSavedSessions();
        QTRY_COMPARE(library.profileId(), QStringLiteral("me"));
        settle();
        const auto connect = rig.core.take(QStringLiteral("connectStored"));
        QVERIFY(connect);
        connect->succeed(QStringLiteral("librespot-login"));
        QTRY_VERIFY(rig.session.isConnected());

        Artist sade;
        sade.id = QStringLiteral("sade");
        sade.name = QStringLiteral("Sade");
        QVERIFY(sade.artworkUrl.isEmpty()); // Credits in a track have no portrait.
        library.selectArtist(sade);
        const QUrl portrait(QStringLiteral("https://images.test/sade.jpg"));
        QTRY_COMPARE(library.currentArtist().artworkUrl, portrait);
        const auto read = rig.core.take(QStringLiteral("topTracks"));
        QVERIFY(read);
        QCOMPARE(read->argument, QStringLiteral("spotify:artist:sade"));
        read->succeed(QStringLiteral(
            R"({"tracks":[{"type":"track","uri":"spotify:track:a","name":"Smooth Operator"},)"
            R"({"type":"track","uri":"spotify:track:b","name":"Your Love Is King"}]})"));
        QTRY_COMPARE(names(), (QStringList {QStringLiteral("Smooth Operator"), QStringLiteral("Your Love Is King")}));
        QTRY_COMPARE(library.artistAlbums()->items().size(), 1);
        QVERIFY(!rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("top-tracks")));

        // the page still opens without them
        Artist gone;
        gone.id = QStringLiteral("gone");
        gone.name = QStringLiteral("Gone");
        library.selectArtist(gone);
        settle();
        const auto failed = rig.core.take(QStringLiteral("topTracks"));
        QVERIFY(failed);
        failed->fail(QStringLiteral("Spotify would not send the artist"));
        QTRY_COMPARE(library.artistAlbums()->items().size(), 1);
        QCOMPARE(library.artistAlbums()->items().first().name, QStringLiteral("Left Behind"));
        QVERIFY(names().isEmpty());
        QCOMPARE(rig.errors.message(), QString());
        QVERIFY(library.currentArtist().artworkUrl.isEmpty()); // A genuinely absent photo stays absent.

        // Reopening from the same sparse track credits keeps the fetched portrait.
        library.selectArtist(sade);
        QCOMPARE(library.currentArtist().artworkUrl, portrait);
        settle();
        const auto again = rig.core.take(QStringLiteral("topTracks"));
        QVERIFY(again);
        again->succeed(QStringLiteral("{\"tracks\":[]}"));
        QTRY_VERIFY(!library.isLoadingArtist());
        QCOMPARE(library.currentArtist().artworkUrl, portrait);

        // A late profile from the previous page must not replace the current artist.
        rig.accounts.holding = true;
        library.selectArtist(sade);
        library.selectArtist(gone);
        rig.accounts.holding = false;
        rig.accounts.release();
        settle();
        QCOMPARE(library.currentArtist().id, gone.id);
        QVERIFY(library.currentArtist().artworkUrl.isEmpty());
        const auto latest = rig.core.take(QStringLiteral("topTracks"));
        QVERIFY(latest);
        QCOMPARE(latest->argument, QStringLiteral("spotify:artist:gone"));
        latest->succeed(QStringLiteral("{\"tracks\":[]}"));
        QTRY_VERIFY(!library.isLoadingArtist());

        // A refused refresh neither erases a cached portrait nor blocks albums.
        rig.accounts.api.remove(QStringLiteral("/v1/artists/sade"));
        library.selectArtist(sade);
        settle();
        const auto deniedProfile = rig.core.take(QStringLiteral("topTracks"));
        QVERIFY(deniedProfile);
        deniedProfile->succeed(QStringLiteral("{\"tracks\":[]}"));
        QTRY_VERIFY(!library.isLoadingArtist());
        QCOMPARE(library.currentArtist().artworkUrl, portrait);
        QCOMPARE(library.artistAlbums()->items().size(), 1);
        QCOMPARE(rig.errors.message(), QString());
    }

    void aPlaylistIsUnfollowedThroughTheLibrary()
    {
        Rig rig;
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        LibraryStore library(&rig.session, &settings, &rig.errors);
        rig.accounts.api.insert(QStringLiteral("/v1/me"), R"({"id":"me","display_name":"Me"})");
        rig.accounts.api.insert(QStringLiteral("/v1/me/library"), {});
        rig.accounts.api.insert(QStringLiteral("/v1/me/playlists"), R"({"items":[]})");
        rig.storeLibrary(rig.accounts.login(), QStringLiteral("access"), 60);
        rig.session.restoreSavedSessions();
        QTRY_COMPARE(library.profileId(), QStringLiteral("me"));
        QVERIFY(rig.session.hasFullLibraryAccess());

        QSignalSpy deleted(&library, &LibraryStore::playlistDeleted);
        Playlist theirs;
        theirs.id = QStringLiteral("theirs");
        theirs.uri = QStringLiteral("spotify:playlist:theirs");
        theirs.name = QStringLiteral("Theirs");
        theirs.ownerId = QStringLiteral("someone");
        library.deletePlaylist(theirs);
        QTRY_COMPARE(deleted.size(), 1);
        QCOMPARE(deleted.first().first().toString(), theirs.id);
        QVERIFY(rig.accounts.apiCalls.contains(QStringLiteral("DELETE /v1/me/library?uris=spotify:playlist:theirs")));
        QVERIFY(!rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("followers")));
        QCOMPARE(rig.errors.message(), QString());
    }

    void aPlaylistDescriptionReadsAsPlainText()
    {
        const auto description = [](const char *html) {
            return json::playlist(QJsonObject {{QStringLiteral("id"), QStringLiteral("x")},
                                               {QStringLiteral("description"), QLatin1StringView(html)}})
                .description;
        };
        QCOMPARE(description("JoJo&#x27;s ops &amp; some other bangers."),
                 QStringLiteral("JoJo's ops & some other bangers."));
        QCOMPARE(description(R"(more at <a href="spotify:playlist:y">Part 2</a> &#39;n &quot;more&quot;)"),
                 QStringLiteral("more at Part 2 'n \"more\""));
        // not an entity, left as it is
        QCOMPARE(description("rock &roll; &bogus; a&b"), QStringLiteral("rock &roll; &bogus; a&b"));
    }

    void aTrackSaysWhetherItWillPlay()
    {
        const auto parse = [](const char *object) {
            return json::track(QJsonDocument::fromJson(object).object());
        };
        QVERIFY(parse(R"({"type":"track","uri":"spotify:track:a","name":"A"})")->isPlayable);
        const auto blocked = parse(R"({"type":"track","uri":"spotify:track:a","name":"A","is_playable":false})");
        QVERIFY(!blocked->isPlayable);
        QVERIFY(!Song::fromJson(blocked->toJson()).isPlayable);
        QVERIFY(Song::fromJson(QJsonObject {{QStringLiteral("uri"), QStringLiteral("spotify:track:a")}}).isPlayable);
        // a relinked copy keeps the uri playlists hold
        const auto relinked = parse(R"({"type":"track","uri":"spotify:track:b","name":"A","is_playable":true,)"
                                    R"("linked_from":{"uri":"spotify:track:a"}})");
        QCOMPARE(relinked->uri, QStringLiteral("spotify:track:a"));
    }

    void aLogoutForgetsALoginStillBeingWritten()
    {
        Rig rig;
        rig.storeLibrary(rig.accounts.login());
        rig.store.holding = true;
        rig.session.restoreSavedSessions();
        QTRY_VERIFY(rig.session.hasLibraryToken());
        QSignalSpy loggedOut(&rig.session, &SpotifySession::loggedOut);
        rig.session.logOut();
        settle();
        rig.store.holding = false;
        emit rig.store.gate.opened();
        QTRY_COMPARE(loggedOut.count(), 1);
        settle();
        QVERIFY(rig.store.items.isEmpty());
    }

    void anAlbumIsReadByItsId()
    {
        // Given
        Rig rig;
        rig.accounts.api.insert(QStringLiteral("/v1/albums/4aawyAB9vmqN3uQ7FjRGTy"),
                                R"({"id":"4aawyAB9vmqN3uQ7FjRGTy","uri":"spotify:album:4aawyAB9vmqN3uQ7FjRGTy",)"
                                R"("name":"Global Warming","artists":[{"name":"Pitbull"}],)"
                                R"("images":[{"url":"https:\/\/img.test/cover","width":640}]})");

        // When
        const Album album = QCoro::waitFor(rig.session.webApi().album(QStringLiteral("4aawyAB9vmqN3uQ7FjRGTy"), QStringLiteral("access")));

        // Then
        QCOMPARE(album.id, QStringLiteral("4aawyAB9vmqN3uQ7FjRGTy"));
        QCOMPARE(album.name, QStringLiteral("Global Warming"));
        QCOMPARE(album.artists, QStringLiteral("Pitbull"));
        QCOMPARE(album.artworkUrl, QUrl(QStringLiteral("https://img.test/cover")));
        QCOMPARE(rig.accounts.apiCalls.size(), 1);
        QVERIFY(rig.accounts.apiCalls.first().startsWith(QStringLiteral("GET /v1/albums/4aawyAB9vmqN3uQ7FjRGTy")));
    }

    void anAlbumWithoutAnIdIsUnsupported()
    {
        // Given
        Rig rig;
        rig.accounts.api.insert(QStringLiteral("/v1/albums/4aawyAB9vmqN3uQ7FjRGTy"), "{}");

        // When
        const QString failure = failureOf(rig.session.webApi().album(QStringLiteral("4aawyAB9vmqN3uQ7FjRGTy"), QStringLiteral("access")));

        // Then
        QVERIFY2(failure.contains(QStringLiteral("unsupported album")), qPrintable(failure));
    }

    void aPlaylistIsReadByItsIdWithoutItsTracks()
    {
        // Given
        Rig rig;
        rig.accounts.api.insert(QStringLiteral("/v1/playlists/37i9dQZF1DXcBWIGoYBM5M"),
                                R"({"id":"37i9dQZF1DXcBWIGoYBM5M","name":"Todays Top Hits",)"
                                R"("owner":{"id":"spotify","display_name":"Spotify"},)"
                                R"("images":[{"url":"https:\/\/img.test/playlist","width":640}]})");

        // When
        const Playlist playlist = QCoro::waitFor(rig.session.webApi().playlist(QStringLiteral("37i9dQZF1DXcBWIGoYBM5M"), QStringLiteral("access")));

        // Then
        QCOMPARE(playlist.name, QStringLiteral("Todays Top Hits"));
        QCOMPARE(playlist.ownerId, QStringLiteral("spotify"));
        QCOMPARE(playlist.ownerName, QStringLiteral("Spotify"));
        QCOMPARE(playlist.artworkUrl, QUrl(QStringLiteral("https://img.test/playlist")));
        QCOMPARE(playlist.uri, QStringLiteral("spotify:playlist:37i9dQZF1DXcBWIGoYBM5M"));
        QCOMPARE(rig.accounts.apiCalls.size(), 1);
        QVERIFY(rig.accounts.apiCalls.first().contains(QStringLiteral("fields=")));
        QVERIFY(!rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("/items")));
    }

    void aPlaylistWithoutAnIdIsUnsupported()
    {
        // Given
        Rig rig;
        rig.accounts.api.insert(QStringLiteral("/v1/playlists/37i9dQZF1DXcBWIGoYBM5M"), "{}");

        // When
        const QString failure = failureOf(rig.session.webApi().playlist(QStringLiteral("37i9dQZF1DXcBWIGoYBM5M"), QStringLiteral("access")));

        // Then
        QVERIFY2(failure.contains(QStringLiteral("unsupported playlist")), qPrintable(failure));
    }

    void anArtistIsReadByItsId()
    {
        // Given
        Rig rig;
        rig.accounts.api.insert(QStringLiteral("/v1/artists/4Z8W4fKeB5YxbusRsdQVPb"),
                                R"({"id":"4Z8W4fKeB5YxbusRsdQVPb","uri":"spotify:artist:4Z8W4fKeB5YxbusRsdQVPb",)"
                                R"("name":"Radiohead","images":[{"url":"https:\/\/img.test/artist","width":640}]})");

        // When
        const Artist artist = QCoro::waitFor(rig.session.webApi().artist(QStringLiteral("4Z8W4fKeB5YxbusRsdQVPb"), QStringLiteral("access")));

        // Then
        QCOMPARE(artist.id, QStringLiteral("4Z8W4fKeB5YxbusRsdQVPb"));
        QCOMPARE(artist.name, QStringLiteral("Radiohead"));
        QCOMPARE(artist.artworkUrl, QUrl(QStringLiteral("https://img.test/artist")));
        QCOMPARE(rig.accounts.apiCalls.size(), 1);
    }

    void anArtistWithoutAnIdIsUnsupported()
    {
        // Given
        Rig rig;
        rig.accounts.api.insert(QStringLiteral("/v1/artists/4Z8W4fKeB5YxbusRsdQVPb"), "{}");

        // When
        const QString failure = failureOf(rig.session.webApi().artist(QStringLiteral("4Z8W4fKeB5YxbusRsdQVPb"), QStringLiteral("access")));

        // Then
        QVERIFY2(failure.contains(QStringLiteral("unsupported artist")), qPrintable(failure));
    }

    void aMissingAlbumPlaylistOrArtistResultsInTheNotFound()
    {
        // Given
        Rig rig;
        const QString id = QStringLiteral("4aawyAB9vmqN3uQ7FjRGTy");

        // When
        const QString album = failureOf(rig.session.webApi().album(id, QStringLiteral("access")));
        const QString playlist = failureOf(rig.session.webApi().playlist(id, QStringLiteral("access")));
        const QString artist = failureOf(rig.session.webApi().artist(id, QStringLiteral("access")));

        // Then
        QVERIFY(!album.isEmpty());
        QVERIFY(!playlist.isEmpty());
        QVERIFY(!artist.isEmpty());
    }

    void aPastedTrackLinkOpensItsAlbum()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/tracks/4uLU6hMCjMI75M1A2tKUQC"),
                                R"({"type":"track","uri":"spotify:track:4uLU6hMCjMI75M1A2tKUQC","name":"Never Gonna Give You Up",)"
                                R"("album":)" + albumJson() + '}');
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("track", "4uLU6hMCjMI75M1A2tKUQC"));

        // Then
        QTRY_COMPARE(rig.library.page(), LibraryStore::Page::Album);
        QCOMPARE(rig.library.currentAlbum().id, QStringLiteral("6eUW0wxWtzkFdaEFsTJto6"));
        QVERIFY(!rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("/v1/search")));
    }

    void aPastedAlbumLinkOpensTheAlbum()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/albums/6eUW0wxWtzkFdaEFsTJto6"), albumJson());
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("album", "6eUW0wxWtzkFdaEFsTJto6"));

        // Then
        QTRY_COMPARE(rig.library.page(), LibraryStore::Page::Album);
        QCOMPARE(rig.library.currentAlbum().id, QStringLiteral("6eUW0wxWtzkFdaEFsTJto6"));
    }

    void aPastedPlaylistLinkOpensThePlaylist()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/playlists/37i9dQZF1DXcBWIGoYBM5M"),
                                R"({"id":"37i9dQZF1DXcBWIGoYBM5M","name":"Todays Top Hits","owner":{"id":"spotify"}})");
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("playlist", "37i9dQZF1DXcBWIGoYBM5M"));

        // Then
        QTRY_COMPARE(rig.library.page(), LibraryStore::Page::Playlist);
        QCOMPARE(rig.library.selectedPlaylist().id, QStringLiteral("37i9dQZF1DXcBWIGoYBM5M"));
    }

    void aPastedArtistLinkOpensTheArtist()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/artists/4Z8W4fKeB5YxbusRsdQVPb"),
                                R"({"id":"4Z8W4fKeB5YxbusRsdQVPb","uri":"spotify:artist:4Z8W4fKeB5YxbusRsdQVPb","name":"Radiohead"})");
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("artist", "4Z8W4fKeB5YxbusRsdQVPb"));

        // Then
        QTRY_COMPARE(rig.library.page(), LibraryStore::Page::Artist);
        QCOMPARE(rig.library.currentArtist().id, QStringLiteral("4Z8W4fKeB5YxbusRsdQVPb"));
    }

    void aPastedLinkClearsTheSearchField()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/albums/6eUW0wxWtzkFdaEFsTJto6"), albumJson());
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("album", "6eUW0wxWtzkFdaEFsTJto6"));

        // Then
        QTRY_COMPARE(rig.library.page(), LibraryStore::Page::Album);
        QCOMPARE(rig.library.searchQuery(), QString());
        QVERIFY(!rig.library.isSearching());
    }

    void aPastedLinkIsNotDebounced()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("album", "6eUW0wxWtzkFdaEFsTJto6"));

        // Then
        QTRY_VERIFY_WITH_TIMEOUT(rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("/v1/albums/6eUW0wxWtzkFdaEFsTJto6")), 150);
    }

    void aTrackWithoutAnAlbumShowsAnError()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/tracks/4uLU6hMCjMI75M1A2tKUQC"),
                                R"({"type":"track","uri":"spotify:track:4uLU6hMCjMI75M1A2tKUQC","name":"Never Gonna Give You Up"})");
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("track", "4uLU6hMCjMI75M1A2tKUQC"));

        // Then
        QTRY_VERIFY(rig.errors.message().contains(QStringLiteral("did not return an album")));
        QCOMPARE(rig.library.page(), LibraryStore::Page::Search);
    }

    void aLinkSpotifyDoesNotKnowShowsAnError()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.library.showSearch();
        const QString link = linkTo("album", "6eUW0wxWtzkFdaEFsTJto6");

        // When
        rig.library.setSearchQuery(link);

        // Then
        QTRY_COMPARE(rig.errors.message(), QStringLiteral("Spotify API request failed: Not found"));
        QCOMPARE(rig.library.page(), LibraryStore::Page::Search);
        QVERIFY(!rig.library.isSearching());
        QCOMPARE(rig.library.searchQuery(), link);
    }

    void aLinkIsNotSearchedWithoutALibraryLogin()
    {
        // Given
        LibraryRig rig;
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(linkTo("album", "6eUW0wxWtzkFdaEFsTJto6"));
        settle();

        // Then
        QVERIFY(rig.errors.message().contains(QStringLiteral("Connect your Spotify library")));
        QVERIFY(rig.accounts.apiCalls.isEmpty());
    }

    void aNewQueryCancelsALinkStillLoading()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/albums/6eUW0wxWtzkFdaEFsTJto6"), albumJson());
        rig.library.showSearch();
        rig.accounts.holding = true;
        rig.library.setSearchQuery(linkTo("album", "6eUW0wxWtzkFdaEFsTJto6"));
        QTRY_VERIFY(!rig.accounts.apiCalls.isEmpty());

        // When
        rig.library.setSearchQuery(QString());
        rig.accounts.release();
        settle();

        // Then
        QCOMPARE(rig.library.page(), LibraryStore::Page::Search);
        QVERIFY(!rig.library.isSearching());
    }

    void plainTextStillSearches()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.library.showSearch();

        // When
        rig.library.setSearchQuery(QStringLiteral("radiohead"));

        // Then
        QTRY_VERIFY(rig.accounts.apiCalls.join(u'\n').contains(QStringLiteral("/v1/search")));
        QCOMPARE(rig.library.page(), LibraryStore::Page::Search);
    }

    void backAfterALinkReturnsToACleanSearchPage()
    {
        // Given
        LibraryRig rig;
        signIn(rig);
        rig.accounts.api.insert(QStringLiteral("/v1/albums/6eUW0wxWtzkFdaEFsTJto6"), albumJson());
        rig.library.showSearch();
        rig.library.setSearchQuery(linkTo("album", "6eUW0wxWtzkFdaEFsTJto6"));
        QTRY_COMPARE(rig.library.page(), LibraryStore::Page::Album);

        // When
        rig.library.goBack();

        // Then
        QCOMPARE(rig.library.page(), LibraryStore::Page::Search);
        QCOMPARE(rig.library.searchQuery(), QString());
    }
};

QTEST_GUILESS_MAIN(SessionTest)
#include "tst_session.moc"
