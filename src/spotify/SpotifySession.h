#pragma once

#include "core/Async.h"
#include "spotify/CredentialStore.h"
#include "spotify/OAuthFlow.h"
#include "spotify/SpotifyError.h"
#include "spotify/WebApi.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

class ErrorCenter;
class PlaybackCore;

/// a token refresh other callers can wait on, so only one runs per login
class TokenRefresh : public QObject {
    Q_OBJECT

signals:
    void finished(bool ok, const QString &accessToken, int errorKind, const QString &errorMessage);
};

/// the two spotify logins and their tokens: playback (librespot, with spotify's
/// own client id) and library (the web api, with the user's developer app).
/// the auth half of PlayerModel.swift.
///
/// spotify rotates a refresh token each time it is spent, and revokes the
/// login when a token it already replaced is spent again. so a refresh token
/// is spent as seldom as possible, by one refresh at a time, and what comes
/// back is held before anything else can run and then kept in the background:
/// - playback reconnects with librespot's own login for the session, which
///   spends no token; the refresh token is only a way back to it
/// - an access token still good when the app starts again is used as it is
/// - a reconnect keeps the access token it has while it lasts
/// a login spotify revoked all the same is signed in again on its own: the
/// login page opens in the background, and spotify, already approved and
/// signed in, answers at once. only if it does not is anyone asked.
class SpotifySession : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Session)
    QML_SINGLETON

    Q_PROPERTY(QString status READ status NOTIFY statusChanged FINAL)
    Q_PROPERTY(QString libraryStatus READ libraryStatus NOTIFY libraryStatusChanged FINAL)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(bool libraryConnected READ isLibraryConnected NOTIFY libraryConnectedChanged FINAL)
    Q_PROPERTY(bool libraryNeedsAuthorizationUpgrade READ libraryNeedsAuthorizationUpgrade NOTIFY libraryConnectedChanged FINAL)
    Q_PROPERTY(bool hasFullLibraryAccess READ hasFullLibraryAccess NOTIFY libraryConnectedChanged FINAL)
    Q_PROPERTY(bool hasLibraryToken READ hasLibraryToken NOTIFY libraryConnectedChanged FINAL)
    Q_PROPERTY(bool evaluatingStoredLogin READ isEvaluatingStoredLogin NOTIFY evaluatingStoredLoginChanged FINAL)
    Q_PROPERTY(QString libraryClientId READ libraryClientId WRITE setLibraryClientId NOTIFY libraryClientIdChanged FINAL)
    Q_PROPERTY(bool loginInProgress READ isLoginInProgress NOTIFY loginChanged FINAL)
    Q_PROPERTY(bool loginIsForLibrary READ isLoginForLibrary NOTIFY loginChanged FINAL)
    Q_PROPERTY(QUrl authorizationUrl READ authorizationUrl NOTIFY loginChanged FINAL)
    Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT FINAL)

