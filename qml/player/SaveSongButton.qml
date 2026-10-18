pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// The same Spotify saved-song state as track rows and their context menu.
IconButton {
    id: root

    property song song: Player.currentSong
    readonly property bool saved: Library.savedRevision >= 0 && Library.isSaved(song.uri)
    readonly property bool pending: Library.savedRevision >= 0 && Library.isSavePending(song.uri)

    visible: song.valid && !song.isLocalFile && song.uri.startsWith("spotify:track:")
    enabled: visible && !pending
    symbol: "favorite"
    toggled: saved
    tooltip: pending ? qsTr("Updating saved song…") : saved ? qsTr("Remove from Songs") : qsTr("Save to Songs")
    iconColor: saved ? scheme.primary : scheme.onSurfaceVariant
    onClicked: Library.toggleSaved(song)
}
