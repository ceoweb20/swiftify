#include "FakeCore.h"
#include "app/ErrorCenter.h"
#include "app/Settings.h"
#include "library/LibraryStore.h"
#include "library/PlaylistCollections.h"
#include "spotify/CredentialStore.h"
#include "spotify/OAuthFlow.h"
#include "spotify/PlaylistLinks.h"
#include "spotify/SpotifySession.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QPointer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <algorithm>
#include <utility>

using namespace swiftify;
namespace {
const QString a = QStringLiteral("37i9dQZF1DXcBWIGoYBM5M");
const QString b = QStringLiteral("37i9dQZF1DX4JAvHpjipBk");
const QString c = QStringLiteral("37i9dQZF1DXcBWIGoYBM5N");

QJsonObject playlist(const QString &id)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("playlist")},
        {QStringLiteral("name"), QStringLiteral("Mix ") + id.right(2)},
        {QStringLiteral("uri"), QStringLiteral("spotify:playlist:") + id},
        {QStringLiteral("collaborative"), true},
        {QStringLiteral("owner"), QJsonObject {{QStringLiteral("id"), QStringLiteral("owner")}}}};
}

QJsonObject context(const QString &id)
{
    return {{QStringLiteral("context"), QJsonObject {{QStringLiteral("type"), QStringLiteral("playlist")},
        {QStringLiteral("uri"), QStringLiteral("spotify:playlist:") + id}}}};
}

class Reply : public QNetworkReply {
public:
    Reply(const QNetworkRequest &request, int status, QByteArray body, QObject *parent)
        : QNetworkReply(parent), m_body(std::move(body))
    {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status == 429) setRawHeader("Retry-After", "5");
        if (status == 302) setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl(QStringLiteral("https://example.test/sink")));
        if (status >= 400) setError(QNetworkReply::ProtocolInvalidOperationError, QStringLiteral("Fixture error"));
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
    void finish() { setFinished(true); emit readyRead(); emit finished(); }
    void abort() override {}
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char *target, qint64 maximum) override
    {
        const qint64 count = std::min<qint64>(maximum, m_body.size() - m_offset);
        memcpy(target, m_body.constData() + m_offset, size_t(count)); m_offset += count; return count;
    }
private:
    QByteArray m_body;
    qint64 m_offset = 0;
};

class Network : public QNetworkAccessManager {
public:
    QList<QNetworkRequest> requests;
    QString user = QStringLiteral("account-A");
    QJsonArray saved;
    QJsonArray history;
    QHash<QString, int> status;
    bool holding = false;
    QList<QPointer<Reply>> held;
    void release() { for (auto reply : std::exchange(held, {})) if (reply) reply->finish(); }
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        requests.append(request);
        const auto path = request.url().path();
        const int code = status.value(path, 200);
        QJsonObject body;
        if (path == QLatin1StringView("/v1/me")) body = {{QStringLiteral("id"), user}};
        else if (path == QLatin1StringView("/v1/me/playlists")) body = {{QStringLiteral("items"), saved}, {QStringLiteral("next"), QJsonValue::Null}};
        else if (path == QLatin1StringView("/v1/me/player/recently-played")) body = {{QStringLiteral("items"), history}};
        else body = playlist(path.section(u'/', -1));
        if (code >= 400) body = {{QStringLiteral("error"), QJsonObject {{QStringLiteral("message"), QStringLiteral("sensitive server detail")}}}};
        auto *reply = new Reply(request, code, QJsonDocument(body).toJson(), this);
        if (holding) held.append(reply);
        else QTimer::singleShot(0, reply, [reply] { reply->finish(); });
        return reply;
    }
};

struct Rig {
    QTemporaryDir directory;
    Settings settings {directory.filePath(QStringLiteral("settings.ini"))};
    ErrorCenter errors {nullptr};
    MemoryCredentialStore credentials;
    Network network;
    testing::FakeCore core;
    SpotifySession session {&core, &credentials, &network, &errors};
    LibraryStore library {&session, &settings, &errors};
    PlaylistCollections collections {&session, &settings, &library};

    Rig()
    {
        // Isolate collection traffic from the application's startup library loader.
        session.setInitialLibraryLoader([]() -> QCoro::Task<> { co_return; });
    }
    void login()
    {
        credentials.items.insert(CredentialStore::Account::Library,
            {QStringLiteral("fixture-client"), QStringLiteral("fixture-refresh"), OAuthFlow::kLibraryAuthorizationVersion,
             {}, QStringLiteral("fixture-access"), QDateTime::currentDateTimeUtc().addSecs(3600)});
        session.restoreSavedSessions();
    }
};
}

