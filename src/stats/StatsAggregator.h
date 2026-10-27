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
