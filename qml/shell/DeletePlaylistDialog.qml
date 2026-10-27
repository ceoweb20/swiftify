pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

StyledDialog {
    id: root

    property playlist playlist

    function ask(target: playlist) {
        playlist = target
        open()
    }

    headline: qsTr("Delete “%1”?").arg(playlist.name)
symbol: "delete"
iconShape: "ghostish"
    body: qsTr("This removes the playlist from your Spotify library.")

    buttons: [
        TextButton {
            text: qsTr("Delete Playlist")
            colBackground: Appearance.colors.error
            colBackgroundHover: Appearance.colors.errorHover
            labelColor: Appearance.colors.onError
            onClicked: {
                Library.deletePlaylist(root.playlist)
                root.accept()
            }
        },
        TextButton {
            variant: TextButton.Text
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    ]
}
