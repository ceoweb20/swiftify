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

#include "stats/StatsFmClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QCoroNetworkReply>

#include <algorithm>
#include <memory>
#include <vector>

namespace swiftify {

namespace {

const QString kApiBase = QStringLiteral("https://api.stats.fm/api/v1");

QString firstSpotifyId(const QJsonObject &object)
{
    const QJsonArray ids = object.value(QStringLiteral("externalIds")).toObject().value(QStringLiteral("spotify")).toArray();
    return ids.isEmpty() ? QString() : ids.first().toString();
}

/// a count stats.fm gives, or -1 where it leaves it null (no plus)
int countOf(const QJsonObject &entry)
{
    const QJsonValue streams = entry.value(QStringLiteral("streams"));
    return streams.isDouble() ? streams.toInt() : -1;
}

QUrl imageUrl(const QJsonValue &value)
{
    const QString text = value.toString();
    return text.startsWith(QLatin1StringView("https://")) ? QUrl(text) : QUrl();
}

/// a date as stats.fm writes it, a day or a moment, as the local day it fell on
QDate localDay(const QString &text)
{
    const QDateTime moment = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (moment.isValid() && text.size() > 10) {
        return moment.toLocalTime().date();
    }
    return QDate::fromString(text.left(10), Qt::ISODate);
}

/// a part of the page that stays empty when stats.fm cannot give it: it
/// awaits `task` to the end either way, so nothing is left running unowned
QCoro::Task<QJsonValue> settled(QCoro::Task<QJsonValue> &task)
{
    try {
        co_return co_await task;
    } catch (const StatsFmError &) {
        co_return QJsonValue();
    }
}

} // namespace

StatsFmClient::StatsFmClient(QNetworkAccessManager *network)
    : m_network(network)
{
}

QString StatsFmClient::normalizeUsername(const QString &raw)
{
    QString name = raw.trimmed();
    // a link to the profile, stats.fm/<name> or stats.fm/user/<name>
    if (name.contains(u'/')) {
        const QStringList parts = QUrl::fromUserInput(name).path().split(u'/', Qt::SkipEmptyParts);
        name = parts.isEmpty() ? QString() : parts.last();
    }
    if (name.startsWith(u'@')) {
        name.remove(0, 1);
    }
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9._-]{1,64}$"));
    return valid.match(name).hasMatch() ? name : QString();
}

QString StatsFmClient::timeZoneName()
{
    const QByteArray id = QTimeZone::systemTimeZoneId();
    return id.isEmpty() ? QStringLiteral("UTC") : QString::fromUtf8(id);
}

QString StatsFmClient::rangeName(StatsPeriod period)
{
    switch (period) {
    case StatsPeriod::Today: return QStringLiteral("today");
    case StatsPeriod::Weeks: return QStringLiteral("weeks");
    case StatsPeriod::Months: return QStringLiteral("months");
    case StatsPeriod::Lifetime: return QStringLiteral("lifetime");
    }
    return QStringLiteral("weeks");
}

QCoro::Task<QJsonValue> StatsFmClient::get(QString path, QUrlQuery query)
{
    QUrl url(kApiBase + path);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15'000);

    std::unique_ptr<QNetworkReply> reply(m_network->get(request));
    co_await qCoro(reply.get()).waitForFinished();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    if (status == 0) {
        throw StatsFmError(StatsFmError::Kind::Network,
                           QStringLiteral("Could not reach stats.fm: %1").arg(reply->errorString()));
    }
    if (status == 404) {
        throw StatsFmError(StatsFmError::Kind::NotFound, QStringLiteral("stats.fm has no user by that name."));
    }
    if (status == 401 || status == 403) {
        throw StatsFmError(StatsFmError::Kind::Private,
                           QStringLiteral("This stats.fm profile keeps these stats private."));
    }
    if (status == 429) {
        throw StatsFmError(StatsFmError::Kind::RateLimited, QStringLiteral("stats.fm is busy. Try again in a minute."));
    }
    if (status < 200 || status >= 300) {
        throw StatsFmError(StatsFmError::Kind::Other, QStringLiteral("stats.fm answered with HTTP %1.").arg(status));
    }
    const QJsonObject object = QJsonDocument::fromJson(data).object();
    co_return object.contains(QStringLiteral("item")) ? object.value(QStringLiteral("item"))
                                                      : object.value(QStringLiteral("items"));
}

