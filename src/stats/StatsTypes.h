#pragma once

#include "spotify/SpotifyTypes.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <array>

namespace swiftify {

/// the stretch of listening the stats cover, as stats.fm names them
enum class StatsPeriod { Today, Weeks, Months, Lifetime };

/// one song heard for long enough to count, as the play history keeps it
struct PlayRecord {
    /// when it began playing
    QDateTime startedAt;
    /// how long it was actually heard, pauses left out
    qint64 listenedMs = 0;
    Song song;
};

/// a place in a top list, or a song in the recent ones: a song, an artist, an
/// album or a genre, and how much of it was heard
struct StatEntry {
    QString key;
    QString name;
    QString subtitle;
    QUrl artworkUrl;
    /// -1 when the source does not say: stats.fm leaves the counts of its top
    /// lists out for accounts without plus, until they are asked for one by one
    int plays = 0;
    qint64 listenedMs = 0;
    /// stats.fm's own id for it (and a song's album), to ask for its counts
    qint64 statsFmId = 0;
    qint64 statsFmAlbumId = 0;
    /// an artist's genres, where the source gives them
    QStringList genres;
    /// when it played, for the recent ones
    QDateTime playedAt;
    Song song;
    Artist artist;
    Album album;
};

/// all there is to show for a period, from either source
struct StatsSnapshot {
    int plays = 0;
    qint64 listenedMs = 0;
    int uniqueTracks = 0;
    int uniqueArtists = 0;
    int uniqueAlbums = 0;

    QList<StatEntry> topTracks;
    QList<StatEntry> topArtists;
    QList<StatEntry> topAlbums;
    QList<StatEntry> topGenres;
    QList<StatEntry> recent;

    /// plays by hour of the day, local time
    std::array<int, 24> hourly {};
    /// plays by day of the week, monday first
    std::array<int, 7> weekdays {};
    /// plays on each day, every period's own: the calendar shows a year
    QMap<QDate, int> daily;

    /// days in a row with something played, up to today (or yesterday, when
    /// nothing has played yet today)
    int streakDays = 0;
    /// days with anything played, in the period
    int activeDays = 0;
};

/// the first moment `period` covers, before `now`; invalid for all of it
QDateTime periodStart(StatsPeriod period, const QDateTime &now);

/// the days in a row ending today, or yesterday, that have plays
int streakEndingAt(const QMap<QDate, int> &daily, const QDate &today);

} // namespace swiftify
