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

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QUrl>

#include <memory>

namespace swiftify {

class ArtworkCache;
class PlaybackController;

/// the player in macos's now playing (control centre, the menu bar's now
/// playing item, the lock screen) and on the media keys and headphone
/// buttons, through the MediaPlayer framework. the job
/// SystemMediaSession.swift does in the swift app and Mpris does on linux.
class NowPlaying : public QObject {
    Q_OBJECT

public:
    NowPlaying(PlaybackController *player, ArtworkCache *artwork, QObject *parent = nullptr);
    ~NowPlaying() override;

private:
    struct Native;

    void update();
    /// a seek moves the position by more than playback would have
    void checkForJump();
    void loadArtwork(const QUrl &url);

    PlaybackController *m_player;
    ArtworkCache *m_artwork;
    std::unique_ptr<Native> m_native;
    QUrl m_artworkUrl;
    // where the position stood at the last update, and since when, so a seek
    // can be told from the clock running on
    qint64 m_publishedPositionMs = 0;
    QElapsedTimer m_publishedAt;
    bool m_publishedPlaying = false;
};

} // namespace swiftify
