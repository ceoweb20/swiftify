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

#include "system/dbus/Mpris.h"

#include "playback/PlaybackController.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QLoggingCategory>

namespace swiftify {

Q_LOGGING_CATEGORY(lcMpris, "swiftify.mpris")

namespace {

const QString kObjectPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
constexpr qint64 kConnectionPlayGraceMs = 3000;

QDBusObjectPath trackPath(const Song &song)
{
    return QDBusObjectPath(QStringLiteral("/dev/addenator/swiftify/track/") + song.spotifyId());
}

} // namespace

Mpris::Mpris(PlaybackController *player, QObject *parent)
    : QObject(parent)
    , m_player(player)
{
    new MprisRootAdaptor(this);
    new MprisPlayerAdaptor(this);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCInfo(lcMpris) << "no session bus; media keys will not reach swiftify";
        return;
    }
    if (!bus.registerObject(kObjectPath, this)) {
        qCWarning(lcMpris) << "could not register the media player object";
        return;
    }
    // a second copy of swiftify would clash on the plain name.
    m_registered = bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.swiftify"))
        || bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.swiftify.instance%1").arg(QCoreApplication::applicationPid()));

    connect(m_player, &PlaybackController::mediaStateChanged, this, &Mpris::update);
    connect(m_player, &PlaybackController::durationChanged, this, &Mpris::update);
    connect(m_player, &PlaybackController::currentSongChanged, this, &Mpris::update);
    connect(m_player, &PlaybackController::queueChanged, this, &Mpris::update);
    connect(m_player, &PlaybackController::volumeChanged, this, &Mpris::update);
    connect(m_player, &PlaybackController::shuffleChanged, this, &Mpris::update);
    connect(m_player, &PlaybackController::repeatModeChanged, this, &Mpris::update);
    connect(m_player, &PlaybackController::positionChanged, this, &Mpris::checkForJump);
    update();
}

Mpris::~Mpris()
{
    if (m_registered) {
        QDBusConnection::sessionBus().unregisterObject(kObjectPath);
    }
}

void Mpris::noteOutputArrived()
{
    m_outputArrivedAt = QDateTime::currentDateTimeUtc();
}

bool Mpris::startIsRefused() const
{
    return m_outputArrivedAt.isValid() && m_outputArrivedAt.msecsTo(QDateTime::currentDateTimeUtc()) < kConnectionPlayGraceMs;
}

bool Mpris::hasSong() const
{
    return m_player->hasSong();
}

bool Mpris::canSeek() const
{
    return m_player->hasSong() && (m_player->durationMs() > 0 || m_player->currentSong().durationMs > 0);
}

bool Mpris::canGoNext() const
{
    return m_player->canSkipForward();
}

bool Mpris::canGoPrevious() const
{
    return m_player->canSkipBackward();
}

QString Mpris::playbackStatus() const
{
    if (!m_player->hasSong()) {
        return QStringLiteral("Stopped");
    }
    return m_player->isPlaying() ? QStringLiteral("Playing") : QStringLiteral("Paused");
}

qlonglong Mpris::positionMicroseconds() const
{
    return qlonglong(m_player->livePositionMs()) * 1000;
}

