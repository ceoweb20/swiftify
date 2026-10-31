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

#include "system/macos/NowPlaying.h"

#include "artwork/ArtworkCache.h"
#include "playback/PlaybackController.h"

#include <QImage>
#include <QLoggingCategory>
#include <QPointer>

#import <AppKit/AppKit.h>
#import <MediaPlayer/MediaPlayer.h>

Q_LOGGING_CATEGORY(lcNowPlaying, "swiftify.nowplaying")

namespace swiftify {

namespace {

/// how far a position may stray from the clock before it counts as a seek
constexpr qint64 kJumpToleranceMs = 1500;
constexpr int kArtworkPixels = 600;

} // namespace

// compiled with arc, so these hold their objects strongly
struct NowPlaying::Native {
    NSMutableArray<MPRemoteCommand *> *commands = [NSMutableArray array];
    NSMutableArray *targets = [NSMutableArray array];
    MPMediaItemArtwork *artwork = nil;
};

NowPlaying::NowPlaying(PlaybackController *player, ArtworkCache *artwork, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_artwork(artwork)
    , m_native(std::make_unique<Native>())
{
    MPRemoteCommandCenter *center = [MPRemoteCommandCenter sharedCommandCenter];
    QPointer<PlaybackController> guarded(player);
    Native *native = m_native.get();

    // the system calls these on the main thread, where the player lives
    const auto add = [native](MPRemoteCommand *command, MPRemoteCommandHandlerStatus (^handler)(MPRemoteCommandEvent *)) {
        command.enabled = YES;
        [native->commands addObject:command];
        [native->targets addObject:[command addTargetWithHandler:handler]];
    };
    const auto withSong = [guarded](void (^action)(PlaybackController *)) -> MPRemoteCommandHandlerStatus {
        if (!guarded) {
            return MPRemoteCommandHandlerStatusCommandFailed;
        }
        if (!guarded->hasSong()) {
            return MPRemoteCommandHandlerStatusNoActionableNowPlayingItem;
        }
        action(guarded.data());
        return MPRemoteCommandHandlerStatusSuccess;
    };

    add(center.playCommand, ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        return withSong(^(PlaybackController *p) { p->resume(); });
    });
    add(center.pauseCommand, ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        return withSong(^(PlaybackController *p) { p->pause(); });
    });
    add(center.togglePlayPauseCommand, ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        return withSong(^(PlaybackController *p) { p->togglePlayback(); });
    });
    add(center.nextTrackCommand, ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        return withSong(^(PlaybackController *p) { p->skipForward(); });
    });
    add(center.previousTrackCommand, ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *) {
        return withSong(^(PlaybackController *p) { p->skipBackward(); });
    });
    add(center.changePlaybackPositionCommand, ^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent *event) {
        const NSTimeInterval seconds = ((MPChangePlaybackPositionCommandEvent *)event).positionTime;
        return withSong(^(PlaybackController *p) { p->seek(int(seconds * 1000)); });
    });

    connect(m_player, &PlaybackController::mediaStateChanged, this, &NowPlaying::update);
    connect(m_player, &PlaybackController::durationChanged, this, &NowPlaying::update);
    connect(m_player, &PlaybackController::positionChanged, this, &NowPlaying::checkForJump);
    update();
}

NowPlaying::~NowPlaying()
{
    for (NSUInteger index = 0; index < m_native->commands.count; ++index) {
        [m_native->commands[index] removeTarget:m_native->targets[index]];
    }
    MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
    center.nowPlayingInfo = nil;
    center.playbackState = MPNowPlayingPlaybackStateStopped;
}

void NowPlaying::checkForJump()
{
    if (!m_player->hasSong() || !m_publishedAt.isValid()) {
        return;
    }
    const qint64 expected = m_publishedPositionMs + (m_publishedPlaying ? m_publishedAt.elapsed() : 0);
    if (std::abs(qint64(m_player->livePositionMs()) - expected) > kJumpToleranceMs) {
        update();
    }
}

void NowPlaying::update()
{
    MPNowPlayingInfoCenter *center = [MPNowPlayingInfoCenter defaultCenter];
    if (!m_player->hasSong()) {
        center.nowPlayingInfo = nil;
        center.playbackState = MPNowPlayingPlaybackStateStopped;
        m_publishedAt.invalidate();
        return;
    }

    const Song song = m_player->currentSong();
    if (song.artworkUrl != m_artworkUrl) {
        m_native->artwork = nil;
        loadArtwork(song.artworkUrl);
    }

    const qint64 durationMs = std::max<qint64>(m_player->durationMs(), song.durationMs);
    m_publishedPositionMs = m_player->livePositionMs();
    m_publishedPlaying = m_player->isPlaying();
    m_publishedAt.start();

    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    info[MPMediaItemPropertyTitle] = song.name.toNSString();
    info[MPMediaItemPropertyArtist] = song.artists.toNSString();
    if (!song.albumName.isEmpty()) {
        info[MPMediaItemPropertyAlbumTitle] = song.albumName.toNSString();
    }
    if (durationMs > 0) {
        info[MPMediaItemPropertyPlaybackDuration] = @(double(durationMs) / 1000);
    }
    info[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(double(m_publishedPositionMs) / 1000);
    info[MPNowPlayingInfoPropertyPlaybackRate] = @(m_publishedPlaying ? 1.0 : 0.0);
    info[MPNowPlayingInfoPropertyMediaType] = @(MPNowPlayingInfoMediaTypeAudio);
    if (m_native->artwork) {
        info[MPMediaItemPropertyArtwork] = m_native->artwork;
    }
    center.nowPlayingInfo = info;
    center.playbackState = m_publishedPlaying ? MPNowPlayingPlaybackStatePlaying : MPNowPlayingPlaybackStatePaused;
}

void NowPlaying::loadArtwork(const QUrl &url)
{
    m_artworkUrl = url;
    if (!url.isValid()) {
        return;
    }
    QPointer<NowPlaying> self(this);
    m_artwork->image(url, kArtworkPixels, [self, url](const QImage &image) {
        if (!self || self->m_artworkUrl != url || image.isNull()) {
            return;
        }
        CGImageRef cgImage = image.toCGImage();
        if (!cgImage) {
            return;
        }
        NSImage *cover = [[NSImage alloc] initWithCGImage:cgImage size:NSMakeSize(image.width(), image.height())];
        CGImageRelease(cgImage);
        self->m_native->artwork = [[MPMediaItemArtwork alloc] initWithBoundsSize:cover.size
                                                                   requestHandler:^NSImage *(CGSize) { return cover; }];
        self->update();
    });
}

} // namespace swiftify