class CollectionsTest : public QObject {
    Q_OBJECT
private slots:
    void validatesAndNormalizesLinks()
    {
        for (const auto &text : {QStringLiteral("spotify:playlist:") + a,
             QStringLiteral("https://open.spotify.com/playlist/") + a + QStringLiteral("?si=do-not-store#fragment"),
             QStringLiteral("https://open.spotify.com/intl-ru/playlist/") + a}) {
            QCOMPARE(playlistlinks::id(text).value(), a);
        }
        for (const auto &text : {QStringLiteral("https://evil.test/playlist/") + a,
             QStringLiteral("https://open.spotify.com.evil.test/playlist/") + a,
             QStringLiteral("https://user@open.spotify.com/playlist/") + a,
             QStringLiteral("http://open.spotify.com/playlist/") + a,
             QStringLiteral("spotify:app:dj"), QStringLiteral("https://spotify.link/short"),
             QStringLiteral("https://open.spotify.com/playlist/../../me"), QString(3000, u'a')}) {
            QVERIFY2(!playlistlinks::id(text), qPrintable(text.left(80)));
        }
    }

    void recentContextsAreDeduplicatedAndAppContextsIgnored()
    {
        QJsonArray items {context(a), context(a), context(b), QJsonObject(),
            QJsonObject {{QStringLiteral("context"), QJsonObject {{QStringLiteral("type"), QStringLiteral("app")},
                {QStringLiteral("uri"), QStringLiteral("spotify:app:dj")}}}}};
        QCOMPARE(playlistlinks::recentIds({{QStringLiteral("items"), items}}), (QStringList {a, b}));
    }