public:
    static const QStringList kLibraryScopes;

    SpotifySession(PlaybackCore *core, CredentialStore *credentials, QNetworkAccessManager *network,
                   ErrorCenter *errors, QObject *parent = nullptr);
    ~SpotifySession() override;

    static SpotifySession *instance();
    static SpotifySession *create(QQmlEngine *, QJSEngine *);

    QString status() const { return m_status; }
    QString libraryStatus() const { return m_libraryStatus; }
    bool isConnected() const { return m_connected; }
    bool isLibraryConnected() const { return m_libraryConnected; }
    bool libraryNeedsAuthorizationUpgrade() const { return m_libraryNeedsUpgrade; }
    bool hasFullLibraryAccess() const { return m_libraryConnected && !m_libraryNeedsUpgrade; }
    bool hasLibraryToken() const { return !m_libraryAccessToken.isEmpty(); }
    bool hasLibraryRefreshToken() const { return !m_libraryRefreshToken.isEmpty(); }
    bool isEvaluatingStoredLogin() const { return m_evaluatingStoredLogin; }
    QString libraryClientId() const { return m_libraryClientId; }
    void setLibraryClientId(const QString &clientId);
    /// waiting on the browser: from opening the page until its callback
    /// arrives. a login signed in again on its own is not, while it may
    /// still finish by itself
    bool isLoginInProgress() const
    {
        return m_loginPurpose.has_value() && m_authorizationUrl.isValid() && !m_automaticLogin;
    }
    /// a revoked login is being signed in again in the background
    bool isRepairingLogin() const { return m_repairing.has_value(); }
    bool isLoginForLibrary() const { return m_loginPurpose == LoginPurpose::Library; }
    QUrl authorizationUrl() const { return m_authorizationUrl; }
    QString redirectUri() const { return OAuthFlow::redirectUri().toString(); }

    Q_INVOKABLE void beginPlaybackLogin();
    Q_INVOKABLE void beginLibraryLogin();
    /// opens the login page in the browser again, if the first one was closed
    Q_INVOKABLE void reopenLoginPage();
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void logOut();

    /// reads the saved logins and reconnects them. called once at launch.
    void restoreSavedSessions();
    /// waits, briefly, for logins still being written to the keychain: a
    /// refresh token spotify rotated and nobody kept is a login lost
    void flushCredentialSaves();
    /// how a login page is opened without taking the screen; tests catch it
    void setBackgroundOpener(std::function<void(const QUrl &)> opener) { m_openInBackground = std::move(opener); }

    /// the status line also carries playback state: Loading, Playing, Paused…
    void setStatus(const QString &status);
    void setLibraryStatus(const QString &status);

    // for the rest of the app ---------------------------------------------------

    WebApi &webApi() { return m_webApi; }
    PlaybackCore *core() const { return m_core; }
    /// bumped by a logout. work started before one must not touch what came after.
    quint64 generation() const { return m_generation; }
    void throwIfStale(quint64 generation) const
    {
        if (generation != m_generation) {
            throw Cancelled();
        }
    }

    /// runs `operation` with a current library token, refreshing it first when
    /// it is about to expire and once more if spotify answers 401.
    template <typename T>
    QCoro::Task<T> withLibraryAccessToken(std::function<QCoro::Task<T>(QString)> operation);

    /// reconnects the core when its session is stale, invalid or suspect, or
    /// when `force` asks: with librespot's own login when there is one, and
    /// otherwise the access token, refreshed only once it runs out.
    QCoro::Task<> ensureFreshPlaybackConnection(bool force = false);
    /// whatever just happened may have killed the librespot session; the next
    /// play reconnects first.
    void markPlaybackConnectionSuspect() { m_playbackConnectionIsSuspect = true; }

    /// what a library login or restore runs before it counts as finished:
    /// profile, playlists and home, owned by the library.
    void setInitialLibraryLoader(std::function<QCoro::Task<>()> loader) { m_loadInitialLibrary = std::move(loader); }

signals:
    void statusChanged();
    void libraryStatusChanged();
    void connectedChanged();
    void libraryConnectedChanged();
    void evaluatingStoredLoginChanged();
    void libraryClientIdChanged();
    void loginChanged();
    /// the core has a fresh session
    void playbackConnected();
    /// the core was replaced and every login forgotten. reset everything.
    void loggedOut();
    /// every login asked to be kept has been written
    void credentialsSaved();

