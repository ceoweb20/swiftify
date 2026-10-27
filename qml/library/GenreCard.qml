pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Swiftify

// a genre to explore, in material 3 expressive: a tonal container tinted with
// the genre's colour, two of material's shapes along its foot, one big and
// carrying the symbol, one small, and the name over them in heavy type. hovered, the
// shapes turn and spring up and the card's edge runs to scallops, as a
// cover's does.
T.AbstractButton {
    id: root

    property string name
    property string symbol
    property color startColor
    property color endColor

    readonly property list<string> bigShapes: ["cookie9", "flower", "clover8", "softBurst", "cookie12", "puffy",
                                               "sunny", "cookie7"]
    readonly property list<string> smallShapes: ["cookie4", "clover4", "sunny", "pill", "softBurst", "diamond"]
    readonly property real hash: {
        let value = 0
        for (let index = 0; index < name.length; ++index) {
            value = (value * 31 + name.charCodeAt(index)) >>> 0
        }
        return value
    }
    /// the genre's own shapes, from its name
    readonly property string shape: bigShapes[hash % bigShapes.length]
    readonly property string accentShape: smallShapes[(hash >>> 3) % smallShapes.length]
    /// the container: the surface with a little of the genre's colour in it
    readonly property color container: Qt.tint(Appearance.colors.surfaceContainerHigh, Qt.alpha(startColor, 0.2))
    /// what sits on a shape in the genre's colour
    readonly property color ink: Qt.darker(startColor, 3)

    implicitWidth: Appearance.sizes.cardSize
    implicitHeight: 112
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    scale: down ? 0.96 : (hovered ? 1.03 : 1)

    Behavior on scale {
        enabled: !Appearance.performanceMode
        Bounce {}
    }

    background: Item {
        SoftShadow {
            anchors.fill: parent
            radius: Appearance.rounding.large
            blur: root.hovered ? 12 : 4
            offset: Qt.vector2d(0, root.hovered ? 6 : 2)
            color: Qt.rgba(0, 0, 0, root.hovered ? 0.22 : 0.1)
        }

        WavyClip {
            anchors.fill: parent
            radius: Appearance.rounding.large
            wavy: root.hovered

            Rectangle {
                anchors.fill: parent
                radius: Appearance.rounding.large
                color: root.container

                // the small shape, in the second colour, springs up when hovered
                MaterialShape {
                    x: parent.width - 124
                    y: parent.height - 46
                    width: 34
                    height: 34
                    shape: root.accentShape
                    color: root.endColor
                    scale: root.hovered ? 1.2 : 1
                    spinAngle: root.hovered ? -50 : -10

                    Behavior on scale {
                        enabled: !Appearance.performanceMode
                        Bounce {}
                    }
                    Behavior on spinAngle {
                        enabled: !Appearance.performanceMode
                        Bounce {}
                    }
                }

                // the big one is cut by the corner, with the symbol in it
                MaterialShape {
                    x: parent.width - 80
                    y: parent.height - 72
                    width: 112
                    height: 112
                    shape: root.shape
                    color: root.startColor
                    parentClipRadius: Appearance.rounding.large
                    spinAngle: root.hovered ? 40 : 6

                    Behavior on spinAngle {
                        enabled: !Appearance.performanceMode
                        Bounce {}
                    }

                    MaterialSymbol {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: -14
                        anchors.verticalCenterOffset: -14
                        text: root.symbol
                        iconSize: 36
                        fill: 1
                        weight: 600
                        rotation: root.hovered ? -8 : 0
                        color: root.ink

                        Behavior on rotation {
                            enabled: !Appearance.performanceMode
                            Bounce {}
                        }
                    }
                }
            }
        }
    }

    contentItem: Item {
        StyledText {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 16
            text: qsTranslate("Genres", root.name)
            elide: Text.ElideRight
            color: Appearance.colors.onSurface
            font.pixelSize: Appearance.fontSize.larger
            font.weight: Font.Black
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
