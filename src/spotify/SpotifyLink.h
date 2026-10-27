#pragma once

#include <QString>

#include <optional>

namespace swiftify {

struct SpotifyLink {
    enum class Kind { Track, Album, Playlist, Artist };

    Kind kind;
    QString id;
};

/// Nullopt for anything that is not an open.spotify.com URL or spotify: URI of a track, album, playlist or artist
std::optional<SpotifyLink> parseSpotifyLink(const QString &input);

} // namespace swiftify