QCoro::Task<StatsFmProfile> StatsFmClient::profile(QString username)
{
    const QJsonObject user = (co_await get(QStringLiteral("/users/%1").arg(username))).toObject();
    StatsFmProfile profile;
    profile.id = user.value(QStringLiteral("customId")).toString(user.value(QStringLiteral("id")).toString());
    profile.displayName = user.value(QStringLiteral("displayName")).toString(profile.id);
    profile.imageUrl = imageUrl(user.value(QStringLiteral("image")));
    profile.isPlus = user.value(QStringLiteral("isPlus")).toBool() || user.value(QStringLiteral("isPro")).toBool();
    if (profile.id.isEmpty()) {
        throw StatsFmError(StatsFmError::Kind::NotFound, QStringLiteral("stats.fm has no user by that name."));
    }
    co_return profile;
}

QCoro::Task<StatsSnapshot> StatsFmClient::snapshot(QString username, StatsPeriod period, bool isPlus)
{
    const QString user = QStringLiteral("/users/%1").arg(username);
    const QUrlQuery range {{QStringLiteral("range"), rangeName(period)}};
    QUrlQuery top = range;
    top.addQueryItem(QStringLiteral("limit"), QString::number(50));

    // started together; each is awaited in turn
    auto tracksTask = get(user + QStringLiteral("/top/tracks"), top);
    auto artistsTask = get(user + QStringLiteral("/top/artists"), top);
    auto genresTask = get(user + QStringLiteral("/top/genres"), top);
    auto statsTask = get(user + QStringLiteral("/streams/stats"), range);
    auto recentTask = get(user + QStringLiteral("/streams/recent"), QUrlQuery {{QStringLiteral("limit"), QStringLiteral("20")}});
    // days and hours are grouped by the time zone given, and refused without one
    QUrlQuery dated = range;
    dated.addQueryItem(QStringLiteral("timeZone"), timeZoneName());
    auto datesTask = get(user + QStringLiteral("/streams/stats/dates"), dated);
    auto daysTask = get(user + QStringLiteral("/streams/stats/per-day"),
                        QUrlQuery {{QStringLiteral("range"), QStringLiteral("lifetime")},
                                   {QStringLiteral("timeZone"), timeZoneName()}});

    StatsSnapshot snapshot;
    // the top tracks decide whether the profile can be read at all; the rest
    // leave their part of the page empty when they fail
    for (const QJsonValue &value : (co_await tracksTask).toArray()) {
        if (StatEntry entry = track(value.toObject()); !entry.name.isEmpty()) {
            snapshot.topTracks.append(entry);
        }
    }

    for (const QJsonValue &item : (co_await settled(artistsTask)).toArray()) {
        if (StatEntry entry = artist(item.toObject()); !entry.name.isEmpty()) snapshot.topArtists.append(entry);
    }
    for (const QJsonValue &item : (co_await settled(genresTask)).toArray()) {
        if (StatEntry entry = genre(item.toObject()); !entry.name.isEmpty()) snapshot.topGenres.append(entry);
    }
    const QJsonObject stats = (co_await settled(statsTask)).toObject();
    snapshot.plays = stats.value(QStringLiteral("count")).toInt();
    snapshot.listenedMs = stats.value(QStringLiteral("durationMs")).toInteger();
    const QJsonObject cardinality = stats.value(QStringLiteral("cardinality")).toObject();
    snapshot.uniqueTracks = cardinality.value(QStringLiteral("tracks")).toInt();
    snapshot.uniqueArtists = cardinality.value(QStringLiteral("artists")).toInt();
    snapshot.uniqueAlbums = cardinality.value(QStringLiteral("albums")).toInt();
    for (const QJsonValue &item : (co_await settled(recentTask)).toArray()) {
        if (StatEntry entry = recentStream(item.toObject()); !entry.name.isEmpty()) snapshot.recent.append(entry);
    }
    readDates((co_await settled(datesTask)).toObject(), snapshot);
    readDays((co_await settled(daysTask)).toObject(), snapshot);

    if (isPlus) {
        try {
            QList<StatEntry> albums;
            for (const QJsonValue &item : (co_await get(user + QStringLiteral("/top/albums"), top)).toArray()) {
                if (StatEntry entry = album(item.toObject()); !entry.name.isEmpty()) albums.append(entry);
            }
            snapshot.topAlbums = albums;
        } catch (const StatsFmError &) {
            snapshot.topAlbums = albumsFromTracks(snapshot.topTracks);
        }
    } else {
        snapshot.topAlbums = albumsFromTracks(snapshot.topTracks);
    }

    // without plus the top lists come without counts: each place's own
    // stats have them, asked for a few at a time
    const QString rangeText = rangeName(period);
    co_await fillCounts(user, QStringLiteral("tracks"), &snapshot.topTracks, rangeText);
    co_await fillCounts(user, QStringLiteral("artists"), &snapshot.topArtists, rangeText);
    co_await fillCounts(user, QStringLiteral("albums"), &snapshot.topAlbums, rangeText);
    if (!isPlus) {
        std::stable_sort(snapshot.topAlbums.begin(), snapshot.topAlbums.end(),
                         [](const StatEntry &a, const StatEntry &b) { return a.plays > b.plays; });
    }
    weighGenres(snapshot.topGenres, snapshot.topArtists);
    // the days in the period, from the calendar
    const QDateTime start = periodStart(period, QDateTime::currentDateTime());
    for (auto it = snapshot.daily.cbegin(); it != snapshot.daily.cend(); ++it) {
        if (it.value() > 0 && (!start.isValid() || it.key() >= start.date())) {
            ++snapshot.activeDays;
        }
    }
    snapshot.streakDays = streakEndingAt(snapshot.daily, QDate::currentDate());
    co_return snapshot;
}

