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

#include "app/Settings.h"
#include "stats/PlayHistory.h"
#include "stats/StatsAggregator.h"
#include "stats/StatsFmClient.h"
#include "stats/StatsListModel.h"
#include "stats/StatsStore.h"

#include "StatsFixtures.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimeZone>

using namespace swiftify;

namespace {

Song makeSong(const QString &id, const QString &name, const QString &artistId, const QString &artistName,
              const QString &albumId = QString(), int durationMs = 200'000)
{
    Song song;
    song.uri = QStringLiteral("spotify:track:") + id;
    song.name = name;
    song.artists = artistName;
    song.durationMs = durationMs;
    song.albumId = albumId;
    song.albumName = albumId.isEmpty() ? QString() : QStringLiteral("Album ") + albumId;
    Artist artist;
    artist.id = artistId;
    artist.name = artistName;
    artist.uri = QStringLiteral("spotify:artist:") + artistId;
    song.artistItems = {artist};
    return song;
}

PlayRecord play(const Song &song, const QDateTime &when, qint64 ms = 180'000)
{
    return {when.toUTC(), ms, song};
}

QJsonObject parse(const char *json)
{
    return QJsonDocument::fromJson(json).object();
}

} // namespace

class StatsTest : public QObject {
    Q_OBJECT

private slots:
    void aPlayCountsAfterThirtySecondsOrHalfAShortSong()
    {
        const Song song = makeSong(QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("x"), QStringLiteral("X"));
        QVERIFY(!PlayHistory::counts(song, 29'999));
        QVERIFY(PlayHistory::counts(song, 30'000));

        Song jingle = song;
        jingle.durationMs = 20'000;
        QVERIFY(!PlayHistory::counts(jingle, 9'999));
        QVERIFY(PlayHistory::counts(jingle, 10'000));

        QVERIFY(!PlayHistory::counts(Song {}, 60'000));
    }

    void historyIsKeptAcrossRunsAndSkipsBrokenLines()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("history.jsonl"));
        const Song song = makeSong(QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("x"), QStringLiteral("X"), QStringLiteral("al"));
        const QDateTime when = QDateTime(QDate(2026, 3, 4), QTime(21, 15), QTimeZone::UTC);
        {
            PlayHistory history(path);
            QSignalSpy recorded(&history, &PlayHistory::recorded);
            QVERIFY(history.record(song, when, 120'000));
            QVERIFY(!history.record(song, when, 5'000));
            QCOMPARE(recorded.count(), 1);
        }
        // a line cut short, as a crash mid-write would leave it
        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::Append));
            file.write("{\"t\": 17, \"song\": {\"na");
        }
        PlayHistory again(path);
        QCOMPARE(again.records().size(), 1);
        const PlayRecord &record = again.records().first();
        QCOMPARE(record.startedAt, when);
        QCOMPARE(record.listenedMs, 120'000);
        QCOMPARE(record.song.uri, song.uri);
        QCOMPARE(record.song.albumId, QStringLiteral("al"));
        QCOMPARE(record.song.artistItems.first().id, QStringLiteral("x"));
        QVERIFY(again.toCsv().contains("\"A\",\"X\",\"Album al\""));
    }

    void trackerCountsOnlyTimeSpentPlaying()
    {
        PlayHistory history(QString {});
        PlayTracker tracker(nullptr, &history);
        const Song first = makeSong(QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("x"), QStringLiteral("X"), {}, 120);
        const Song second = makeSong(QStringLiteral("b"), QStringLiteral("B"), QStringLiteral("x"), QStringLiteral("X"), {}, 120);

        tracker.observe(first, true);
        QThread::msleep(80);
        tracker.observe(first, false);
        // paused: none of this counts
        QThread::msleep(200);
        tracker.observe(second, true);
        QCOMPARE(history.records().size(), 1);
        QCOMPARE(history.records().first().song.uri, first.uri);
        QVERIFY(history.records().first().listenedMs >= 60);
        QVERIFY(history.records().first().listenedMs < 200);

        // a skip straight away is no play
        tracker.observe(first.freshInstance(), true);
        QCOMPARE(history.records().size(), 1);

        tracker.setEnabled(false);
        tracker.observe(second, true);
        QThread::msleep(80);
        tracker.finish();
        QCOMPARE(history.records().size(), 1);
    }

    void aggregatesTopListsActivityAndStreaks()
    {
        const QDateTime now = QDateTime(QDate(2026, 10, 18), QTime(12, 0));
        const Song a = makeSong(QStringLiteral("a"), QStringLiteral("Alpha"), QStringLiteral("x"), QStringLiteral("Xavier"), QStringLiteral("al1"));
        const Song b = makeSong(QStringLiteral("b"), QStringLiteral("Beta"), QStringLiteral("y"), QStringLiteral("Yara"), QStringLiteral("al2"));
        const Song c = makeSong(QStringLiteral("c"), QStringLiteral("Gamma"), QStringLiteral("x"), QStringLiteral("Xavier"), QStringLiteral("al1"));
        const QList<PlayRecord> records {
            // long ago: only lifetime counts it
            play(b, now.addMonths(-8)),
            play(a, now.addDays(-2).addSecs(-3600)),
            play(a, now.addDays(-1)),
            play(c, now.addDays(-1).addSecs(60)),
            play(a, now.addSecs(-600)),
            play(b, now.addSecs(-300), 60'000),
        };
        const QHash<QString, QStringList> genres {{QStringLiteral("x"), {QStringLiteral("indie"), QStringLiteral("pop")}},
                                                  {QStringLiteral("y"), {QStringLiteral("pop")}}};

        const StatsSnapshot weeks = StatsAggregator::aggregate(records, StatsPeriod::Weeks, now, genres);
        QCOMPARE(weeks.plays, 5);
        QCOMPARE(weeks.uniqueTracks, 3);
        QCOMPARE(weeks.uniqueArtists, 2);
        QCOMPARE(weeks.uniqueAlbums, 2);
        QCOMPARE(weeks.topTracks.first().name, QStringLiteral("Alpha"));
        QCOMPARE(weeks.topTracks.first().plays, 3);
        QCOMPARE(weeks.topArtists.first().name, QStringLiteral("Xavier"));
        QCOMPARE(weeks.topArtists.first().plays, 4);
        QCOMPARE(weeks.topAlbums.first().key, QStringLiteral("al1"));
        // every play of either artist is pop; only xavier's are indie
        QCOMPARE(weeks.topGenres.first().name, QStringLiteral("pop"));
        QCOMPARE(weeks.topGenres.first().plays, 5);
        QCOMPARE(weeks.topGenres.at(1).plays, 4);
        QCOMPARE(weeks.streakDays, 3);
        QCOMPARE(weeks.activeDays, 3);
        QCOMPARE(weeks.recent.first().name, QStringLiteral("Beta"));
        QCOMPARE(weeks.recent.size(), 6);
        int hourly = 0;
        for (const int count : weeks.hourly) hourly += count;
        QCOMPARE(hourly, 5);

        const StatsSnapshot today = StatsAggregator::aggregate(records, StatsPeriod::Today, now, genres);
        QCOMPARE(today.plays, 2);
        // the calendar keeps the whole year whatever the period
        QCOMPARE(today.daily.size(), 4);

        const StatsSnapshot lifetime = StatsAggregator::aggregate(records, StatsPeriod::Lifetime, now, genres);
        QCOMPARE(lifetime.plays, 6);
        QCOMPARE(lifetime.listenedMs, 5 * 180'000 + 60'000);
    }

    void aStreakSurvivesATodayWithNothingYet()
    {
        const QDate today(2026, 10, 18);
        QMap<QDate, int> daily {{today.addDays(-1), 3}, {today.addDays(-2), 1}, {today.addDays(-4), 2}};
        QCOMPARE(streakEndingAt(daily, today), 2);
        daily.insert(today, 1);
        QCOMPARE(streakEndingAt(daily, today), 3);
        QCOMPARE(streakEndingAt({}, today), 0);
    }

    void listsRankByTimeWhenAsked()
    {
        StatsListModel model;
        StatEntry often;
        often.name = QStringLiteral("often");
        often.plays = 10;
        often.listenedMs = 1'000;
        StatEntry long_;
        long_.name = QStringLiteral("long");
        long_.plays = 2;
        long_.listenedMs = 9'000;
        model.setEntries({often, long_}, false);
        QCOMPARE(model.nameAt(0), QStringLiteral("often"));
        QCOMPARE(model.data(model.index(1), StatsListModel::ShareRole).toDouble(), 0.2);
        model.setEntries({often, long_}, true);
        QCOMPARE(model.nameAt(0), QStringLiteral("long"));
        QCOMPARE(model.data(model.index(0), StatsListModel::RankRole).toInt(), 1);
    }

    void usernamesAreTakenFromLinksToo_data()
    {
        QTest::addColumn<QString>("raw");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << "  someone " << "someone";
        QTest::newRow("at") << "@someone" << "someone";
        QTest::newRow("link") << "https://stats.fm/someone" << "someone";
        QTest::newRow("user link") << "stats.fm/user/some.one_2" << "some.one_2";
        QTest::newRow("bad") << "two words" << "";
        QTest::newRow("empty") << "" << "";
    }
    void usernamesAreTakenFromLinksToo()
    {
        QFETCH(QString, raw);
        QFETCH(QString, expected);
        QCOMPARE(StatsFmClient::normalizeUsername(raw), expected);
    }

    void readsStatsFmTopTracks()
    {
        const StatEntry track = StatsFmClient::track(parse(kTrackJson));
        QCOMPARE(track.name, QStringLiteral("Song"));
        QCOMPARE(track.plays, 42);
        QCOMPARE(track.listenedMs, 7'560'000);
        QCOMPARE(track.song.uri, QStringLiteral("spotify:track:4uLU6hMCjMI75M1A2tKUQC"));
        QCOMPARE(track.song.artists, QStringLiteral("Singer, Guest"));
        QCOMPARE(track.song.artistItems.size(), 2);
        QCOMPARE(track.song.albumId, QStringLiteral("alb1"));
        QCOMPARE(track.artworkUrl, QUrl(QStringLiteral("https://i.scdn.co/image/abc")));

        // without plus there is no time heard: the song's length a play
        const StatEntry estimated = StatsFmClient::track(parse(kEstimatedTrackJson));
        QCOMPARE(estimated.listenedMs, 3'000);
    }

    void freeAccountsGetTheirCountsFromEachPlacesStats()
    {
        StatEntry track = StatsFmClient::track(parse(kFreeTrackJson));
        // left out, not none
        QCOMPARE(track.plays, -1);
        QCOMPARE(track.listenedMs, 0);
        QCOMPARE(track.statsFmId, 42997036);
        QCOMPARE(track.statsFmAlbumId, 7756776);
        QCOMPARE(track.song.uri, QStringLiteral("spotify:track:6TxR6ScvxDzMZBj9UgxYx4"));
        QCOMPARE(track.song.artists, QStringLiteral("Marlon Funaki"));

        StatsFmClient::readCounts(parse(kItemStatsJson), track);
        QCOMPARE(track.plays, 13);
        QCOMPARE(track.listenedMs, 4'112'663);

        // albums from songs without counts stay unknown until their own stats come
        const QList<StatEntry> albums = StatsFmClient::albumsFromTracks({StatsFmClient::track(parse(kFreeTrackJson))});
        QCOMPARE(albums.size(), 1);
        QCOMPARE(albums.first().plays, -1);
        QCOMPARE(albums.first().statsFmId, 7756776);

        StatEntry artist = StatsFmClient::artist(parse(kFreeArtistJson));
        QCOMPARE(artist.plays, -1);
        QCOMPARE(artist.statsFmId, 19967);
        artist.plays = 36;
        StatEntry other;
        other.name = QStringLiteral("Other");
        other.plays = 4;
        other.genres = {QStringLiteral("indie")};
        StatEntry psych = StatsFmClient::genre(parse("{\"streams\": null, \"genre\": {\"tag\": \"neo-psychedelic\"}}"));
        StatEntry indie = StatsFmClient::genre(parse("{\"streams\": null, \"genre\": {\"tag\": \"indie\"}}"));
        QCOMPARE(psych.plays, -1);
        QList<StatEntry> genres {psych, indie};
        StatsFmClient::weighGenres(genres, {artist, other});
        QCOMPARE(genres.first().name, QStringLiteral("indie"));
        QCOMPARE(genres.first().plays, 40);
        QCOMPARE(genres.at(1).plays, 36);
    }

    void unknownCountsGetNoShare()
    {
        StatsListModel model;
        StatEntry known;
        known.plays = 5;
        StatEntry unknown;
        unknown.plays = -1;
        model.setEntries({known, unknown}, false);
        QCOMPARE(model.data(model.index(0), StatsListModel::ShareRole).toDouble(), 1.0);
        QCOMPARE(model.data(model.index(1), StatsListModel::ShareRole).toDouble(), 0.0);
    }

    void daysAndHoursAreAskedForInThisTimeZone()
    {
        QVERIFY(!StatsFmClient::timeZoneName().isEmpty());
        QVERIFY(QTimeZone(StatsFmClient::timeZoneName().toUtf8()).isValid());
    }

    void readsStatsFmArtistsGenresAndActivity()
    {
        const StatEntry artist = StatsFmClient::artist(parse(kArtistJson));
        QCOMPARE(artist.artist.id, QStringLiteral("art1"));
        QCOMPARE(artist.subtitle, QStringLiteral("indie, pop"));
        QCOMPARE(artist.plays, 9);

        const StatEntry genre = StatsFmClient::genre(parse(kGenreJson));
        QCOMPARE(genre.name, QStringLiteral("shoegaze"));

        StatsSnapshot snapshot;
        StatsFmClient::readDates(parse(kDatesJson), snapshot);
        QCOMPARE(snapshot.hourly[0], 2);
        QCOMPARE(snapshot.hourly[21], 9);
        QCOMPARE(snapshot.weekdays[0], 4);
        QCOMPARE(snapshot.weekdays[6], 6);

        StatsFmClient::readDays(parse(kDaysJson), snapshot);
        QCOMPARE(snapshot.daily.value(QDate(2026, 10, 16)), 3);
        QVERIFY(!snapshot.daily.contains(QDate(2026, 10, 17)));
    }

    void albumsComeFromTracksWithoutPlus()
    {
        StatEntry one;
        one.song = makeSong(QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("x"), QStringLiteral("X"), QStringLiteral("al1"));
        one.plays = 2;
        StatEntry two;
        two.song = makeSong(QStringLiteral("b"), QStringLiteral("B"), QStringLiteral("y"), QStringLiteral("Y"), QStringLiteral("al2"));
        two.plays = 5;
        StatEntry three;
        three.song = makeSong(QStringLiteral("c"), QStringLiteral("C"), QStringLiteral("x"), QStringLiteral("X"), QStringLiteral("al1"));
        three.plays = 4;
        const QList<StatEntry> albums = StatsFmClient::albumsFromTracks({one, two, three});
        QCOMPARE(albums.size(), 2);
        QCOMPARE(albums.first().key, QStringLiteral("al1"));
        QCOMPARE(albums.first().plays, 6);
        QCOMPARE(albums.first().album.id, QStringLiteral("al1"));
    }

    void calendarEndsOnTheWeekOfToday()
    {
        // a saturday
        const QDate today(2026, 10, 17);
        const QMap<QDate, int> daily {{today, 4}, {today.addDays(-1), 2}, {today.addDays(-400), 9}};
        QVariantList months;
        int maximum = 0;
        int total = 0;
        const QVariantList weeks = StatsStore::calendarFor(daily, today, &months, &maximum, &total);
        QCOMPARE(weeks.size(), 53);
        const QVariantList last = weeks.last().toList();
        QCOMPARE(last.size(), 7);
        QCOMPARE(last.at(4).toInt(), 2);
        QCOMPARE(last.at(5).toInt(), 4);
        // sunday is still to come
        QCOMPARE(last.at(6).toInt(), -1);
        QCOMPARE(maximum, 4);
        // more than a year back is off the calendar
        QCOMPARE(total, 6);
        QVERIFY(months.size() >= 12);
    }

    void storeShowsLocalStatsAndFollowsSettings()
    {
        QTemporaryDir directory;
        Settings settings(directory.filePath(QStringLiteral("swiftify.conf")));
        QCOMPARE(settings.statsSource(), QStringLiteral("local"));
        QVERIFY(settings.trackListening());
        PlayHistory history(QString {});
        PlayTracker tracker(nullptr, &history);
        StatsStore store(&settings, nullptr, nullptr, &history, &tracker);
        QVERIFY(!store.hasData());

        const Song song = makeSong(QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("x"), QStringLiteral("X"));
        QVERIFY(history.record(song, QDateTime::currentDateTimeUtc(), 90'000));
        QTRY_VERIFY(store.hasData());
        QCOMPARE(store.plays(), 1);
        QCOMPARE(store.topTracks()->count(), 1);
        QCOMPARE(store.topTrackSongs()->count(), 1);
        QCOMPARE(store.historyCount(), 1);

        store.setTracking(false);
        QVERIFY(!settings.trackListening());
        QVERIFY(!tracker.isEnabled());

        // stats.fm with no username shows nothing, and asks nothing of the network
        store.setSource(QStringLiteral("statsfm"));
        QCOMPARE(store.source(), QStringLiteral("statsfm"));
        QVERIFY(!store.hasData());
        QVERIFY(!store.isLoading());
        store.setUsername(QStringLiteral("not a name"));
        QVERIFY(store.username().isEmpty());
    }
};

QTEST_MAIN(StatsTest)
#include "tst_stats.moc"
