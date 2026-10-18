#include "library/PlaylistCollections.h"
#include "app/Settings.h"
#include "library/LibraryStore.h"
#include "spotify/PlaylistLinks.h"
#include "spotify/SpotifySession.h"

#include <QCryptographicHash>
#include <QQmlEngine>
#include <QSet>
#include <algorithm>

namespace swiftify {
namespace {
PlaylistCollections *s_instance = nullptr;
constexpr int maximumLinks = 30;
constexpr int maximumRecent = 20;
const QString unavailable = QStringLiteral("Spotify does not expose this playlist to this application.");
}

PlaylistCollections::PlaylistCollections(SpotifySession *session, Settings *settings, LibraryStore *library, QObject *parent)
    : QObject(parent), m_session(session), m_settings(settings), m_library(library)
{
    if (!s_instance) s_instance = this;
    connect(session, &SpotifySession::loggedOut, this, &PlaylistCollections::reset);
    // A new OAuth login can replace the library account without a logout.
    connect(session, &SpotifySession::loginChanged, this, &PlaylistCollections::reset);
    connect(session, &SpotifySession::libraryConnectedChanged, this, [this] {
        if (!m_session->isLibraryConnected()) reset();
    });
}

PlaylistCollections::~PlaylistCollections()
{
    m_request.cancel();
    if (s_instance == this) s_instance = nullptr;
}

PlaylistCollections *PlaylistCollections::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QVariantList PlaylistCollections::entries() const
{
    QVariantList result;
    for (const auto &entry : m_entries) result.append(QVariantMap {
        {QStringLiteral("id"), entry.playlist.id}, {QStringLiteral("playlist"), QVariant::fromValue(entry.playlist)},
        {QStringLiteral("origin"), entry.origin}, {QStringLiteral("status"), entry.error},
        {QStringLiteral("available"), entry.available}, {QStringLiteral("linked"), m_links.contains(entry.playlist.id)}});
    return result;
}

void PlaylistCollections::reset()
{
    m_request.cancel();
    m_busy = false;
    m_entries.clear(); m_saved.clear(); m_recent.clear(); m_links.clear();
    m_cache.clear(); m_errors.clear(); m_userId.clear(); m_status.clear();
    emit changed();
}

QString PlaylistCollections::preferenceKey() const
{
    return QStringLiteral("playlistLinks/") + QString::fromLatin1(
        QCryptographicHash::hash(m_userId.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void PlaylistCollections::identify(const QString &userId)
{
    if (userId == m_userId) return;
    m_entries.clear(); m_saved.clear(); m_recent.clear(); m_cache.clear(); m_errors.clear(); m_links.clear();
    m_userId = userId;
    QSet<QString> seen;
    for (const auto &id : m_settings->value(preferenceKey()).toStringList()) {
        if (playlistlinks::validId(id) && !seen.contains(id) && m_links.size() < maximumLinks) {
            m_links.append(id); seen.insert(id);
        }
    }
    emit changed();
}

void PlaylistCollections::saveLinks()
{
    if (m_userId.isEmpty()) return;
    // Store only canonical IDs, not share URLs/query parameters or account tokens.
    m_settings->setValue(preferenceKey(), m_links);
    m_settings->sync();
}

bool PlaylistCollections::begin()
{
    if (m_busy) return false;
    if (m_session->isLoginInProgress()) {
        m_status = QStringLiteral("Finish signing in before loading collections.");
        emit changed(); return false;
    }
    if (!m_session->hasLibraryToken()) {
        m_status = QStringLiteral("Connect your Spotify library to load collections.");
        emit changed(); return false;
    }
    if (m_blockedUntil > QDateTime::currentDateTimeUtc()) {
        m_status = QStringLiteral("Spotify rate limit reached. Try again later.");
        emit changed(); return false;
    }
    m_busy = true; m_status.clear(); emit changed(); return true;
}

void PlaylistCollections::refresh()
{
    if (begin()) detach(run(false, {}, m_request.renew()));
}

void PlaylistCollections::add(const QString &reference)
{
    const auto id = playlistlinks::id(reference);
    if (!id) {
        m_status = QStringLiteral("Use a full open.spotify.com playlist link or spotify:playlist URI.");
        emit changed(); return;
    }
    if (begin()) detach(run(true, *id, m_request.renew()));
}

void PlaylistCollections::check(const CancelToken &token, quint64 generation) const
{
    throwIfCancelled(token);
    m_session->throwIfStale(generation);
}

bool PlaylistCollections::report(const SpotifyError &error)
{
    if (error.kind() == SpotifyError::Kind::RateLimited) {
        m_blockedUntil = QDateTime::currentDateTimeUtc().addSecs(std::max(1, error.retryAfterSeconds() < 0 ? 30 : error.retryAfterSeconds()));
        m_status = QStringLiteral("Spotify rate limit reached. Try again later.");
        return true;
    }
    if (error.kind() == SpotifyError::Kind::Unauthorized || error.kind() == SpotifyError::Kind::LoginRevoked
        || error.kind() == SpotifyError::Kind::Session) {
        m_status = QStringLiteral("Reconnect your Spotify library to continue."); return true;
    }
    if (error.kind() == SpotifyError::Kind::Forbidden || error.httpStatus() == 404) {
        m_status = unavailable; return false;
    }
    m_status = QStringLiteral("Could not refresh collections. Previously loaded entries are kept.");
    return true;
}

QCoro::Task<Playlist> PlaylistCollections::metadata(QString id, CancelToken token)
{
    const auto cached = m_cache.constFind(id);
    if (cached != m_cache.cend() && cached->expires > QDateTime::currentDateTimeUtc()) co_return cached->playlist;
    auto *session = m_session;
    const Playlist playlist = co_await session->withLibraryAccessToken<Playlist>(
        [session, id, token](QString accessToken) {
            throwIfCancelled(token);
            return session->webApi().playlist(id, accessToken);
        });
    throwIfCancelled(token);
    if (m_cache.size() >= 100) m_cache.clear();
    m_cache.insert(id, {playlist, QDateTime::currentDateTimeUtc().addSecs(600)});
    co_return playlist;
}

void PlaylistCollections::rebuild()
{
    QList<Entry> entries;
    QSet<QString> seen;
    for (const auto &playlist : m_saved) {
        if (!playlistlinks::validId(playlist.id) || seen.contains(playlist.id)) continue;
        const bool available = !playlist.name.trimmed().isEmpty();
        seen.insert(playlist.id); entries.append({playlist, QStringLiteral("saved"), available ? QString() : unavailable, available});
    }
    const auto append = [&](const QStringList &ids, const QString &origin) {
        for (const auto &id : ids) {
            if (seen.contains(id)) continue;
            seen.insert(id);
            const auto cached = m_cache.constFind(id);
            Playlist playlist;
            playlist.id = id; playlist.uri = QStringLiteral("spotify:playlist:") + id;
            if (cached != m_cache.cend()) playlist = cached->playlist;
            const QString error = m_errors.value(id);
            const bool available = cached != m_cache.cend() && error.isEmpty();
            // A linked/recent card never grants editing rights by itself.
            playlist.ownerId.clear(); playlist.isCollaborative = false;
            entries.append({playlist, origin, available ? QString() : error.isEmpty() ? unavailable : error, available});
        }
    };
    append(m_links, QStringLiteral("link"));
    append(m_recent, QStringLiteral("recent"));
    m_entries = entries;
}

QCoro::Task<> PlaylistCollections::run(bool adding, QString id, CancelToken token)
{
    const quint64 generation = m_session->generation();
    auto *session = m_session;
    bool identified = false;
    try {
        const Profile profile = co_await session->withLibraryAccessToken<Profile>([session, token](QString accessToken) {
            throwIfCancelled(token); return session->webApi().profile(accessToken);
        });
        check(token, generation); identify(profile.id); identified = true;
        if (adding) {
            if (m_links.contains(id)) m_status = QStringLiteral("This playlist link is already added.");
            else if (m_links.size() >= maximumLinks) m_status = QStringLiteral("You can keep up to 30 playlist links.");
            else {
                co_await metadata(id, token);
                check(token, generation);
                m_links.append(id); m_errors.remove(id); saveLinks(); rebuild();
                m_status = QStringLiteral("Playlist link added. Your Spotify library was not changed.");
            }
        } else {
            bool requestsAllowed = true;
            try {
                const auto saved = co_await session->withLibraryAccessToken<QList<Playlist>>([session, token](QString accessToken) {
                    throwIfCancelled(token); return session->webApi().playlists(accessToken);
                });
                check(token, generation); m_saved = saved;
            } catch (const SpotifyError &error) {
                throwIfCancelled(token); requestsAllowed = !report(error);
            }
            if (requestsAllowed) {
                try {
                    const auto history = co_await session->withLibraryAccessToken<RecentPlayback>([session, token](QString accessToken) {
                        throwIfCancelled(token); return session->webApi().recentHistory(accessToken, 50);
                    });
                    check(token, generation); m_recent = history.playlistIds.mid(0, maximumRecent);
                } catch (const SpotifyError &error) {
                    throwIfCancelled(token); requestsAllowed = !report(error);
                }
            }
            m_errors.clear();
            QSet<QString> known;
            for (const auto &playlist : m_saved) known.insert(playlist.id);
            for (const auto &key : m_links + m_recent) {
                if (known.contains(key)) continue;
                known.insert(key);
                if (!requestsAllowed) { m_errors.insert(key, QStringLiteral("Playlist details were not refreshed. Try again later.")); continue; }
                try {
                    co_await metadata(key, token);
                    check(token, generation);
                } catch (const SpotifyError &error) {
                    throwIfCancelled(token);
                    requestsAllowed = !report(error);
                    m_errors.insert(key, error.kind() == SpotifyError::Kind::Forbidden || error.httpStatus() == 404 ? unavailable : m_status);
                }
            }
            check(token, generation); rebuild();
        }
    } catch (const Cancelled &) {
        co_return;
    } catch (const SpotifyError &error) {
        if (isCancelled(token) || generation != m_session->generation()) co_return;
        report(error);
        if (!identified) {
            // Don't expose a previous account's cached board if identity could
            // not be verified. Persisted links remain in their account scope.
            m_entries.clear(); m_saved.clear(); m_recent.clear(); m_links.clear();
            m_cache.clear(); m_errors.clear(); m_userId.clear();
            if (error.kind() != SpotifyError::Kind::RateLimited) {
                m_status = QStringLiteral("Could not verify the Spotify account. Reconnect and refresh collections.");
            }
        }
    }
    m_busy = false; m_request.finish(token); emit changed();
}

void PlaylistCollections::removeLink(const QString &id)
{
    if (m_busy || !m_links.removeAll(id)) return;
    saveLinks(); rebuild(); m_status = QStringLiteral("Playlist link removed. Your Spotify library was not changed."); emit changed();
}

void PlaylistCollections::open(const QString &id)
{
    if (m_busy || !m_session->hasLibraryToken()) return;
    for (const auto &entry : m_entries) {
        if (entry.playlist.id == id && entry.available) { m_library->selectPlaylist(entry.playlist); return; }
    }
}
} // namespace swiftify