QCoro::Task<> StatsFmClient::fillCounts(QString user, QString kind, QList<StatEntry> *entries, QString range)
{
    constexpr qsizetype kAtOnce = 6;
    constexpr qint64 kFreshSeconds = 10 * 60;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QUrlQuery query {{QStringLiteral("range"), range}, {QStringLiteral("timeZone"), timeZoneName()}};
    const auto keyOf = [&](const StatEntry &entry) {
        return kind + u'/' + QString::number(entry.statsFmId) + u'/' + range;
    };

    QList<qsizetype> missing;
    for (qsizetype index = 0; index < std::min<qsizetype>(kCountedPlaces, entries->size()); ++index) {
        StatEntry &entry = (*entries)[index];
        if (entry.plays >= 0 || entry.statsFmId <= 0) {
            continue;
        }
        if (const auto cached = m_counts.constFind(keyOf(entry));
            cached != m_counts.cend() && cached->fetchedAt.secsTo(now) < kFreshSeconds) {
            entry.plays = cached->plays;
            entry.listenedMs = cached->listenedMs;
            continue;
        }
        missing.append(index);
    }

    for (qsizetype start = 0; start < missing.size(); start += kAtOnce) {
        const qsizetype end = std::min(missing.size(), start + kAtOnce);
        std::vector<QCoro::Task<QJsonValue>> batch;
        for (qsizetype at = start; at < end; ++at) {
            const StatEntry &entry = entries->at(missing.at(at));
            batch.push_back(get(user + QStringLiteral("/streams/%1/%2/stats").arg(kind).arg(entry.statsFmId), query));
        }
        // every one awaited to the end, so none is left running unowned
        bool busy = false;
        for (qsizetype at = start; at < end; ++at) {
            StatEntry &entry = (*entries)[missing.at(at)];
            try {
                readCounts((co_await batch[size_t(at - start)]).toObject(), entry);
                if (entry.plays >= 0) {
                    m_counts.insert(keyOf(entry), {entry.plays, entry.listenedMs, now});
                }
            } catch (const StatsFmError &error) {
                // the rest stay without counts rather than the page failing
                busy = busy || error.kind() == StatsFmError::Kind::RateLimited;
            }
        }
        if (busy) {
            co_return;
        }
    }
}

void StatsFmClient::readCounts(const QJsonObject &stats, StatEntry &entry)
{
    if (!stats.value(QStringLiteral("count")).isDouble()) {
        return;
    }
    entry.plays = stats.value(QStringLiteral("count")).toInt();
    entry.listenedMs = stats.value(QStringLiteral("durationMs")).toInteger();
}

void StatsFmClient::weighGenres(QList<StatEntry> &genres, const QList<StatEntry> &artists)
{
    bool weighed = false;
    for (StatEntry &genre : genres) {
        if (genre.plays >= 0) {
            continue;
        }
        int plays = 0;
        qint64 listened = 0;
        for (const StatEntry &artist : artists) {
            if (artist.plays > 0 && artist.genres.contains(genre.name, Qt::CaseInsensitive)) {
                plays += artist.plays;
                listened += artist.listenedMs;
            }
        }
        genre.plays = plays;
        genre.listenedMs = listened;
        weighed = true;
    }
    if (weighed) {
        std::stable_sort(genres.begin(), genres.end(),
                         [](const StatEntry &a, const StatEntry &b) { return a.plays > b.plays; });
    }
}

