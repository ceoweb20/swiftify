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

// one colour theme in the settings: a little window in its colours, with a
// page, two lines of text and its three accents, and its name beneath. the
// chosen one wears a ring and a tick that springs in.
RippleButton {
    id: root

    required property themeOption option
    property bool selected: false
    readonly property bool matugen: option.id === "system"

    implicitWidth: 124
    implicitHeight: 104
    padding: 0
    buttonRadius: Appearance.rounding.normal
    colBackground: "transparent"
    // the button's own spring carries it
    scale: down ? 0.94 : (hovered ? 1.04 : 1)

    component Dot: MaterialShape {
        width: 13
        height: 13
    }

    contentItem: Column {
        spacing: 6

        Item {
            width: root.width
            height: 76

            // Matugen's own accents glow around its preview, even before selection.
            Loader {
                anchors.fill: parent
                active: root.matugen && !Appearance.performanceMode
                sourceComponent: Item {
                    opacity: root.selected ? 0.65 : (root.hovered ? 0.55 : 0.35)

                    Behavior on opacity {
                        enabled: !Appearance.reduceMotion
                        ElementMoveFast {}
                    }

                    SoftShadow {
                        anchors.fill: parent
                        radius: 16
                        blur: 14
                        offset: Qt.vector2d(-3, 0)
                        color: root.option.primary
                    }
                    SoftShadow {
                        anchors.fill: parent
                        radius: 16
                        blur: 12
                        offset: Qt.vector2d(3, 0)
                        color: Qt.rgba(root.option.tertiary.r, root.option.tertiary.g,
                                       root.option.tertiary.b, 0.6)
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: 16
                color: root.option.window
                border.width: root.selected ? 3 : 1
                border.color: root.selected ? Appearance.colors.primary
                                            : root.matugen ? Qt.rgba(root.option.primary.r, root.option.primary.g, root.option.primary.b, 0.65)
                                            : Qt.rgba(root.option.text.r, root.option.text.g, root.option.text.b, 0.14)

                Behavior on border.width {
                    ElementMoveFast {}
                }

                // the sidebar's entries
                Column {
                    x: 9
                    y: 12
                    spacing: 5

                    Repeater {
                        model: 3
                        delegate: Rectangle {
                            width: 10
                            height: 4
                            radius: 2
                            color: Qt.rgba(root.option.text.r, root.option.text.g, root.option.text.b, 0.35)
                        }
                    }
                }

                Rectangle {
                    x: 26
                    y: 8
                    width: parent.width - 34
                    height: parent.height - 16
                    radius: 9
                    color: root.option.page

                    Rectangle {
                        x: 8
                        y: 9
                        width: parent.width * 0.62
                        height: 5
                        radius: 2.5
                        color: root.option.text
                        opacity: 0.85
                    }
                    Rectangle {
                        x: 8
                        y: 19
                        width: parent.width * 0.4
                        height: 5
                        radius: 2.5
                        color: root.option.text
                        opacity: 0.45
                    }
                    Row {
                        x: 8
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        spacing: 4

                        Dot { shape: "heart"; color: root.option.primary }
                        Dot { shape: "clover4"; color: root.option.secondary }
                        Dot { shape: "cookie9"; color: root.option.tertiary }
                    }
                }
            }

            MaterialShape {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: -6
                width: 26
                height: 26
                shape: "cookie9"
                color: Appearance.colors.primary
                spinAngle: root.selected ? 0 : -90
                scale: root.selected ? 1 : 0
                visible: scale > 0.01

                Behavior on scale {
                    enabled: !Appearance.performanceMode
                    SpringAnimation { spring: 5; damping: 0.28; epsilon: 0.005 }
                }

                MaterialSymbol {
                    anchors.centerIn: parent
                    text: "check"
                    iconSize: 16
                    weight: 700
                    color: Appearance.colors.onPrimary
                }
            }
        }

        StyledText {
            width: root.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTranslate("Themes", root.option.name)
            elide: Text.ElideRight
            font.pixelSize: Appearance.fontSize.smaller
            font.weight: root.selected ? Font.DemiBold : Font.Normal
            color: root.selected ? Appearance.colors.primary : Appearance.colors.onSurface
        }
    }
}
