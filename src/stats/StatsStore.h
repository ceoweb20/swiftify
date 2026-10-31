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

#include "core/Async.h"
#include "library/SongListModel.h"
#include "stats/StatsFmClient.h"
#include "stats/StatsListModel.h"
#include "stats/StatsTypes.h"

#include <QHash>
#include <QSet>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QNetworkAccessManager;
class QQmlEngine;
class QJSEngine;

namespace swiftify {

class PlayHistory;
class PlayTracker;
class Settings;
class SpotifySession;

/// the stats page (listening-stats for spicetify, after stats.fm): the top
/// songs, artists, albums and genres of a period, when the listening
/// happened, and the plays of the last year on a calendar. read from stats.fm
/// by username, or worked out from the plays kept on this computer
class StatsStore : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Stats)
    QML_SINGLETON

    /// "statsfm" or "local"
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY sourceChanged FINAL)
    Q_PROPERTY(bool tracking READ tracking WRITE setTracking NOTIFY trackingChanged FINAL)
    /// 0 today, 1 the last four weeks, 2 the last six months, 3 all time
    Q_PROPERTY(int period READ period WRITE setPeriod NOTIFY periodChanged FINAL)
    /// rank the lists by time heard rather than by plays
    Q_PROPERTY(bool rankByTime READ rankByTime WRITE setRankByTime NOTIFY rankByTimeChanged FINAL)

    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged FINAL)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged FINAL)
    Q_PROPERTY(bool hasData READ hasData NOTIFY snapshotChanged FINAL)

    Q_PROPERTY(QString profileName READ profileName NOTIFY profileChanged FINAL)
    Q_PROPERTY(QUrl profileImage READ profileImage NOTIFY profileChanged FINAL)
    Q_PROPERTY(bool isPlus READ isPlus NOTIFY profileChanged FINAL)

    Q_PROPERTY(int plays READ plays NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(double listenedMs READ listenedMs NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(int uniqueTracks READ uniqueTracks NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(int uniqueArtists READ uniqueArtists NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(int uniqueAlbums READ uniqueAlbums NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(int streakDays READ streakDays NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(int activeDays READ activeDays NOTIFY snapshotChanged FINAL)
    /// the hour with the most plays, -1 with none
    Q_PROPERTY(int peakHour READ peakHour NOTIFY snapshotChanged FINAL)

    Q_PROPERTY(swiftify::StatsListModel *topTracks READ topTracks CONSTANT FINAL)
    Q_PROPERTY(swiftify::StatsListModel *topArtists READ topArtists CONSTANT FINAL)
    Q_PROPERTY(swiftify::StatsListModel *topAlbums READ topAlbums CONSTANT FINAL)
    Q_PROPERTY(swiftify::StatsListModel *topGenres READ topGenres CONSTANT FINAL)
    Q_PROPERTY(swiftify::StatsListModel *recent READ recent CONSTANT FINAL)
    /// the same songs as the top tracks and recent plays, in their order,
    /// for the player to play from
    Q_PROPERTY(swiftify::SongListModel *topTrackSongs READ topTrackSongs CONSTANT FINAL)
    Q_PROPERTY(swiftify::SongListModel *recentSongs READ recentSongs CONSTANT FINAL)

    /// plays by hour, 0 to 23
    Q_PROPERTY(QVariantList hourly READ hourly NOTIFY snapshotChanged FINAL)
    /// plays by day of the week, monday first
    Q_PROPERTY(QVariantList weekdays READ weekdays NOTIFY snapshotChanged FINAL)
    /// the last year a week a column, monday at the top: each week a list
    /// of seven play counts, -1 for days still to come
    Q_PROPERTY(QVariantList calendarWeeks READ calendarWeeks NOTIFY snapshotChanged FINAL)
    /// {week, label} where each month begins on the calendar
    Q_PROPERTY(QVariantList calendarMonths READ calendarMonths NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(int calendarMax READ calendarMax NOTIFY snapshotChanged FINAL)
    Q_PROPERTY(int calendarTotal READ calendarTotal NOTIFY snapshotChanged FINAL)

    /// plays kept on this computer, all told
    Q_PROPERTY(int historyCount READ historyCount NOTIFY historyChanged FINAL)

public:
    StatsStore(Settings *settings, SpotifySession *session, QNetworkAccessManager *network, PlayHistory *history,
               PlayTracker *tracker, QObject *parent = nullptr);
    ~StatsStore() override;

    static StatsStore *instance();
    static StatsStore *create(QQmlEngine *, QJSEngine *);

    QString source() const;
    void setSource(const QString &source);
    QString username() const;
    void setUsername(const QString &username);
    bool tracking() const;
    void setTracking(bool tracking);
    int period() const { return int(m_period); }
    void setPeriod(int period);
    bool rankByTime() const { return m_rankByTime; }
    void setRankByTime(bool byTime);

    bool isLoading() const { return m_loading; }
    QString errorMessage() const { return m_errorMessage; }
    bool hasData() const { return m_snapshot.plays > 0 || !m_snapshot.topTracks.isEmpty(); }

    QString profileName() const { return m_profile.displayName; }
    QUrl profileImage() const { return m_profile.imageUrl; }
    bool isPlus() const { return m_profile.isPlus; }

    int plays() const { return m_snapshot.plays; }
    double listenedMs() const { return double(m_snapshot.listenedMs); }
    int uniqueTracks() const { return m_snapshot.uniqueTracks; }
    int uniqueArtists() const { return m_snapshot.uniqueArtists; }
    int uniqueAlbums() const { return m_snapshot.uniqueAlbums; }
    int streakDays() const { return m_snapshot.streakDays; }
    int activeDays() const { return m_snapshot.activeDays; }
    int peakHour() const;

    StatsListModel *topTracks() const { return m_topTracks; }
    StatsListModel *topArtists() const { return m_topArtists; }
    StatsListModel *topAlbums() const { return m_topAlbums; }
    StatsListModel *topGenres() const { return m_topGenres; }
    StatsListModel *recent() const { return m_recent; }
    SongListModel *topTrackSongs() const { return m_topTrackSongs; }
    SongListModel *recentSongs() const { return m_recentSongs; }

    QVariantList hourly() const;
    QVariantList weekdays() const;
    QVariantList calendarWeeks() const { return m_calendarWeeks; }
    QVariantList calendarMonths() const { return m_calendarMonths; }
    int calendarMax() const { return m_calendarMax; }
    int calendarTotal() const { return m_calendarTotal; }

    int historyCount() const;

    /// reads the stats again
    Q_INVOKABLE void refresh();
    /// the local history as csv in the downloads folder; the file's path, or
    /// empty when it could not be written
    Q_INVOKABLE QString exportHistory() const;

    /// the calendar for `daily` ending the week of `today`, for the tests
    static QVariantList calendarFor(const QMap<QDate, int> &daily, const QDate &today, QVariantList *months = nullptr,
                                    int *maximum = nullptr, int *total = nullptr);

signals:
    void sourceChanged();
    void trackingChanged();
    void periodChanged();
    void rankByTimeChanged();
    void loadingChanged();
    void errorChanged();
    void profileChanged();
    void snapshotChanged();
    void historyChanged();

private:
    bool usesStatsFm() const;
    void setLoading(bool loading);
    void setError(const QString &message);
    void apply(const StatsSnapshot &snapshot);
    void reorder();
    void computeLocal();
    QCoro::Task<> loadStatsFm(CancelToken token);
    QCoro::Task<> fillGenres(QStringList artistIds, CancelToken token);
    void loadGenreCache();
    void saveGenreCache() const;

    Settings *m_settings;
    SpotifySession *m_session;
    PlayHistory *m_history;
    PlayTracker *m_tracker;
    StatsFmClient m_statsFm;
    StatsPeriod m_period = StatsPeriod::Weeks;
    bool m_rankByTime = false;
    bool m_loading = false;
    QString m_errorMessage;
    StatsFmProfile m_profile;
    QString m_profileFor;
    StatsSnapshot m_snapshot;
    TaskSlot m_load;
    TaskSlot m_genreLoad;
    /// a recorded play folds into the local stats a moment later, with any
    /// others that land in the meantime
    QTimer m_localRefresh;

    StatsListModel *m_topTracks;
    StatsListModel *m_topArtists;
    StatsListModel *m_topAlbums;
    StatsListModel *m_topGenres;
    StatsListModel *m_recent;
    SongListModel *m_topTrackSongs;
    SongListModel *m_recentSongs;

    QVariantList m_calendarWeeks;
    QVariantList m_calendarMonths;
    int m_calendarMax = 0;
    int m_calendarTotal = 0;

    /// what spotify says each artist plays, and their pictures, kept between
    /// runs: an artist is asked about once
    QHash<QString, QStringList> m_genres;
    QHash<QString, QUrl> m_artistArtwork;
    QSet<QString> m_genresAsked;
};

} // namespace swiftify
