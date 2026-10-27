#include "playback/DjMix.h"

#include "core/PlaybackCore.h"
#include "library/LibraryStore.h"
#include "playback/PlaybackController.h"
#include "spotify/SpotifyJson.h"
#include "spotify/SpotifySession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlEngine>

#include <algorithm>
#include <optional>

namespace swiftify {
namespace {
DjMix *s_instance = nullptr;

QString artistKey(const Song &song)
{
    const auto artist = song.primaryArtist();
    return artist.id.isEmpty() ? song.artists.section(u',', 0, 0).trimmed().toCaseFolded() : artist.id;
}
}

QList<Song> dj::queue(QList<Song> recommendations, QList<Song> familiar, int limit,
                     const QSet<QString> &excluded, QRandomGenerator &random)
{
    std::shuffle(recommendations.begin(), recommendations.end(), random);
    std::shuffle(familiar.begin(), familiar.end(), random);
    QSet<QString> used = excluded;
    QString previousArtist;
    QList<Song> result;
    const auto take = [&](QList<Song> &pool, bool differentArtist) -> std::optional<Song> {
        for (qsizetype i = 0; i < pool.size(); ++i) {
            const auto &song = pool.at(i);
            if (!song.isPlayable || !song.uri.startsWith(QLatin1StringView("spotify:track:"))
                || song.uri.size() <= 14 || used.contains(song.uri)) continue;
            const auto artist = artistKey(song);
            if (differentArtist && !artist.isEmpty() && artist == previousArtist) continue;
            Song chosen = pool.takeAt(i).freshInstance();
            used.insert(chosen.uri);
            return chosen;
        }
        return {};
    };
    while (result.size() < std::clamp(limit, 0, 100)) {
        // Prefer Spotify's personalized mixes for three out of every five slots.
        const bool recommended = result.size() % 5 != 2 && result.size() % 5 != 4;
        auto &first = recommended ? recommendations : familiar;
        auto &second = recommended ? familiar : recommendations;
        auto chosen = take(first, true);
        if (!chosen) chosen = take(second, true);
        if (!chosen) chosen = take(first, false);
        if (!chosen) chosen = take(second, false);
        if (!chosen) break;
        previousArtist = artistKey(*chosen);
        result.append(*chosen);
    }
    return result;
}

DjMix::DjMix(SpotifySession *session, LibraryStore *library, PlaybackController *player, QObject *parent)
    : QObject(parent), m_session(session), m_library(library), m_player(player)
{
    s_instance = this;
    connect(session, &SpotifySession::loggedOut, this, [this] {
        cancel();
        m_active = false;
        m_instances.clear();
        m_status.clear();
        emit changed();
    });
    connect(session, &SpotifySession::loginChanged, this, [this] {
        cancel();
        m_active = false;
        m_instances.clear();
        emit changed();
    });
    connect(player, &PlaybackController::currentSongChanged, this, [this] {
        // A manual track choice wins over a mix still being assembled.
        if (m_busy) cancel();
        if (m_active && (!m_player->hasSong() || !m_instances.contains(m_player->currentSong().instanceId))) {
            m_active = false;
            emit changed();
        }
    });
    connect(player, &PlaybackController::queueChanged, this, [this] {
        if (m_busy) cancel();
    });
}

DjMix::~DjMix()
{
    m_request.cancel();
    if (s_instance == this) s_instance = nullptr;
}

DjMix *DjMix::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void DjMix::start()
{
    if (m_busy) return;
    if (!m_session || !m_library || !m_player || !m_session->isConnected() || !m_session->hasLibraryToken()) {
        m_status = tr("Connect Spotify playback and your library before starting DJ Mix.");
        emit changed();
        return;
    }
    m_busy = true;
    m_status = tr("Building your DJ Mix…");
    const auto token = m_request.renew();
    emit changed();
    if (!isCancelled(token)) detach(collect(token));
}

void DjMix::cancel()
{
    m_request.cancel();
    if (!m_busy) return;
    m_busy = false;
    m_status = tr("DJ Mix cancelled.");
    emit changed();
}

QCoro::Task<> DjMix::collect(CancelToken token)
{
    const QPointer<DjMix> self(this);
    const QPointer<SpotifySession> session(m_session);
    const quint64 generation = session->generation();
    const auto check = [self, session, token, generation] {
        throwIfCancelled(token);
        if (!self || !session || !self->m_library || !self->m_player) throw Cancelled();
        session->throwIfStale(generation);
    };
    try {
        QList<Song> recommendations;
        QList<Song> familiar = m_library->recentlyPlayed()->songs().mid(0, 50);
        if (familiar.isEmpty()) {
            try {
                familiar = co_await session->withLibraryAccessToken<QList<Song>>([session](QString access) {
                    if (!session) throw Cancelled();
                    return session->webApi().recentlyPlayed(access);
                });
            } catch (const SpotifyError &error) {
                if (error.kind() == SpotifyError::Kind::RateLimited) throw;
            }
            check();
        }
        QList<Song> saved = m_library->savedSongs()->songs().mid(0, 50);
        if (saved.isEmpty()) {
            try {
                saved = co_await session->withLibraryAccessToken<QList<Song>>([session](QString access) {
                    if (!session) throw Cancelled();
                    return session->webApi().savedSongSample(access);
                });
            } catch (const SpotifyError &error) {
                if (error.kind() == SpotifyError::Kind::RateLimited) throw;
            }
            check();
        }
        familiar.append(saved);

        // Never mix personalized feeds from a different playback account.
        auto playlists = !m_library->profileId().isEmpty() && m_library->personalizedAccountId() == m_library->profileId()
                         ? m_library->personalizedPlaylists()->items() : QList<Playlist>{};
        std::shuffle(playlists.begin(), playlists.end(), *QRandomGenerator::global());
        for (const auto &playlist : playlists.mid(0, 3)) {
            if (recommendations.size() >= 150) break;
            try {
                const QString payload = co_await session->core()->playlistJson(playlist.uri);
                check();
                const auto items = QJsonDocument::fromJson(payload.toUtf8()).object().value(QStringLiteral("items")).toArray();
                for (const auto &entry : items) {
                    if (const auto song = json::wrappedTrack(entry.toObject())) recommendations.append(*song);
                    if (recommendations.size() >= 150) break;
                }
            } catch (const SpotifyError &error) {
                if (error.kind() == SpotifyError::Kind::RateLimited) throw;
            }
            check();
        }

        // Artist top tracks supply related material when mixes are absent.
        QSet<QString> artists;
        for (const auto &song : familiar) {
            const auto artist = song.primaryArtist();
            if (artist.id.isEmpty() || artists.contains(artist.id)) continue;
            artists.insert(artist.id);
            try {
                const QString uri = artist.uri.isEmpty() ? QStringLiteral("spotify:artist:") + artist.id : artist.uri;
                const QString payload = co_await session->core()->artistTopTracksJson(uri);
                check();
                const auto tracks = QJsonDocument::fromJson(payload.toUtf8()).object().value(QStringLiteral("tracks")).toArray();
                for (const auto &track : tracks) {
                    if (const auto item = json::track(track.toObject())) recommendations.append(*item);
                }
            } catch (const SpotifyError &error) {
                if (error.kind() == SpotifyError::Kind::RateLimited) throw;
            }
            check();
            if (artists.size() >= 2) break;
        }
        check();
        QSet<QString> excluded;
        if (m_player->hasSong()) excluded.insert(m_player->currentSong().uri);
        const auto mix = dj::queue(recommendations, familiar, 40, excluded, *QRandomGenerator::global());
        if (mix.isEmpty()) {
            m_status = tr("No tracks for a mix yet. Save some songs or listen in Spotify, then try again.");
        } else {
            m_instances.clear();
            for (const auto &song : mix) m_instances.insert(song.instanceId);
            m_active = true;
            m_status = tr("DJ Mix ready: %1 tracks.").arg(mix.size());
        }
        m_request.finish(token);
        m_busy = false;
        emit changed();
        if (!mix.isEmpty()) {
            // Preserve the diversified order instead of reshuffling it again.
            if (m_player->isShuffleEnabled()) m_player->toggleShuffle();
            m_player->play(mix.first(), mix);
        }
    } catch (const Cancelled &) {
        co_return;
    } catch (const SpotifyError &error) {
        if (!self || !session || isCancelled(token) || generation != session->generation()) co_return;
        m_request.finish(token);
        m_busy = false;
        m_status = error.message();
        emit changed();
    }
}

} // namespace swiftify
