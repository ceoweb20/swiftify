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
        Reveal.shown: !root.playlist.valid
        symbol: "queue_music"
        title: "Playlist Unavailable"
    }

    SongListPage {
        anchors.fill: parent
        Reveal.shown: root.playlist.valid
        songs: Library.displayedPlaylistSongs
        loading: Library.loadingSongs
        allowsPlaylistRemoval: Library.selectedPlaylistEditable
        reorderable: Library.canReorderPlaylist
        showsEmpty: Library.playlistSongs.count === 0 || Library.displayedPlaylistSongs.count === 0
        emptySymbol: Library.playlistSongs.count === 0 ? "music_note" : "search_off"
        emptyTitle: Library.playlistSongs.count === 0 ? "No Songs" : "No Results for “" + Library.playlistFilterQuery.trim() + "”"
        emptyDescription: Library.playlistSongs.count > 0 ? "Check the spelling or try a new search." : root.playlist.isLocal ? "Add tracks from anywhere on your computer, or drop them here." : "Add songs from Search using the More menu."

        MusicPicker {
            id: trackPicker
            playlistId: root.playlist.isLocal ? root.playlist.id : ""
        }

        hero: CollectionHero {
            artworkUrl: root.playlist.artworkUrl
            kind: root.playlist.isLocal ? "LOCAL PLAYLIST" : "PLAYLIST"
            title: root.playlist.name
            description: root.playlist.description
            facts: Format.joined([root.playlist.ownerDisplayName,
                Library.loadingSongs && root.playlist.songCount === 0 ? "" : Format.songCount(Library.loadingSongs ? root.playlist.songCount : Library.playlistSongs.count),
                Format.totalDuration(Library.playlistSongs.totalDurationMs)], "  •  ")

            actions: [
                TextButton {
                    id: playButton
                    // a little wider than its words, and as wide as shuffle
                    // beside it, whose words are longer
                    width: iconOnly ? compactSize : Math.max(naturalWidth + 28, shuffleButton.naturalWidth)
                    iconOnly: parent ? parent.compactActions : false
                    compactSize: parent ? parent.actionSize : 48
                    implicitHeight: iconOnly ? compactSize : 48
                    wavy: true
                    symbol: "play_arrow"
                    burst: "play"
                    text: "Play"
                    enabled: Library.playlistSongs.count > 0
                    onClicked: Player.playFromModel(Library.playlistSongs, 0)
                },
                // play's twin in the primary's softer tone
                TextButton {
                    id: shuffleButton
                    width: playButton.width
                    iconOnly: parent ? parent.compactActions : false
                    compactSize: parent ? parent.actionSize : 48
                    implicitHeight: iconOnly ? compactSize : 48
                    wavy: true
                    symbol: "shuffle"
                    burst: "play"
                    text: "Shuffle"
                    colBackground: shuffleButton.scheme.primaryContainer
                    colBackgroundHover: shuffleButton.scheme.primaryContainerHover
                    colRipple: shuffleButton.scheme.primaryContainerActive
                    labelColor: shuffleButton.scheme.onPrimaryContainer
                    enabled: Library.playlistSongs.count > 0
                    onClicked: Player.shufflePlay(Library.playlistSongs)
                },
                // a playlist made here takes tracks from anywhere: copies,
                // so the originals can move or go
                TextButton {
                    visible: root.playlist.isLocal
                    iconOnly: parent ? parent.compactActions : false
                    compactSize: parent ? parent.actionSize : 48
                    implicitHeight: iconOnly ? compactSize : 48
                    variant: TextButton.Tonal
                    symbol: "add"
                    text: "Add Tracks"
                    busy: LocalFiles.copying
                    onClicked: trackPicker.pickFiles()
                }
            ]
        }
    }
}
