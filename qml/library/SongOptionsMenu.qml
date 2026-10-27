pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// SongOptionsContent: save, go to album or artist, queue, and add to
// or remove from a playlist. one instance per window, opened at the pointer.
StyledMenu {
    id: root

    property song song
    property bool allowsPlaylistRemoval: false
    readonly property bool saved: Library.savedRevision >= 0 && Library.isSaved(song.uri)
    property var editablePlaylists: []

    function openFor(target: song, allowsRemoval: bool) {
        song = target
        allowsPlaylistRemoval = allowsRemoval
        editablePlaylists = Library.editablePlaylists()
        popup()
    }

    StyledMenuItem {
        id: saveItem
        text: root.saved ? qsTr("Remove from Songs") : qsTr("Save to Songs")
        symbol: root.saved ? "star" : "star"
        onTriggered: {
            if (!root.saved) {
                UiState.burst(saveItem, "hearts")
            }
            Library.toggleSaved(root.song)
        }
    }
    StyledMenuSeparator {}
    StyledMenuItem {
        visible: root.song.albumName.length > 0
        height: visible ? implicitHeight : 0
        text: qsTr("Go to Album")
        symbol: "album"
        onTriggered: UiState.goToAlbum(root.song)
    }
    StyledMenuItem {
        visible: root.song.artists.length > 0
        height: visible ? implicitHeight : 0
        text: qsTr("Go to Artist")
        symbol: "person"
        onTriggered: UiState.goToArtist(root.song)
    }
    StyledMenuSeparator {}
    StyledMenuItem {
        text: qsTr("Play Next")
        symbol: "queue_play_next"
        onTriggered: Player.playNext(root.song)
    }
    StyledMenuItem {
        text: qsTr("Play Last")
        symbol: "playlist_add"
        onTriggered: Player.playLast(root.song)
    }

    StyledMenu {
        id: addMenu
        title: qsTr("Add to Playlist")

        Instantiator {
            model: root.editablePlaylists

            StyledMenuItem {
                id: playlistItem
                required property var modelData
                text: modelData.name
                onTriggered: {
                    UiState.burst(playlistItem, "playlist")
                    Library.addToPlaylist(root.song, playlistItem.modelData)
                }
            }

            onObjectAdded: (index, object) => addMenu.insertItem(index, object as StyledMenuItem)
            onObjectRemoved: (index, object) => addMenu.removeItem(object as StyledMenuItem)
        }
    }

    StyledMenuSeparator {
        visible: root.allowsPlaylistRemoval
        height: visible ? implicitHeight : 0
    }
    StyledMenuItem {
        visible: root.allowsPlaylistRemoval
        height: visible ? implicitHeight : 0
        text: qsTr("Remove from Playlist")
        symbol: "delete"
        destructive: true
        onTriggered: Library.removeFromSelectedPlaylist(root.song)
    }
}