private:
    enum class LoginPurpose { Playback, Library };

    QCoro::Task<> restore();
    QCoro::Task<> restorePlaybackSession(StoredCredentials credentials);
    QCoro::Task<> restoreLibrarySession(StoredCredentials credentials);
    QCoro::Task<> logOutAsync();
    void beginLogin(LoginPurpose purpose, const QStringList &scopes, const QString &clientId);
    QCoro::Task<> completeLogin(QUrl callbackUrl);
    QCoro::Task<> completePlaybackLogin(OAuthToken token);
    QCoro::Task<> completeLibraryLogin(OAuthToken token, QString clientId);
    /// connects the core with librespot's own login when one is given, the
    /// access token otherwise, and returns the login to keep
    QCoro::Task<QString> connectPlaybackCore(QString accessToken, QString storedLogin);
    /// the stored login first, then the access token, and a fresh one once
    QCoro::Task<> reconnectPlayback();
    /// the access token held while it lasts, or a fresh one; `fresh` insists
    QCoro::Task<QString> playbackAccessToken(bool fresh);
    QCoro::Task<QString> refreshLibraryAccessToken();
    void adoptPlaybackLogin(const QString &login);
    StoredCredentials storedCredentials(CredentialStore::Account account) const;
    /// keeps the login as it is now, in the background and in order
    void persist(CredentialStore::Account account);
    QCoro::Task<> drainSaves(CredentialStore::Account account);
    /// signs a revoked login in again, by itself if spotify lets it
    void repairLogin(LoginPurpose purpose);
    void finishRepair(bool ok);
    void repairTimedOut();
    /// what `error` says to the person, starting a repair if it is a revoked login
    SpotifyError repairIfRevoked(const SpotifyError &error, LoginPurpose purpose);
    static QCoro::Task<QString> awaitRefresh(std::shared_ptr<TokenRefresh> refresh);
    QString loginClientId(LoginPurpose purpose) const;

    void setConnected(bool connected);
    void setLibraryConnected(bool connected, bool needsUpgrade);
    void setEvaluatingStoredLogin(bool evaluating);
    void setLoginPurpose(std::optional<LoginPurpose> purpose, const QUrl &url = {});
    void reportError(const QString &message);

    PlaybackCore *m_core;
    CredentialStore *m_credentials;
    ErrorCenter *m_errors;
    OAuthFlow m_oauth;
    WebApi m_webApi;
    std::function<QCoro::Task<>()> m_loadInitialLibrary;

    quint64 m_generation = 0;
    QString m_status = QStringLiteral("Disconnected");
    QString m_libraryStatus = QStringLiteral("Not connected");
    bool m_connected = false;
    bool m_libraryConnected = false;
    bool m_libraryNeedsUpgrade = false;
    bool m_evaluatingStoredLogin = true;
    /// reading the saved logins at launch is still under way
    bool m_restoring = false;
    bool m_restoreStarted = false;
    bool m_playbackConnectionIsSuspect = false;
    std::optional<LoginPurpose> m_loginPurpose;
    QUrl m_authorizationUrl;

    QString m_refreshToken;
    /// librespot's reusable credentials, as the core hands them over
    QString m_playbackLogin;
    QString m_playbackAccessToken;
    QDateTime m_playbackAccessTokenExpiration;
    std::shared_ptr<TokenRefresh> m_playbackRefresh;

    QString m_libraryClientId;
    QString m_libraryAccessToken;
    QString m_libraryRefreshToken;
    QDateTime m_libraryAccessTokenExpiration;
    int m_libraryAuthorizationVersion = 0;
    std::shared_ptr<TokenRefresh> m_libraryRefresh;

    /// a login to write once the one being written is done, by account
    struct SaveQueue {
        std::optional<StoredCredentials> next;
        quint64 generation = 0;
        bool running = false;
    };
    SaveQueue m_saves[2];

    std::function<void(const QUrl &)> m_openInBackground;
    std::optional<LoginPurpose> m_repairing;
    QList<LoginPurpose> m_repairQueue;
    QDateTime m_lastRepair[2];
    /// the login under way was started by a repair, and has not yet asked
    /// anyone for anything
    bool m_automaticLogin = false;
    QTimer m_repairTimer;
};

template <typename T>
QCoro::Task<T> SpotifySession::withLibraryAccessToken(std::function<QCoro::Task<T>(QString)> operation)
{
    const quint64 generation = m_generation;
    if (m_libraryAccessToken.isEmpty()) {
        throw SpotifyError(SpotifyError::Kind::Session, QStringLiteral("Connect your Spotify library to continue."));
    }

    try {
        QString accessToken = m_libraryAccessToken;
        if (m_libraryAccessTokenExpiration.isValid()
            && m_libraryAccessTokenExpiration <= QDateTime::currentDateTimeUtc().addSecs(60)) {
            accessToken = co_await refreshLibraryAccessToken();
        }
        throwIfStale(generation);

        bool retry = false;
        if constexpr (std::is_void_v<T>) {
            try {
                co_await operation(accessToken);
            } catch (const SpotifyError &error) {
                if (!error.shouldRefreshAccessToken()) {
                    throw;
                }
                retry = true;
            }
            if (retry) {
                throwIfStale(generation);
                const QString retryToken = !m_libraryAccessToken.isEmpty() && m_libraryAccessToken != accessToken
                    ? m_libraryAccessToken
                    : co_await refreshLibraryAccessToken();
                co_await operation(retryToken);
            }
            throwIfStale(generation);
            co_return;
        } else {
            std::optional<T> value;
            try {
                value = co_await operation(accessToken);
            } catch (const SpotifyError &error) {
                if (!error.shouldRefreshAccessToken()) {
                    throw;
                }
                retry = true;
            }
            if (retry) {
                throwIfStale(generation);
                const QString retryToken = !m_libraryAccessToken.isEmpty() && m_libraryAccessToken != accessToken
                    ? m_libraryAccessToken
                    : co_await refreshLibraryAccessToken();
                value = co_await operation(retryToken);
            }
            throwIfStale(generation);
            co_return std::move(*value);
        }
    } catch (const SpotifyError &) {
        // a failure that lands after a logout belongs to nobody.
        throwIfStale(generation);
        throw;
    }
}

} // namespace swiftify