    void combinesSourcesAndStoresOnlyAccountScopedIds()
    {
        Rig rig; rig.login(); QTRY_VERIFY(rig.session.hasLibraryToken());
        rig.network.saved = {playlist(a)};
        rig.network.history = {context(a), context(a), context(b)};
        rig.collections.refresh(); QTRY_VERIFY(!rig.collections.busy());
        auto entries = rig.collections.entries();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].toMap().value(QStringLiteral("origin")).toString(), QStringLiteral("saved"));
        QCOMPARE(entries[1].toMap().value(QStringLiteral("origin")).toString(), QStringLiteral("recent"));
        QVERIFY(entries[1].toMap().value(QStringLiteral("available")).toBool());
        const auto recent = entries[1].toMap().value(QStringLiteral("playlist")).value<Playlist>();
        QVERIFY(recent.ownerId.isEmpty()); QVERIFY(!recent.isCollaborative);
        rig.collections.add(QStringLiteral("https://open.spotify.com/playlist/") + b + QStringLiteral("?si=do-not-store"));
        QTRY_VERIFY(!rig.collections.busy());
        QVERIFY(rig.collections.entries()[1].toMap().value(QStringLiteral("linked")).toBool());
        QFile file(rig.directory.filePath(QStringLiteral("settings.ini")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto data = file.readAll();
        QVERIFY(data.contains(b.toUtf8())); QVERIFY(!data.contains("do-not-store")); QVERIFY(!data.contains("fixture-access"));
        const auto count = rig.network.requests.size();
        rig.collections.removeLink(b);
        QCOMPARE(rig.network.requests.size(), count);
        QVERIFY(!rig.collections.entries()[1].toMap().value(QStringLiteral("linked")).toBool());
        for (const auto &request : rig.network.requests) {
            QCOMPARE(request.url().host(), QStringLiteral("api.spotify.com"));
            QCOMPARE(request.url().scheme(), QStringLiteral("https"));
            QCOMPARE(request.rawHeader("User-Agent"), QByteArrayLiteral("Swiftify"));
            QVERIFY(!request.hasRawHeader("app-platform"));
            QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
            QCOMPARE(request.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(), int(QNetworkRequest::Manual));
            QCOMPARE(request.attribute(QNetworkRequest::CookieSaveControlAttribute).toInt(), int(QNetworkRequest::Manual));
            QVERIFY(!request.url().toString().contains(QStringLiteral("do-not-store")));
        }
        QVERIFY(rig.core.pending.isEmpty());
    }

    void denialDoesNotTriggerPrivateApiFallbackOrSaveTheLink()
    {
        Rig rig; rig.login(); QTRY_VERIFY(rig.session.hasLibraryToken());
        rig.network.history = {context(b)};
        rig.network.status.insert(QStringLiteral("/v1/playlists/") + b, 403);
        rig.collections.refresh(); QTRY_VERIFY(!rig.collections.busy());
        QCOMPARE(rig.collections.entries().size(), 1);
        QVERIFY(!rig.collections.entries().first().toMap().value(QStringLiteral("available")).toBool());
        QVERIFY(!rig.collections.status().contains(QStringLiteral("sensitive")));
        rig.collections.open(b);
        QCOMPARE(rig.library.page(), LibraryStore::Page::Home);
        QVERIFY(rig.core.pending.isEmpty());
        rig.collections.add(QStringLiteral("spotify:playlist:") + b);
        QTRY_VERIFY(!rig.collections.busy());
        QVERIFY(!rig.collections.entries().first().toMap().value(QStringLiteral("linked")).toBool());
        for (const auto &request : rig.network.requests) {
            QVERIFY(request.url().path() == QLatin1StringView("/v1/me")
                || request.url().path() == QLatin1StringView("/v1/me/playlists")
                || request.url().path() == QLatin1StringView("/v1/me/player/recently-played")
                || request.url().path() == QStringLiteral("/v1/playlists/") + b);
        }
    }

    void rateLimitStopsTheBatchAndImmediateRetries()
    {
        Rig rig; rig.login(); QTRY_VERIFY(rig.session.hasLibraryToken());
        rig.network.history = {context(b), context(c)};
        rig.network.status.insert(QStringLiteral("/v1/playlists/") + b, 429);
        rig.collections.refresh(); QTRY_VERIFY(!rig.collections.busy());
        QCOMPARE(rig.network.requests.size(), 4);
        const auto count = rig.network.requests.size();
        rig.collections.refresh(); rig.collections.add(QStringLiteral("spotify:playlist:") + c);
        QCOMPARE(rig.network.requests.size(), count);
        QVERIFY(rig.collections.status().contains(QStringLiteral("rate limit")));
    }

    void redirectsAreNotFollowedAndIdentityFailuresClearTheBoard()
    {
        Rig rig; rig.login(); QTRY_VERIFY(rig.session.hasLibraryToken());
        rig.network.history = {context(b)};
        rig.network.status.insert(QStringLiteral("/v1/playlists/") + b, 302);
        rig.collections.refresh(); QTRY_VERIFY(!rig.collections.busy());
        QVERIFY(!rig.collections.entries().first().toMap().value(QStringLiteral("available")).toBool());
        for (const auto &request : rig.network.requests) QCOMPARE(request.url().host(), QStringLiteral("api.spotify.com"));
        rig.network.status.clear();
        rig.collections.refresh(); QTRY_VERIFY(!rig.collections.busy());
        QVERIFY(rig.collections.entries().first().toMap().value(QStringLiteral("available")).toBool());
        rig.network.status.insert(QStringLiteral("/v1/me"), 403);
        rig.collections.refresh(); QTRY_VERIFY(!rig.collections.busy());
        QVERIFY(rig.collections.entries().isEmpty());
        QVERIFY(rig.collections.status().contains(QStringLiteral("verify")));
    }

    void logoutCancelsInflightDataAndDoesNotLeakLinksToAnotherAccount()
    {
        Rig rig; rig.login(); QTRY_VERIFY(rig.session.hasLibraryToken());
        rig.collections.add(QStringLiteral("spotify:playlist:") + b);
        QTRY_VERIFY(!rig.collections.busy());
        rig.session.logOut(); QTRY_VERIFY(!rig.session.hasLibraryToken());
        QVERIFY(rig.collections.entries().isEmpty());
        rig.network.user = QStringLiteral("account-B");
        rig.login(); QTRY_VERIFY(rig.session.hasLibraryToken());
        rig.collections.refresh(); QTRY_VERIFY(!rig.collections.busy());
        QVERIFY(rig.collections.entries().isEmpty());
        rig.network.holding = true;
        rig.collections.refresh(); QVERIFY(rig.collections.busy());
        rig.session.logOut(); QTRY_VERIFY(!rig.session.hasLibraryToken());
        rig.network.release();
        QCoreApplication::processEvents();
        QVERIFY(!rig.collections.busy()); QVERIFY(rig.collections.entries().isEmpty());
    }
};

QTEST_GUILESS_MAIN(CollectionsTest)
#include "tst_collections.moc"
