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

#include "library/CollectionModels.h"

namespace swiftify {

QVariant PlaylistListModel::data(const QModelIndex &index, int role) const
{
    const Playlist *playlist = m_list.at(index);
    if (!playlist) {
        return {};
    }
    switch (role) {
    case PlaylistRole: return QVariant::fromValue(*playlist);
    case IdRole: return playlist->id;
    case NameRole: return playlist->name;
    case DescriptionRole: return playlist->description;
    case SongCountRole: return playlist->songCount;
    case ArtworkUrlRole: return playlist->artworkUrl;
    case OwnerNameRole: return playlist->ownerDisplayName();
    default: return {};
    }
}

QHash<int, QByteArray> PlaylistListModel::roleNames() const
{
    return {{PlaylistRole, "playlist"}, {IdRole, "playlistId"}, {NameRole, "name"}, {DescriptionRole, "description"},
            {SongCountRole, "songCount"}, {ArtworkUrlRole, "artworkUrl"}, {OwnerNameRole, "ownerName"}};
}

QVariant AlbumListModel::data(const QModelIndex &index, int role) const
{
    const Album *album = m_list.at(index);
    if (!album) {
        return {};
    }
    switch (role) {
    case AlbumRole: return QVariant::fromValue(*album);
    case IdRole: return album->id;
    case NameRole: return album->name;
    case ArtistsRole: return album->artists;
    case ArtworkUrlRole: return album->artworkUrl;
    case ReleaseDateRole: return album->releaseDate;
    default: return {};
    }
}

QHash<int, QByteArray> AlbumListModel::roleNames() const
{
    return {{AlbumRole, "album"}, {IdRole, "albumId"}, {NameRole, "name"}, {ArtistsRole, "artists"},
            {ArtworkUrlRole, "artworkUrl"}, {ReleaseDateRole, "releaseDate"}};
}

QVariant ArtistListModel::data(const QModelIndex &index, int role) const
{
    const Artist *artist = m_list.at(index);
    if (!artist) {
        return {};
    }
    switch (role) {
    case ArtistRole: return QVariant::fromValue(*artist);
    case IdRole: return artist->id;
    case NameRole: return artist->name;
    case ArtworkUrlRole: return artist->artworkUrl;
    default: return {};
    }
}

QHash<int, QByteArray> ArtistListModel::roleNames() const
{
    return {{ArtistRole, "artist"}, {IdRole, "artistId"}, {NameRole, "name"}, {ArtworkUrlRole, "artworkUrl"}};
}

} // namespace swiftify
