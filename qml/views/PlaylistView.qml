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
        title: qsTr("Playlist Unavailable")
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
        emptyTitle: Library.playlistSongs.count === 0 ? qsTr("No Songs") : qsTr("No Results for “%1”").arg(Library.playlistFilterQuery.trim())
        emptyDescription: Library.playlistSongs.count > 0 ? qsTr("Check the spelling or try a new search.") : root.playlist.isLocal ? qsTr("Add tracks from anywhere on your computer, or drop them here.") : qsTr("Add songs from Search using the More menu.")

        MusicPicker {
            id: trackPicker
            playlistId: root.playlist.isLocal ? root.playlist.id : ""
        }

        hero: CollectionHero {
            artworkUrl: root.playlist.artworkUrl
            fallbackKey: root.playlist.id.length > 0 ? root.playlist.id : root.playlist.name
            kind: root.playlist.isLocal ? "LOCAL PLAYLIST" : "PLAYLIST"
            title: root.playlist.name
            description: root.playlist.description
            facts: Format.joined([root.playlist.ownerDisplayName,
                Library.playlistSongs.count > 0 ? Format.songCount(Library.playlistSongs.count)
                    : root.playlist.songCount > 0 ? Format.songCount(root.playlist.songCount) : "",
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
                    onClicked: Player.playFromModel(Library.displayedPlaylistSongs, 0)
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
                    text: qsTr("Add Tracks")
                    busy: LocalFiles.copying
                    onClicked: trackPicker.pickFiles()
                }
            ]
        }
    }
}
