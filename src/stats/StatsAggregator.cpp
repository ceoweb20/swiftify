#include "stats/StatsAggregator.h"

#include <QSet>

#include <algorithm>
#include <functional>

namespace swiftify {

QDateTime periodStart(StatsPeriod period, const QDateTime &now)
{
    const QDateTime local = now.toLocalTime();
    switch (period) {
    case StatsPeriod::Today: return QDateTime(local.date(), QTime(0, 0));
    case StatsPeriod::Weeks: return local.addDays(-28);
    case StatsPeriod::Months: return local.addMonths(-6);
    case StatsPeriod::Lifetime: return {};
    }
    return {};
}

int streakEndingAt(const QMap<QDate, int> &daily, const QDate &today)
{
    const auto played = [&](const QDate &day) { return daily.value(day) > 0; };
    // a day not over yet does not break a streak it has not added to
    QDate day = played(today) ? today : today.addDays(-1);
    int streak = 0;
    while (played(day)) {
        ++streak;
        day = day.addDays(-1);
    }
    return streak;
}

namespace {

QList<StatEntry> ranked(QHash<QString, StatEntry> tallies)
{
    QList<StatEntry> list = tallies.values();
    // most played first; time heard breaks a tie, then the name, so the
    // order is the same every time
    std::sort(list.begin(), list.end(), [](const StatEntry &a, const StatEntry &b) {
        if (a.plays != b.plays) return a.plays > b.plays;
        if (a.listenedMs != b.listenedMs) return a.listenedMs > b.listenedMs;
        return a.name.localeAwareCompare(b.name) < 0;
    });
    if (list.size() > StatsAggregator::kTopLimit) {
        list.resize(StatsAggregator::kTopLimit);
    }
    return list;
}

void count(QHash<QString, StatEntry> &tallies, const QString &key, const PlayRecord &record,
           const std::function<void(StatEntry &)> &describe)
{
    if (key.isEmpty()) {
        return;
    }
    auto found = tallies.find(key);
    if (found == tallies.end()) {
        StatEntry entry;
        entry.key = key;
        describe(entry);
        found = tallies.insert(key, entry);
    }
    found->plays += 1;
    found->listenedMs += record.listenedMs;
}

/// the artists a song names, with an id where spotify gave one, or the
/// names it shows when it gave none (a music file of the person's own)
QList<Artist> artistsOf(const Song &song)
{
    if (!song.artistItems.isEmpty()) {
        return song.artistItems;
    }
    QList<Artist> artists;
    for (const QString &name : song.artists.split(QStringLiteral(", "), Qt::SkipEmptyParts)) {
        Artist artist;
        artist.name = name.trimmed();
        artists.append(artist);
    }
    return artists;
}

} // namespace

StatsSnapshot StatsAggregator::aggregate(const QList<PlayRecord> &records, StatsPeriod period, const QDateTime &now,
                                         const QHash<QString, QStringList> &genres)
{
    StatsSnapshot snapshot;
    const QDateTime start = periodStart(period, now);

    QHash<QString, StatEntry> tracks;
    QHash<QString, StatEntry> artists;
    QHash<QString, StatEntry> albums;
    QHash<QString, StatEntry> genreTallies;
    QSet<QDate> activeDays;

    for (const PlayRecord &record : records) {
        const QDateTime local = record.startedAt.toLocalTime();
        // every play shows on the calendar and in the streak, whatever the
        // period: they look back over the year
        snapshot.daily[local.date()] += 1;
        if (start.isValid() && local < start) {
            continue;
        }

        const Song &song = record.song;
        snapshot.plays += 1;
        snapshot.listenedMs += record.listenedMs;
        snapshot.hourly[std::clamp(local.time().hour(), 0, 23)] += 1;
        snapshot.weekdays[std::clamp(local.date().dayOfWeek() - 1, 0, 6)] += 1;
        activeDays.insert(local.date());

        count(tracks, song.uri, record, [&](StatEntry &entry) {
            entry.name = song.name;
            entry.subtitle = song.artists;
            entry.artworkUrl = song.artworkUrl;
            entry.song = song;
        });

        const QList<Artist> songArtists = artistsOf(song);
        QSet<QString> songGenres;
        for (const Artist &artist : songArtists) {
            const QString key = artist.id.isEmpty() ? QStringLiteral("name:") + artist.name.toLower() : artist.id;
            count(artists, key, record, [&](StatEntry &entry) {
                entry.name = artist.name;
                entry.artworkUrl = artist.artworkUrl;
                entry.artist = artist;
            });
            for (const QString &genre : genres.value(artist.id)) {
                songGenres.insert(genre);
            }
        }
        // a song counts once for each genre its artists play, however many
        // of them play it
        for (const QString &genre : std::as_const(songGenres)) {
            count(genreTallies, genre, record, [&](StatEntry &entry) { entry.name = genre; });
        }

        const QString albumKey = !song.albumId.isEmpty() ? song.albumId
            : song.albumName.isEmpty() ? QString()
                                       : QStringLiteral("name:") + song.albumName.toLower() + u'|' + song.artists.toLower();
        count(albums, albumKey, record, [&](StatEntry &entry) {
            entry.name = song.albumName;
            entry.subtitle = song.artists;
            entry.artworkUrl = song.artworkUrl;
            entry.album = song.albumDestination();
            if (entry.album.name.isEmpty()) {
                entry.album.name = song.albumName;
                entry.album.artists = song.artists;
                entry.album.artworkUrl = song.artworkUrl;
            }
        });
    }

    snapshot.uniqueTracks = int(tracks.size());
    snapshot.uniqueArtists = int(artists.size());
    snapshot.uniqueAlbums = int(albums.size());
    snapshot.activeDays = int(activeDays.size());
    snapshot.topTracks = ranked(tracks);
    snapshot.topArtists = ranked(artists);
    snapshot.topAlbums = ranked(albums);
    snapshot.topGenres = ranked(genreTallies);
    snapshot.streakDays = streakEndingAt(snapshot.daily, now.toLocalTime().date());

    // the latest first, whatever order the history holds them in
    QList<const PlayRecord *> latest;
    latest.reserve(records.size());
    for (const PlayRecord &record : records) {
        latest.append(&record);
    }
    std::stable_sort(latest.begin(), latest.end(),
                     [](const PlayRecord *a, const PlayRecord *b) { return a->startedAt > b->startedAt; });
    for (qsizetype index = 0; index < latest.size() && snapshot.recent.size() < kRecentLimit; ++index) {
        const PlayRecord &record = *latest.at(index);
        StatEntry entry;
        entry.key = record.song.uri;
        entry.name = record.song.name;
        entry.subtitle = record.song.artists;
        entry.artworkUrl = record.song.artworkUrl;
        entry.playedAt = record.startedAt;
        entry.listenedMs = record.listenedMs;
        entry.plays = 1;
        entry.song = record.song;
        snapshot.recent.append(entry);
    }
    return snapshot;
}

} // namespace swiftify
