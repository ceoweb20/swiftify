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

#include "spotify/SpotifyTypes.h"
#include <QByteArray>
#include <optional>

namespace swiftify::playbackstate {

constexpr int maximumQueue = 2000;

struct Snapshot {
    QList<Song> queue;
    QList<Song> shuffled;
    QList<Song> history;
    int currentIndex = -1;
    quint32 positionMs = 0;
    quint32 durationMs = 0;
    bool shuffle = false;
    int repeat = 0;
};

// The index identifies an occurrence, not a URI: duplicates are legitimate.
QByteArray encode(const QList<Song> &queue, const QList<Song> &shuffled,
                  const QList<Song> &history, const Song &current,
                  quint32 positionMs, quint32 durationMs, bool shuffle, int repeat);
std::optional<Snapshot> decode(const QByteArray &bytes);

} // namespace swiftify::playbackstate
