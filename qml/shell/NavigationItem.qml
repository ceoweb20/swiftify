pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

// a navigation drawer row: material 3's pill indicator behind the selected
// one, opening from the middle and going wavy under the pointer, its
// icon popping onto a shape of its own; a state layer on hover, and room for
// a trailing control.
T.AbstractButton {
    id: root

    property string symbol
    property url artworkUrl
    property string fallbackKey: text
    property bool selected: false
    property bool showArtwork: false
    /// the shape the icon sits on while selected
    property string iconShape: "cookie9"
    /// keeps its shape with no shapes: the favourite songs' heart
    property bool keepsShape: false
    property alias trailing: trailingSlot.data
    property int indent: 0

    implicitHeight: 40
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    background: Item {
        // the state layer of an unselected row
        Rectangle {
            anchors.fill: parent
            radius: Appearance.rounding.full
            color: !root.selected && root.down ? Appearance.colors.layer1Active
                 : Qt.alpha(Appearance.colors.layer1Hover, !root.selected && root.hovered ? 1 : 0)

            Behavior on color {
                ElementMoveFastColor {}
            }
        }

        // the selected pill. it opens from the middle on a quick ease that
        // stops at the row's edges: the row is as wide as the drawer, and a
        // spring past its width carried the pill out of the drawer
        WavyClip {
            id: pill

            property real openness: root.selected ? 1 : 0

            anchors.centerIn: parent
            width: parent.width * (0.35 + 0.65 * Math.min(openness, 1))
            height: parent.height
            opacity: root.selected ? 1 : 0
            visible: opacity > 0
            radius: height / 2
            wavy: root.selected && root.hovered
            depth: 2
            wavelength: 14

            Behavior on openness {
                enabled: !Appearance.performanceMode
                ElementMove {}
            }
            Behavior on opacity {
                NumberAnimation { duration: Appearance.duration(160); easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: root.down ? Appearance.colors.secondaryContainerActive
                     : root.hovered ? Appearance.colors.secondaryContainerHover : Appearance.colors.secondaryContainer

                Behavior on color {
                    ElementMoveFastColor {}
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: root.visualFocus
            radius: Appearance.rounding.full
            color: "transparent"
            border.width: 2
            border.color: Appearance.colors.secondary
        }
    }

    contentItem: Item {
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16 + root.indent
            anchors.right: trailingSlot.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            Item {
                id: iconSlot
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.showArtwork
                width: 32
                height: 32

                MaterialSymbol {
                    anchors.centerIn: parent
                    Reveal.shown: !root.selected
                    text: root.symbol
                    iconSize: 22
                    weight: root.hovered ? 600 : 400
                    color: Appearance.colors.onSurfaceVariant
                }
                ShapeIcon {
                    objectName: "selectedNavigationIcon"
                    anchors.centerIn: parent
                    Reveal.shown: root.selected
                    Reveal.overshoot: 2.4
                    size: 30
                    iconSize: 18
                    fill: 0
                    rotateOnHover: false
                    shape: root.iconShape
                    keepsShape: root.keepsShape
                    symbol: root.symbol
                    color: Appearance.colors.primary
                    iconColor: Appearance.colors.onPrimary
                    hovered: root.hovered
                }
            }

            ArtworkImage {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.showArtwork
                width: 24
                height: 24
                radius: 5
                source: root.artworkUrl
                fallbackKey: root.fallbackKey
            }

            StyledText {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - (root.showArtwork ? 24 : iconSlot.width) - parent.spacing)
                text: root.text
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.small
                font.weight: root.selected ? Font.DemiBold : Font.Medium
                color: root.selected ? Appearance.colors.onSecondaryContainer : Appearance.colors.onSurface
            }
        }

        Row {
            id: trailingSlot
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
