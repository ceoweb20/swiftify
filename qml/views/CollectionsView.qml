pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Swiftify

GridView {
    id: root
    readonly property real margin: Appearance.sizes.pageMargin
    readonly property real available: Math.max(200, width - 2 * margin + 20)
    readonly property int columns: Math.max(1, Math.floor(available / 200))
    readonly property real tile: available / columns - 20

    leftMargin: margin - 10
    rightMargin: margin - 10
    topMargin: Appearance.sizes.topBarHeight + 24
    bottomMargin: Appearance.sizes.playerBarClearance
    cellWidth: available / columns
    cellHeight: tile + 128
    boundsBehavior: Flickable.StopAtBounds
    reuseItems: true
    model: Collections.entries
    ScrollBar.vertical: StyledScrollBar { topPadding: 3 + Appearance.sizes.topBarHeight }

    Component.onCompleted: if (Collections.entries.length === 0) Collections.refresh()

    header: Column {
        x: 10
        width: root.available - 20
        spacing: 12

        Item {
            width: parent.width
            height: 44
            TitleText {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - refresh.width - 12
                text: qsTr("Collections")
                font.pixelSize: Appearance.fontSize.display
                elide: Text.ElideRight
            }
            IconButton {
                id: refresh
                anchors.right: parent.right
                symbol: "refresh"
                tooltip: qsTr("Refresh collections")
                enabled: !Collections.busy
                onClicked: Collections.refresh()
            }
        }

        StyledText {
            width: parent.width
            text: qsTr("Saved playlists, recent listening contexts and your links — not Spotify's recommendation feed.")
            wrapMode: Text.Wrap
            color: Appearance.colors.onSurfaceVariant
        }
        StyledText {
            width: parent.width
            text: qsTr("Playlist details use the public Web API. Opening a playlist uses Swiftify's existing player.")
            wrapMode: Text.Wrap
            font.pixelSize: Appearance.fontSize.smaller
            color: Appearance.colors.onSurfaceVariant
        }

        Row {
            width: parent.width
            spacing: 8
            OutlinedTextField {
                id: link
                objectName: "collectionLink"
                width: Math.max(80, parent.width - add.implicitWidth - 8)
                placeholder: qsTr("Spotify playlist link or URI")
                enabled: !Collections.busy
                onAccepted: Collections.add(text)
            }
            TextButton {
                id: add
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Add playlist link")
                symbol: "add"
                iconOnly: root.width < 640
                enabled: !Collections.busy && link.text.trim().length > 0
                onClicked: Collections.add(link.text)
                ToolTip.visible: hovered && iconOnly
                ToolTip.text: text
            }
        }

        StyledText {
            width: parent.width
            visible: Collections.busy || Collections.status.length > 0
            text: Collections.busy ? qsTr("Loading collections…") : qsTranslate("Collections", Collections.status)
            wrapMode: Text.Wrap
            color: Appearance.colors.onSurfaceVariant
        }
        StyledText {
            width: parent.width
            visible: !Collections.busy && Collections.entries.length === 0
            text: qsTr("No collections yet. Add a playlist link or refresh after listening in Spotify.")
            wrapMode: Text.Wrap
            color: Appearance.colors.onSurfaceVariant
        }
        Item { width: 1; height: 12 }
    }

    delegate: Item {
        id: cell
        required property var modelData
        readonly property playlist playlist: modelData.playlist
        width: root.cellWidth
        height: root.cellHeight

        MediaCard {
            x: 10
            artworkSize: root.tile
            artworkUrl: cell.playlist.artworkUrl
            fallbackKey: cell.modelData.id
            title: cell.playlist.name.length > 0 ? cell.playlist.name : qsTr("Unavailable playlist")
            subtitle: !cell.modelData.available ? qsTr("Details unavailable")
                : cell.modelData.origin === "saved" ? qsTr("Saved playlist")
                : cell.modelData.origin === "recent" ? qsTr("From listening history") : qsTr("Added by link")
            enabled: cell.modelData.available && !Collections.busy
            onClicked: Collections.open(cell.modelData.id)
        }
        StyledText {
            x: 10
            y: root.tile + 72
            width: root.tile - actions.width - 8
            text: qsTranslate("Collections", cell.modelData.status)
            elide: Text.ElideRight
            font.pixelSize: Appearance.fontSize.smallest
            color: Appearance.colors.onSurfaceVariant
            ToolTip.visible: errorHover.hovered && text.length > 0
            ToolTip.text: text
            HoverHandler { id: errorHover }
        }
        Row {
            id: actions
            x: 10 + root.tile - width
            y: root.tile + 72
            spacing: 4
            IconButton {
                size: 28
                iconSize: 18
                symbol: "open_in_new"
                tooltip: qsTr("Open in Spotify")
                onClicked: App.openUrl("https://open.spotify.com/playlist/" + cell.modelData.id)
            }
            IconButton {
                visible: cell.modelData.linked
                enabled: !Collections.busy
                size: 28
                iconSize: 18
                symbol: "link_off"
                tooltip: qsTr("Remove playlist link")
                onClicked: Collections.removeLink(cell.modelData.id)
            }
        }
    }
}