QVariantMap Mpris::metadata() const
{
    if (!m_player->hasSong()) {
        return {{QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack")))}};
    }
    const Song song = m_player->currentSong();
    const qlonglong durationMs = std::max<qlonglong>(m_player->durationMs(), song.durationMs);
    QStringList artists;
    if (!song.artistItems.isEmpty()) {
        for (const auto &artist : song.artistItems) {
            artists.append(artist.name);
        }
    } else if (!song.artists.isEmpty()) {
        artists.append(song.artists);
    }
    QVariantMap map {
        {QStringLiteral("mpris:trackid"), QVariant::fromValue(trackPath(song))},
        {QStringLiteral("xesam:title"), song.name},
        {QStringLiteral("xesam:artist"), artists},
        {QStringLiteral("xesam:url"), QStringLiteral("https://open.spotify.com/track/") + song.spotifyId()},
    };
    if (!song.albumName.isEmpty()) {
        map.insert(QStringLiteral("xesam:album"), song.albumName);
    }
    if (durationMs > 0) {
        map.insert(QStringLiteral("mpris:length"), durationMs * 1000);
    }
    if (song.artworkUrl.isValid()) {
        map.insert(QStringLiteral("mpris:artUrl"), song.artworkUrl.toString());
    }
    return map;
}

void Mpris::update()
{
    if (!m_registered) {
        return;
    }
    QVariantMap changed;
    const auto publish = [&changed](const QString &name, auto &published, const auto &value) {
        if (published != value) {
            published = value;
            changed.insert(name, QVariant::fromValue(value));
        }
    };
    publish(QStringLiteral("PlaybackStatus"), m_publishedStatus, playbackStatus());
    publish(QStringLiteral("Metadata"), m_publishedMetadata, metadata());
    publish(QStringLiteral("CanGoNext"), m_publishedCanGoNext, canGoNext());
    publish(QStringLiteral("CanGoPrevious"), m_publishedCanGoPrevious, canGoPrevious());
    publish(QStringLiteral("CanSeek"), m_publishedCanSeek, canSeek());
    const bool songPresent = hasSong();
    if (m_publishedHasSong != songPresent) {
        m_publishedHasSong = songPresent;
        changed.insert(QStringLiteral("CanPlay"), songPresent);
        changed.insert(QStringLiteral("CanPause"), songPresent);
    }
    publish(QStringLiteral("Volume"), m_publishedVolume, m_player->volume());
    publish(QStringLiteral("Shuffle"), m_publishedShuffle, m_player->isShuffleEnabled());
    const QString loop = m_player->repeatMode() == PlaybackController::RepeatMode::One ? QStringLiteral("Track")
        : m_player->repeatMode() == PlaybackController::RepeatMode::All                ? QStringLiteral("Playlist")
                                                                                       : QStringLiteral("None");
    publish(QStringLiteral("LoopStatus"), m_publishedLoop, loop);
    if (!changed.isEmpty()) {
        emitPropertiesChanged(kPlayerInterface, changed);
    }

    // clients run the clock themselves from the playback state, so only a
    // position that drifted from where that clock says it is, a seek or a
    // stall, is news.
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qlonglong position = positionMicroseconds();
    const qlonglong elapsed = m_lastPositionAt.isValid() && m_player->isPlaying() ? m_lastPositionAt.msecsTo(now) * 1000 : 0;
    if (std::llabs(m_lastPositionUs + elapsed - position) >= 1'000'000 || changed.contains(QStringLiteral("Metadata"))) {
        if (auto *adaptor = findChild<MprisPlayerAdaptor *>()) {
            emit adaptor->Seeked(position);
        }
    }
    m_lastPositionUs = position;
    m_lastPositionAt = now;
}

void Mpris::checkForJump()
{
    if (!m_registered || !m_player->hasSong() || !m_lastPositionAt.isValid()) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qlonglong position = positionMicroseconds();
    const qlonglong elapsed = m_player->isPlaying() ? m_lastPositionAt.msecsTo(now) * 1000 : 0;
    if (std::llabs(m_lastPositionUs + elapsed - position) >= 1'000'000) {
        if (auto *adaptor = findChild<MprisPlayerAdaptor *>()) {
            emit adaptor->Seeked(position);
        }
        m_lastPositionUs = position;
        m_lastPositionAt = now;
    }
}

void Mpris::emitPropertiesChanged(const QString &interface, const QVariantMap &changed)
{
    QDBusMessage signal = QDBusMessage::createSignal(kObjectPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                     QStringLiteral("PropertiesChanged"));
    signal << interface << changed << QStringList {};
    QDBusConnection::sessionBus().send(signal);
}

MprisRootAdaptor::MprisRootAdaptor(Mpris *mpris)
    : QDBusAbstractAdaptor(mpris)
    , m_mpris(mpris)
{
}

void MprisRootAdaptor::Raise()
{
    emit m_mpris->raiseRequested();
}

void MprisRootAdaptor::Quit()
{
    emit m_mpris->quitRequested();
}

MprisPlayerAdaptor::MprisPlayerAdaptor(Mpris *mpris)
    : QDBusAbstractAdaptor(mpris)
    , m_mpris(mpris)
{
}

QString MprisPlayerAdaptor::loopStatus() const
{
    switch (m_mpris->player()->repeatMode()) {
    case PlaybackController::RepeatMode::One: return QStringLiteral("Track");
    case PlaybackController::RepeatMode::All: return QStringLiteral("Playlist");
    default: return QStringLiteral("None");
    }
}

void MprisPlayerAdaptor::setLoopStatus(const QString &status)
{
    const auto target = status == QLatin1StringView("Track") ? PlaybackController::RepeatMode::One
        : status == QLatin1StringView("Playlist")             ? PlaybackController::RepeatMode::All
                                                              : PlaybackController::RepeatMode::Off;
    for (int step = 0; step < 3 && m_mpris->player()->repeatMode() != target; ++step) {
        m_mpris->player()->cycleRepeatMode();
    }
}

bool MprisPlayerAdaptor::shuffle() const
{
    return m_mpris->player()->isShuffleEnabled();
}

void MprisPlayerAdaptor::setShuffle(bool shuffle)
{
    if (m_mpris->player()->isShuffleEnabled() != shuffle) {
        m_mpris->player()->toggleShuffle();
    }
}

double MprisPlayerAdaptor::volume() const
{
    return m_mpris->player()->volume();
}

void MprisPlayerAdaptor::setVolume(double volume)
{
    m_mpris->player()->setVolume(volume);
}

void MprisPlayerAdaptor::Next()
{
    m_mpris->player()->skipForward();
}

void MprisPlayerAdaptor::Previous()
{
    m_mpris->player()->skipBackward();
}

void MprisPlayerAdaptor::Pause()
{
    if (m_mpris->player()->isPlaying() || m_mpris->player()->isLoading()) {
        m_mpris->player()->pause();
    }
}

void MprisPlayerAdaptor::PlayPause()
{
    if (m_mpris->player()->isPlaying()) {
        m_mpris->player()->pause();
    } else if (!m_mpris->startIsRefused()) {
        m_mpris->player()->togglePlayback();
    }
}

void MprisPlayerAdaptor::Stop()
{
    if (m_mpris->player()->isPlaying() || m_mpris->player()->isLoading()) {
        m_mpris->player()->pause();
    }
}

void MprisPlayerAdaptor::Play()
{
    if (!m_mpris->startIsRefused() && !m_mpris->player()->isPlaying()) {
        m_mpris->player()->resume();
    }
}

void MprisPlayerAdaptor::Seek(qlonglong offset)
{
    m_mpris->player()->seekBy(int(offset / 1000));
}

void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath &trackId, qlonglong position)
{
    // a position for a track that has since changed is dropped, as the spec asks.
    if (!m_mpris->player()->hasSong() || trackId != trackPath(m_mpris->player()->currentSong())) {
        return;
    }
    m_mpris->player()->seek(int(position / 1000));
}

} // namespace swiftify
