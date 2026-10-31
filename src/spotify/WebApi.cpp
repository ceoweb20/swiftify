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

#include "spotify/WebApi.h"

#include "spotify/SpotifyError.h"
#include "spotify/SpotifyJson.h"
#include "spotify/PlaylistLinks.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QCoroNetworkReply>

#include <algorithm>
#include <exception>
#include <memory>
#include <vector>

namespace swiftify {

namespace {

QByteArray jsonBody(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

WebApi::WebApi(QNetworkAccessManager *network)
    : m_network(network)
{
}

QUrl WebApi::apiUrl(const QString &path, const QUrlQuery &query)
{
    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(QStringLiteral("api.spotify.com"));
    url.setPath(path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    return url;
}

QCoro::Task<QByteArray> WebApi::requestData(QUrl url, QByteArray method, QString accessToken,
                                            QByteArray body, QByteArray contentType)
{
    if (url.scheme() != QLatin1StringView("https") || url.host() != QLatin1StringView("api.spotify.com")
        || !url.userInfo().isEmpty() || (url.port(-1) != -1 && url.port() != 443)) {
        throw SpotifyError(SpotifyError::Kind::InvalidUrl, QStringLiteral("Spotify returned an invalid API URL."));
    }
    if (!m_rateLimitUntil.hasExpired()) {
        const int seconds = int((m_rateLimitUntil.remainingTime() + 999) / 1000);
        throw SpotifyError(SpotifyError::Kind::RateLimited,
                           QStringLiteral("Spotify rate limit exceeded. Try again in %1 seconds.").arg(seconds), seconds, 429);
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("User-Agent", "Swiftify");
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());
    request.setTransferTimeout(30'000);
    if (!body.isEmpty() || !contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType.isEmpty() ? QByteArray("application/json") : contentType);
    }

    std::unique_ptr<QNetworkReply> reply(m_network->sendCustomRequest(request, method, body));
    co_await qCoro(reply.get()).waitForFinished();

    const QByteArray data = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0) {
        throw SpotifyError::requestFailed(reply->errorString());
    }
    if (status >= 200 && status < 300) {
        co_return data;
    }

    QString message = QJsonDocument::fromJson(data).object().value(QStringLiteral("error")).toObject()
                          .value(QStringLiteral("message")).toString();
    if (message.isEmpty()) {
        message = QStringLiteral("HTTP %1").arg(status);
    }
    switch (status) {
    case 401:
        throw SpotifyError(SpotifyError::Kind::Unauthorized,
                           QStringLiteral("Spotify API authorization failed: %1").arg(message), -1, status);
    case 403:
        // spotify's own text here is usually the bare word "forbidden", which
        // says nothing about which permission was missing.
        throw SpotifyError(SpotifyError::Kind::Forbidden,
                           QStringLiteral("Spotify refused this change: %1").arg(message), -1, status);
    case 429: {
        bool ok = false;
        const int retryAfter = reply->rawHeader("Retry-After").toInt(&ok);
        ok = ok && retryAfter >= 0;
        // One 429 pauses this client's entire Web API, not just the failed
        // endpoint. Other background loads must not keep extending the ban.
        const qint64 delayMs = qint64(ok ? std::max(1, retryAfter) : 60) * 1000;
        if (delayMs > m_rateLimitUntil.remainingTime()) {
            m_rateLimitUntil.setRemainingTime(delayMs);
        }
        throw SpotifyError(SpotifyError::Kind::RateLimited,
                           ok ? QStringLiteral("Spotify rate limit exceeded. Try again in %1 seconds.").arg(retryAfter)
                              : QStringLiteral("Spotify rate limit exceeded. Try again shortly."),
                           ok ? retryAfter : -1, status);
    }
    default:
        throw SpotifyError::requestFailed(message, status);
    }
}

QCoro::Task<QJsonDocument> WebApi::requestJson(QUrl url, QByteArray method, QString accessToken,
                                               QByteArray body, QByteArray contentType)
{
    const QByteArray data = co_await requestData(url, method, accessToken, body, contentType);
    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        throw SpotifyError::invalidData(error.errorString());
    }
    co_return document;
}

QCoro::Task<> WebApi::collectPages(QUrl firstUrl, QString accessToken,
                                   std::function<void(const QJsonObject &)> onPage)
{
    QSet<QUrl> visited;
    QUrl next = firstUrl;
    while (next.isValid() && !next.isEmpty()) {
        if (visited.contains(next)) {
            throw SpotifyError::invalidData(QStringLiteral("Spotify repeated a page URL."));
        }
        visited.insert(next);
        const QJsonObject page = (co_await requestJson(next, "GET", accessToken)).object();
        onPage(page);
        const QJsonValue nextValue = page.value(QStringLiteral("next"));
        next = nextValue.isString() ? QUrl(nextValue.toString()) : QUrl();
    }
}

QCoro::Task<QJsonObject> WebApi::requestPage(QUrl url, QString accessToken)
{
    co_return (co_await requestJson(url, "GET", accessToken)).object();
}

QCoro::Task<> WebApi::collectOffsetPages(QString path, QUrlQuery query, QString accessToken,
                                         std::function<void(const QJsonObject &)> onPage)
{
    constexpr int kPageSize = 50;
    constexpr int kAtOnce = 4;
    query.removeAllQueryItems(QStringLiteral("limit"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(kPageSize));
    const auto pageUrl = [&](int offset) {
        QUrlQuery paged = query;
        paged.addQueryItem(QStringLiteral("offset"), QString::number(offset));
        return apiUrl(path, paged);
    };

    const QUrl firstUrl = pageUrl(0);
    const QJsonObject first = co_await requestPage(firstUrl, accessToken);
    onPage(first);
    const int total = first.value(QStringLiteral("total")).toInt();

    for (int start = kPageSize; start < total; start += kPageSize * kAtOnce) {
        // started together, then each awaited in turn, every one to the end
        // even when one fails: a request is not left running unowned
        std::vector<QCoro::Task<QJsonObject>> batch;
        for (int offset = start; offset < std::min(total, start + kPageSize * kAtOnce); offset += kPageSize) {
            batch.push_back(requestPage(pageUrl(offset), accessToken));
        }
        std::vector<QJsonObject> pages;
        std::exception_ptr failure;
        for (QCoro::Task<QJsonObject> &task : batch) {
            try {
                pages.push_back(co_await task);
            } catch (...) {
                if (!failure) {
                    failure = std::current_exception();
                }
            }
        }
        if (failure) {
            std::rethrow_exception(failure);
        }
        for (const QJsonObject &page : pages) {
            onPage(page);
        }
    }
}

QCoro::Task<Profile> WebApi::profile(QString accessToken)
{
    const QJsonDocument document = co_await requestJson(apiUrl(QStringLiteral("/v1/me")), "GET", accessToken);
    const Profile result = json::profile(document.object());
    if (!result.isValid()) {
        throw SpotifyError::invalidData(QStringLiteral("the profile had no id."));
    }
    co_return result;
}

QCoro::Task<QList<Playlist>> WebApi::playlists(QString accessToken)
{
    QList<Playlist> result;
    const QUrlQuery requestQuery = QUrlQuery {{QStringLiteral("limit"), QStringLiteral("50")}};
    co_await collectPages(apiUrl(QStringLiteral("/v1/me/playlists"), requestQuery),
                          accessToken, [&result](const QJsonObject &page) {
        for (const QJsonValue &value : page.value(QStringLiteral("items")).toArray()) {
            if (value.isObject()) {
                const Playlist playlist = json::playlist(value.toObject());
                if (playlist.isValid()) {
                    result.append(playlist);
                }
            }
        }
    });
    co_return result;
}

namespace {

/// the account's own market, so spotify says which tracks it will play there
const std::pair<QString, QString> kMarket {QStringLiteral("market"), QStringLiteral("from_token")};

void appendWrappedTracks(QList<Song> &songs, const QJsonObject &page)
{
    for (const QJsonValue &value : page.value(QStringLiteral("items")).toArray()) {
        if (const auto song = json::wrappedTrack(value.toObject())) {
            songs.append(*song);
        }
    }
}

} // namespace

QCoro::Task<QList<Song>> WebApi::playlistSongs(QString playlistId, QString accessToken)
{
    QList<Song> result;
    const QUrlQuery query {{QStringLiteral("limit"), QStringLiteral("50")},
                           {QStringLiteral("additional_types"), QStringLiteral("track")},
                           kMarket};
    co_await collectOffsetPages(QStringLiteral("/v1/playlists/%1/items").arg(playlistId), query, accessToken,
                                [&result](const QJsonObject &page) { appendWrappedTracks(result, page); });
    co_return result;
}


QCoro::Task<QList<Song>> WebApi::savedSongs(QString accessToken)
{
    QList<Song> result;
    const QUrlQuery requestQuery {kMarket};
    co_await collectOffsetPages(QStringLiteral("/v1/me/tracks"), requestQuery, accessToken,
                                [&result](const QJsonObject &page) { appendWrappedTracks(result, page); });
    co_return result;
}

QCoro::Task<int> WebApi::savedSongTotal(QString accessToken)
{
    const QUrlQuery requestQuery = QUrlQuery {{QStringLiteral("limit"), QStringLiteral("1")}};
    const QJsonDocument document = co_await requestJson(
        apiUrl(QStringLiteral("/v1/me/tracks"), requestQuery),
        "GET", accessToken);
    co_return document.object().value(QStringLiteral("total")).toInt();
}

QCoro::Task<QList<Song>> WebApi::recentlyPlayed(QString accessToken)
{
    const RecentPlayback history = co_await recentHistory(accessToken);
    co_return history.songs;
}

QCoro::Task<QList<Song>> WebApi::savedSongSample(QString accessToken)
{
    const auto document = co_await requestJson(apiUrl(QStringLiteral("/v1/me/tracks"),
        QUrlQuery{{QStringLiteral("limit"), QStringLiteral("50")}, kMarket}), "GET", accessToken);
    QList<Song> result;
    appendWrappedTracks(result, document.object());
    co_return result;
}

QCoro::Task<RecentPlayback> WebApi::recentHistory(QString accessToken, int limit)
{
    const QUrlQuery requestQuery {{QStringLiteral("limit"), QString::number(std::clamp(limit, 1, 50))}};
    const QJsonDocument document = co_await requestJson(
        apiUrl(QStringLiteral("/v1/me/player/recently-played"), requestQuery),
        "GET", accessToken);
    RecentPlayback result;
    appendWrappedTracks(result.songs, document.object());
    result.playlistIds = playlistlinks::recentIds(document.object());
    co_return result;
}

QCoro::Task<SearchResults> WebApi::search(QString query, QString accessToken)
{
    const QUrlQuery requestQuery = QUrlQuery {{QStringLiteral("q"), query},
                                                       {QStringLiteral("type"), QStringLiteral("track,album,artist,playlist")},
                                                       {QStringLiteral("limit"), QStringLiteral("10")},
                                                       kMarket};
    const QJsonObject response = (co_await requestJson(
        apiUrl(QStringLiteral("/v1/search"), requestQuery),
        "GET", accessToken)).object();

    const auto items = [&response](const char *key) {
        return response.value(QLatin1StringView(key)).toObject().value(QStringLiteral("items")).toArray();
    };
    SearchResults results;
    for (const QJsonValue &value : items("tracks")) {
        if (const auto song = json::track(value.toObject())) results.songs.append(*song);
    }
    for (const QJsonValue &value : items("albums")) {
        if (const auto album = json::album(value.toObject())) results.albums.append(*album);
    }
    for (const QJsonValue &value : items("artists")) {
        if (const auto artist = json::artist(value.toObject())) results.artists.append(*artist);
    }
    for (const QJsonValue &value : items("playlists")) {
        if (value.isObject()) {
            const Playlist playlist = json::playlist(value.toObject());
            if (playlist.isValid()) results.playlists.append(playlist);
        }
    }
    co_return results;
}

QCoro::Task<Song> WebApi::song(QString trackId, QString accessToken)
{
    const QJsonDocument document = co_await requestJson(apiUrl(QStringLiteral("/v1/tracks/%1").arg(trackId), QUrlQuery {kMarket}), "GET", accessToken);
    const auto song = json::track(document.object());
    if (!song) {
        throw SpotifyError::invalidData(QStringLiteral("Spotify returned an unsupported track."));
    }
    co_return *song;
}

QCoro::Task<Album> WebApi::album(QString albumId, QString accessToken)
{
    const QJsonDocument document = co_await requestJson(apiUrl(QStringLiteral("/v1/albums/%1").arg(albumId), QUrlQuery {kMarket}), "GET", accessToken);
    const auto album = json::album(document.object());
    if (!album) {
        throw SpotifyError::invalidData(QStringLiteral("Spotify returned an unsupported album."));
    }
    co_return *album;
}


QCoro::Task<Playlist> WebApi::playlist(QString playlistId, QString accessToken)
{
    if (!playlistlinks::validId(playlistId)) {
        throw SpotifyError(SpotifyError::Kind::InvalidUrl, QStringLiteral("Invalid Spotify playlist ID."));
    }
    // json::playlist needs both "items" and "tracks"
    const QUrlQuery query {{QStringLiteral("fields"),
                            QStringLiteral("id,name,description,images,uri,public,collaborative,"
                                           "owner(id,display_name),items(total),tracks(total)")},
                           kMarket};
    const auto object = (co_await requestJson(apiUrl(QStringLiteral("/v1/playlists/%1").arg(playlistId), query),
                                            "GET", accessToken)).object();
    Playlist result = json::playlist(object);
    if (result.id != playlistId || result.name.trimmed().isEmpty()) {
        throw SpotifyError::invalidData(QStringLiteral("Spotify returned an unsupported playlist."));
    }
    result.uri = QStringLiteral("spotify:playlist:") + playlistId;
    co_return result;
}

QCoro::Task<QList<Song>> WebApi::albumSongs(Album album, QString accessToken)
{
    QList<Song> result;
    const QUrlQuery requestQuery = QUrlQuery {{QStringLiteral("limit"), QStringLiteral("50")}, kMarket};
    co_await collectPages(apiUrl(QStringLiteral("/v1/albums/%1/tracks").arg(album.id), requestQuery),
                          accessToken, [&result, &album](const QJsonObject &page) {
        for (const QJsonValue &value : page.value(QStringLiteral("items")).toArray()) {
            if (const auto song = json::track(value.toObject(), album)) result.append(*song);
        }
    });
    co_return result;
}

QCoro::Task<Artist> WebApi::artist(QString artistId, QString accessToken)
{
    const QString id = QString::fromLatin1(QUrl::toPercentEncoding(artistId));
    const auto object = (co_await requestJson(apiUrl(QStringLiteral("/v1/artists/%1").arg(id)),
                                            "GET", accessToken)).object();
    const auto result = json::artist(object);
    if (!result || result->id != artistId || result->name.trimmed().isEmpty()) {
        throw SpotifyError::invalidData(QStringLiteral("Spotify returned an unsupported artist."));
    }
    co_return *result;
}

QCoro::Task<QList<Album>> WebApi::artistAlbums(Artist artist, QString accessToken)
{
    QList<Album> result;
    QSet<QString> seen;
    const QUrlQuery query {{QStringLiteral("include_groups"), QStringLiteral("album,single")},
                           {QStringLiteral("limit"), QStringLiteral("10")}};
    co_await collectPages(apiUrl(QStringLiteral("/v1/artists/%1/albums").arg(artist.id), query), accessToken,
                          [&result, &seen](const QJsonObject &page) {
        for (const QJsonValue &value : page.value(QStringLiteral("items")).toArray()) {
            const auto album = json::album(value.toObject());
            if (album && !seen.contains(album->id)) {
                seen.insert(album->id);
                result.append(*album);
            }
        }
    });
    co_return result;
}

QCoro::Task<QHash<QString, QStringList>> WebApi::artistGenres(QStringList artistIds, QString accessToken,
                                                              QHash<QString, QUrl> *artwork)
{
    QHash<QString, QStringList> result;
    // fifty at a time is as many as spotify takes
    for (qsizetype start = 0; start < artistIds.size(); start += 50) {
        const QUrlQuery query {{QStringLiteral("ids"), artistIds.mid(start, 50).join(u',')}};
        const QJsonDocument document = co_await requestJson(apiUrl(QStringLiteral("/v1/artists"), query), "GET", accessToken);
        for (const QJsonValue &value : document.object().value(QStringLiteral("artists")).toArray()) {
            const QJsonObject object = value.toObject();
            const QString id = object.value(QStringLiteral("id")).toString();
            if (id.isEmpty()) {
                continue;
            }
            QStringList genres;
            for (const QJsonValue &genre : object.value(QStringLiteral("genres")).toArray()) {
                genres.append(genre.toString());
            }
            result.insert(id, genres);
            if (artwork) {
                const QUrl image = json::preferredArtworkUrl(object.value(QStringLiteral("images")).toArray(), 320);
                if (image.isValid()) {
                    artwork->insert(id, image);
                }
            }
        }
    }
    co_return result;
}

QCoro::Task<Playlist> WebApi::createPlaylist(QString name, QString description, bool isPublic, QString accessToken)
{
    const QByteArray requestBody = jsonBody({{QStringLiteral("name"), name}, {QStringLiteral("description"), description}, {QStringLiteral("public"), isPublic}});
    const QJsonDocument document = co_await requestJson(
        apiUrl(QStringLiteral("/v1/me/playlists")), "POST", accessToken,
        requestBody);
    const Playlist playlist = json::playlist(document.object());
    if (!playlist.isValid()) {
        throw SpotifyError::invalidData(QStringLiteral("the new playlist had no id."));
    }
    co_return playlist;
}

QCoro::Task<> WebApi::updatePlaylist(QString id, QString name, QString description, bool isPublic, QString accessToken)
{
    const QByteArray requestBody = jsonBody({{QStringLiteral("name"), name}, {QStringLiteral("description"), description}, {QStringLiteral("public"), isPublic}});
    co_await requestData(apiUrl(QStringLiteral("/v1/playlists/%1").arg(id)), "PUT", accessToken,
                         requestBody);
}

QCoro::Task<> WebApi::addToPlaylist(QString songUri, QString playlistId, QString accessToken)
{
    const QByteArray requestBody = jsonBody({{QStringLiteral("uris"), QJsonArray {songUri}}});
    co_await requestData(apiUrl(QStringLiteral("/v1/playlists/%1/items").arg(playlistId)), "POST", accessToken,
                         requestBody);
}

QCoro::Task<> WebApi::removeFromPlaylist(QString songUri, QString playlistId, QString accessToken)
{
    const QByteArray requestBody = jsonBody({{QStringLiteral("items"), QJsonArray {QJsonObject {{QStringLiteral("uri"), songUri}}}}});
    co_await requestData(apiUrl(QStringLiteral("/v1/playlists/%1/items").arg(playlistId)), "DELETE", accessToken,
                         requestBody);
}

QCoro::Task<> WebApi::reorderPlaylist(QString playlistId, int rangeStart, int insertBefore, int length, QString accessToken)
{
    const QByteArray requestBody = jsonBody({{QStringLiteral("range_start"), rangeStart},
                                   {QStringLiteral("insert_before"), insertBefore},
                                   {QStringLiteral("range_length"), length}});
    co_await requestData(apiUrl(QStringLiteral("/v1/playlists/%1/items").arg(playlistId)), "PUT", accessToken,
                         requestBody);
}

QCoro::Task<> WebApi::setPlaylistImage(QString playlistId, QByteArray jpeg, QString accessToken)
{
    if (jpeg.size() > kMaximumCoverBytes) {
        throw SpotifyError::requestFailed(QStringLiteral("That cover image is too large."));
    }
    co_await requestData(apiUrl(QStringLiteral("/v1/playlists/%1/images").arg(playlistId)), "PUT", accessToken,
                         jpeg.toBase64(), "image/jpeg");
}

QCoro::Task<> WebApi::deletePlaylist(QString id, QString accessToken)
{
    co_await setSaved(false, QStringLiteral("spotify:playlist:%1").arg(id), accessToken);
}

QCoro::Task<> WebApi::setSaved(bool saved, QString uri, QString accessToken)
{
    const QUrlQuery requestQuery = QUrlQuery {{QStringLiteral("uris"), uri}};
    co_await requestData(apiUrl(QStringLiteral("/v1/me/library"), requestQuery),
                         saved ? QByteArray("PUT") : QByteArray("DELETE"), accessToken);
}

QCoro::Task<QHash<QString, bool>> WebApi::savedStatuses(QStringList uris, QString accessToken)
{
    uris.removeDuplicates();
    uris = uris.mid(0, 40);
    QHash<QString, bool> result;
    if (uris.isEmpty()) {
        co_return result;
    }
    const QUrlQuery requestQuery = QUrlQuery {{QStringLiteral("uris"), uris.join(u',')}};
    const QJsonArray statuses = (co_await requestJson(
        apiUrl(QStringLiteral("/v1/me/library/contains"), requestQuery),
        "GET", accessToken)).array();
    for (qsizetype index = 0; index < uris.size() && index < statuses.size(); ++index) {
        result.insert(uris[index], statuses[index].toBool());
    }
    co_return result;
}

} // namespace swiftify
