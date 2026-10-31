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

#include "stats/StatsTypes.h"

#include <QHash>
#include <QStringList>

namespace swiftify {

/// stats worked out from the plays kept on this computer, the way stats.fm
/// works them out from its own
class StatsAggregator {
public:
    /// how many places each top list keeps
    static constexpr int kTopLimit = 50;
    /// how many of the latest plays the recent list shows
    static constexpr int kRecentLimit = 20;

    /// `records` oldest first. `genres` holds what spotify says each artist
    /// plays, by artist id; an artist it has nothing for adds no genre.
    static StatsSnapshot aggregate(const QList<PlayRecord> &records, StatsPeriod period, const QDateTime &now,
                                   const QHash<QString, QStringList> &genres = {});
};

} // namespace swiftify