Song StatsFmClient::song(const QJsonObject &track)
{
    Song song;
    const QString id = firstSpotifyId(track);
    song.name = track.value(QStringLiteral("name")).toString();
    song.uri = id.isEmpty() ? QString() : QStringLiteral("spotify:track:") + id;
    song.durationMs = track.value(QStringLiteral("durationMs")).toInt();
    song.isExplicit = track.value(QStringLiteral("explicit")).toBool();

    QStringList names;
    for (const QJsonValue &value : track.value(QStringLiteral("artists")).toArray()) {
        const QJsonObject object = value.toObject();
        Artist artist;
        artist.id = firstSpotifyId(object);
        artist.name = object.value(QStringLiteral("name")).toString();
        artist.uri = artist.id.isEmpty() ? QString() : QStringLiteral("spotify:artist:") + artist.id;
        artist.artworkUrl = imageUrl(object.value(QStringLiteral("image")));
        if (!artist.name.isEmpty()) {
            names.append(artist.name);
            if (!artist.id.isEmpty()) {
                song.artistItems.append(artist);
            }
        }
    }
    song.artists = names.join(QStringLiteral(", "));

    const QJsonArray albums = track.value(QStringLiteral("albums")).toArray();
    if (!albums.isEmpty()) {
        const QJsonObject album = albums.first().toObject();
        song.albumName = album.value(QStringLiteral("name")).toString();
        song.artworkUrl = imageUrl(album.value(QStringLiteral("image")));
        song.albumId = firstSpotifyId(album);
        song.albumUri = song.albumId.isEmpty() ? QString() : QStringLiteral("spotify:album:") + song.albumId;
    }
    return song;
}

StatEntry StatsFmClient::track(const QJsonObject &entry)
{
    StatEntry stat;
    stat.song = song(entry.value(QStringLiteral("track")).toObject());
    stat.key = stat.song.uri.isEmpty() ? stat.song.name : stat.song.uri;
    stat.name = stat.song.name;
    stat.subtitle = stat.song.artists;
    stat.artworkUrl = stat.song.artworkUrl;
    const QJsonObject object = entry.value(QStringLiteral("track")).toObject();
    stat.statsFmId = object.value(QStringLiteral("id")).toInteger();
    stat.statsFmAlbumId = object.value(QStringLiteral("albums")).toArray().first().toObject()
                              .value(QStringLiteral("id")).toInteger();
    stat.plays = countOf(entry);
    // the time heard comes with plus; otherwise it is about the song's
    // length once for each play
    stat.listenedMs = entry.value(QStringLiteral("playedMs")).isDouble()
        ? entry.value(QStringLiteral("playedMs")).toInteger()
        : qint64(stat.song.durationMs) * std::max(0, stat.plays);
    return stat;
}

StatEntry StatsFmClient::artist(const QJsonObject &entry)
{
    const QJsonObject object = entry.value(QStringLiteral("artist")).toObject();
    StatEntry stat;
    stat.artist.id = firstSpotifyId(object);
    stat.artist.name = object.value(QStringLiteral("name")).toString();
    stat.artist.uri = stat.artist.id.isEmpty() ? QString() : QStringLiteral("spotify:artist:") + stat.artist.id;
    stat.artist.artworkUrl = imageUrl(object.value(QStringLiteral("image")));
    stat.key = stat.artist.id.isEmpty() ? stat.artist.name : stat.artist.id;
    stat.name = stat.artist.name;
    stat.artworkUrl = stat.artist.artworkUrl;
    QStringList genres;
    for (const QJsonValue &value : object.value(QStringLiteral("genres")).toArray()) {
        genres.append(value.toString());
    }
    stat.subtitle = genres.mid(0, 2).join(QStringLiteral(", "));
    stat.genres = genres;
    stat.statsFmId = object.value(QStringLiteral("id")).toInteger();
    stat.plays = countOf(entry);
    stat.listenedMs = entry.value(QStringLiteral("playedMs")).toInteger();
    return stat;
}

StatEntry StatsFmClient::album(const QJsonObject &entry)
{
    const QJsonObject object = entry.value(QStringLiteral("album")).toObject();
    StatEntry stat;
    stat.album.id = firstSpotifyId(object);
    stat.album.name = object.value(QStringLiteral("name")).toString();
    stat.album.uri = stat.album.id.isEmpty() ? QString() : QStringLiteral("spotify:album:") + stat.album.id;
    stat.album.artworkUrl = imageUrl(object.value(QStringLiteral("image")));
    QStringList names;
    for (const QJsonValue &value : object.value(QStringLiteral("artists")).toArray()) {
        names.append(value.toObject().value(QStringLiteral("name")).toString());
    }
    names.removeAll(QString());
    stat.album.artists = names.join(QStringLiteral(", "));
    stat.key = stat.album.id.isEmpty() ? stat.album.name : stat.album.id;
    stat.name = stat.album.name;
    stat.subtitle = stat.album.artists;
    stat.artworkUrl = stat.album.artworkUrl;
    stat.statsFmId = object.value(QStringLiteral("id")).toInteger();
    stat.plays = countOf(entry);
    stat.listenedMs = entry.value(QStringLiteral("playedMs")).toInteger();
    return stat;
}

