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

#include "spotify/SpotifyLink.h"

#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

namespace swiftify {

namespace {

std::optional<SpotifyLink::Kind> kindFromName(const QString &name)
{
    if (name == QLatin1String("track")) {
        return SpotifyLink::Kind::Track;
    }
    if (name == QLatin1String("album")) {
        return SpotifyLink::Kind::Album;
    }
    if (name == QLatin1String("playlist")) {
        return SpotifyLink::Kind::Playlist;
    }
    if (name == QLatin1String("artist")) {
        return SpotifyLink::Kind::Artist;
    }
    return std::nullopt;
}

// Only alphabet and minimum length are checked, an unknown id surfaces as a 404 from the api
std::optional<SpotifyLink> fromParts(const QString &kindName, const QString &id)
{
    static const QRegularExpression idPattern(QStringLiteral("^[A-Za-z0-9]{22,}$"));
    const auto kind = kindFromName(kindName);
    if (!kind || !idPattern.match(id).hasMatch()) {
        return std::nullopt;
    }
    return SpotifyLink{*kind, id};
}

} // namespace

std::optional<SpotifyLink> parseSpotifyLink(const QString &input)
{
    const QString text = input.trimmed();

    // Parse URI links, e.g. "spotify:track:{id}"
    if (text.startsWith(QLatin1String("spotify:"))) {
        const QStringList parts = text.split(QLatin1Char(':'));
        if (parts.size() != 3) {
            return std::nullopt;
        }
        return fromParts(parts[1], parts[2]);
    }

    // Otherwise parse URL "open.spotify.com/{kind}/{id}" links
    const QUrl url(text);
    if (url.scheme() != QLatin1String("https") && url.scheme() != QLatin1String("http")) {
        return std::nullopt;
    }
    if (url.host() != QLatin1String("open.spotify.com")) {
        return std::nullopt;
    }

    QStringList segments = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    // Remove optional locale segment, e.g. /intl-de/
    if (!segments.isEmpty() && segments.first().startsWith(QLatin1String("intl-"))) {
        segments.removeFirst();
    }
    if (segments.size() != 2) {
        return std::nullopt;
    }
    return fromParts(segments[0], segments[1]);
}

} // namespace swiftify
