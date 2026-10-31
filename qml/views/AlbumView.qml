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

pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// an album (AlbumDetailView)
SongListPage {
    id: root

    /// the album it opened on. a page kept behind a newer one (PageHost)
    /// holds on to its own rather than following the library elsewhere
    property album album

    Binding {
        target: root
        property: "album"
        value: Library.currentAlbum
        when: root.visible
        restoreMode: Binding.RestoreNone
    }

    songs: Library.albumSongs
    loading: Library.loadingSongs
    loadingText: qsTr("Loading album…")
    showsEmpty: Library.albumSongs.count === 0
    emptySymbol: "album"
    emptyTitle: qsTranslate("PlaylistView", "No Songs")

    hero: CollectionHero {
        artworkUrl: root.album.artworkUrl
        kind: "ALBUM"
        title: root.album.name
        description: root.album.artists
        facts: Format.joined([root.album.releaseDate, Format.songCount(Library.albumSongs.count), Format.totalDuration(Library.albumSongs.totalDurationMs)], "  •  ")

        actions: [
            TextButton {
                iconOnly: parent ? parent.compactActions : false
                compactSize: parent ? parent.actionSize : 48
                implicitHeight: iconOnly ? compactSize : 48
                wavy: true
                symbol: "play_arrow"
                burst: "play"
                text: qsTr("Play")
                enabled: Library.albumSongs.count > 0
                onClicked: Player.playFromModel(Library.albumSongs, 0)
            }
        ]
    }
}