StatEntry StatsFmClient::genre(const QJsonObject &entry)
{
    StatEntry stat;
    stat.name = entry.value(QStringLiteral("genre")).toObject().value(QStringLiteral("tag")).toString();
    stat.key = stat.name;
    stat.plays = countOf(entry);
    stat.listenedMs = entry.value(QStringLiteral("playedMs")).toInteger();
    return stat;
}

StatEntry StatsFmClient::recentStream(const QJsonObject &entry)
{
    StatEntry stat;
    stat.song = song(entry.value(QStringLiteral("track")).toObject());
    stat.key = stat.song.uri;
    stat.name = stat.song.name;
    stat.subtitle = stat.song.artists;
    stat.artworkUrl = stat.song.artworkUrl;
    stat.playedAt = QDateTime::fromString(entry.value(QStringLiteral("endTime")).toString(), Qt::ISODateWithMs);
    stat.plays = 1;
    stat.listenedMs = entry.value(QStringLiteral("playedMs")).toInteger(stat.song.durationMs);
    return stat;
}

void StatsFmClient::readDates(const QJsonObject &dates, StatsSnapshot &snapshot)
{
    const QJsonObject hours = dates.value(QStringLiteral("hours")).toObject();
    for (auto it = hours.begin(); it != hours.end(); ++it) {
        bool ok = false;
        const int hour = it.key().toInt(&ok);
        if (ok && hour >= 0 && hour < 24) {
            snapshot.hourly[hour] = it.value().toObject().value(QStringLiteral("count")).toInt();
        }
    }
    // 1 is monday, 7 sunday
    const QJsonObject weekDays = dates.value(QStringLiteral("weekDays")).toObject();
    for (auto it = weekDays.begin(); it != weekDays.end(); ++it) {
        bool ok = false;
        const int day = it.key().toInt(&ok);
        if (ok && day >= 1 && day <= 7) {
            snapshot.weekdays[day - 1] = it.value().toObject().value(QStringLiteral("count")).toInt();
        }
    }
}

void StatsFmClient::readDays(const QJsonObject &perDay, StatsSnapshot &snapshot)
{
    const QJsonObject days = perDay.value(QStringLiteral("days")).toObject();
    for (auto it = days.begin(); it != days.end(); ++it) {
        const QDate day = localDay(it.key());
        const int count = it.value().toObject().value(QStringLiteral("count")).toInt();
        if (day.isValid() && count > 0) {
            snapshot.daily[day] += count;
        }
    }
}

QList<StatEntry> StatsFmClient::albumsFromTracks(const QList<StatEntry> &tracks)
{
    QHash<QString, StatEntry> albums;
    QStringList order;
    for (const StatEntry &track : tracks) {
        const Song &song = track.song;
        const QString key = song.albumId.isEmpty() ? song.albumName : song.albumId;
        if (key.isEmpty()) {
            continue;
        }
        auto found = albums.find(key);
        if (found == albums.end()) {
            StatEntry entry;
            entry.key = key;
            entry.name = song.albumName;
            entry.subtitle = song.artistItems.isEmpty() ? song.artists : song.artistItems.first().name;
            entry.artworkUrl = song.artworkUrl;
            entry.album = song.albumDestination();
            entry.album.name = song.albumName;
            entry.album.artists = entry.subtitle;
            entry.album.artworkUrl = song.artworkUrl;
            entry.statsFmId = track.statsFmAlbumId;
            // unknown until a song of it brings a count, or its own stats do
            entry.plays = -1;
            found = albums.insert(key, entry);
            order.append(key);
        }
        if (track.plays >= 0) {
            found->plays = std::max(0, found->plays) + track.plays;
            found->listenedMs += track.listenedMs;
        }
    }
    QList<StatEntry> list;
    for (const QString &key : std::as_const(order)) {
        list.append(albums.value(key));
    }
    std::stable_sort(list.begin(), list.end(), [](const StatEntry &a, const StatEntry &b) { return a.plays > b.plays; });
    return list;
}

} // namespace swiftify
