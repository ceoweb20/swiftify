pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

T.AbstractButton {
    id: root

    property song song
    property bool current: false
    property MaterialScheme scheme: Appearance.colors

    implicitHeight: 54
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    padding: 6
    leftPadding: 10
    rightPadding: 10
    enabled: !current

    background: Rectangle {
        radius: 10
        color: root.hovered ? root.scheme.layer1Hover : "transparent"
    }

    contentItem: Item {
        opacity: root.song.isPlayable ? 1 : 0.38

        ArtworkImage {
            id: art
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 42
            height: 42
            radius: 6
            source: root.song.artworkUrl
        }

        Column {
            anchors.left: art.right
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            StyledText {
                width: parent.width
                text: root.song.name
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.smallie
                font.weight: root.current ? Font.DemiBold : Font.Normal
                color: root.current ? root.scheme.primary : root.scheme.onSurface
            }
            StyledText {
                width: parent.width
                text: root.song.artists
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.smaller
                color: root.scheme.onSurfaceVariant
            }
        }
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
