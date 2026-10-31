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

// a playlist (PlaylistDetailView)
Item {
    id: root

    /// the playlist it opened on. a page kept behind a newer one (PageHost)
    /// holds on to its own rather than following the library elsewhere
    property playlist playlist

    Binding {
        target: root
        property: "playlist"
        value: Library.selectedPlaylist
        when: root.visible
        restoreMode: Binding.RestoreNone
    }
    readonly property bool filterActive: Library.playlistFilterQuery.trim().length > 0

    EmptyState {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 2 * Appearance.sizes.pageMargin)
        Reveal.shown: !root.playlist.valid
        Reveal.scaleFrom: 1
        Reveal.duration: Appearance.duration(220)
        Reveal.overshoot: 0
        symbol: "queue_music"
        title: qsTr("Playlist Unavailable")
    }

    SongListPage {
        anchors.fill: parent
        Reveal.shown: root.playlist.valid
        Reveal.scaleFrom: 1
        Reveal.duration: Appearance.duration(220)
        Reveal.overshoot: 0
        songs: Library.displayedPlaylistSongs
        loading: Library.loadingSongs
        allowsPlaylistRemoval: Library.selectedPlaylistEditable
        reorderable: Library.canReorderPlaylist
        showsEmpty: Library.playlistSongs.count === 0 || Library.displayedPlaylistSongs.count === 0
        emptySymbol: Library.playlistSongs.count === 0 ? "music_note" : "search_off"
        emptyTitle: Library.playlistSongs.count === 0 ? qsTr("No Songs") : qsTr("No Results for “%1”").arg(Library.playlistFilterQuery.trim())
        emptyDescription: Library.playlistSongs.count > 0 ? qsTr("Check the spelling or try a new search.") : qsTr("Add songs from Search using the More menu.")

        hero: CollectionHero {
            artworkUrl: root.playlist.artworkUrl
            fallbackKey: root.playlist.id.length > 0 ? root.playlist.id : root.playlist.name
            kind: "PLAYLIST"
            title: root.playlist.name
            description: root.playlist.description
            facts: Format.joined([root.playlist.ownerDisplayName,
                Library.loadingSongs && root.playlist.songCount === 0 ? "" : Format.songCount(Library.loadingSongs ? root.playlist.songCount : Library.playlistSongs.count),
                Format.totalDuration(Library.playlistSongs.totalDurationMs)], "  •  ")

            actions: [
                TextButton {
                    id: playButton
                    objectName: "playButton"
                    // a little wider than its words, and as wide as shuffle
                    // beside it, whose words are longer
                    width: iconOnly ? compactSize : Math.max(naturalWidth + 28, shuffleButton.naturalWidth)
                    iconOnly: parent ? parent.compactActions : false
                    compactSize: parent ? parent.actionSize : 48
                    implicitHeight: iconOnly ? compactSize : 48
                    wavy: true
                    symbol: "play_arrow"
                    burst: "play"
                    text: qsTr("Play")
                    enabled: Library.displayedPlaylistSongs.count > 0
                    onClicked: Player.playInOrder(Library.displayedPlaylistSongs)
                },
                // play's twin in the primary's softer tone
                TextButton {
                    id: shuffleButton
                    objectName: "shuffleButton"
                    width: playButton.width
                    iconOnly: parent ? parent.compactActions : false
                    compactSize: parent ? parent.actionSize : 48
                    implicitHeight: iconOnly ? compactSize : 48
                    wavy: true
                    symbol: "shuffle"
                    burst: "play"
                    text: qsTr("Shuffle")
                    colBackground: shuffleButton.scheme.primaryContainer
                    colBackgroundHover: shuffleButton.scheme.primaryContainerHover
                    colRipple: shuffleButton.scheme.primaryContainerActive
                    labelColor: shuffleButton.scheme.onPrimaryContainer
                    enabled: Library.displayedPlaylistSongs.count > 0
                    onClicked: Player.shufflePlay(Library.displayedPlaylistSongs)
                }
            ]
        }
    }
}
