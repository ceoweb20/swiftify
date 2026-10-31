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
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import Swiftify

T.Popup {
    id: root

    property int page: 0
    readonly property bool compact: width < 720
    readonly property list<string> titles: [qsTr("Visualizer"), qsTr("Background"), qsTr("Playback"), qsTr("Appearance"), qsTr("Performance"),
                                            qsTr("Account"), qsTr("About")]
    readonly property list<string> symbols: ["graphic_eq", "wallpaper", "play_circle", "palette", "speed", "account_circle", "info"]
    readonly property list<string> shapes: ["sunny", "flower", "cookie9", "clover4", "gem", "cookie12", "heart"]

    parent: T.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(980, parent ? parent.width - 32 : 980)
    height: Math.min(720, parent ? parent.height - 32 : 720)
    modal: true
    focus: true
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    padding: 0
    popupType: T.Popup.Item

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Appearance.duration(150) }
        NumberAnimation { property: "scale"; from: 0.98; to: 1; duration: Appearance.motion(260); easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Appearance.duration(120) }
    }

    T.Overlay.modal: Rectangle {
        color: Appearance.colors.scrimOverlay
    }

    background: Rectangle {
        radius: 28
        color: Appearance.colors.surfaceContainerLow
        border.width: 1
        border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.5)
    }

    contentItem: Item {
        Shortcut {
            sequences: [StandardKey.Close]
            enabled: root.opened
            onActivated: root.close()
        }

        TitleText {
            x: 26
            y: 24
            visible: !root.compact
            text: qsTr("Settings")
            font.pixelSize: Appearance.fontSize.huge
        }
        MaterialSymbol {
            x: 40
            y: 27
            visible: root.compact
            text: "tune"
            iconSize: 24
            color: Appearance.colors.primary
        }

        Flickable {
            id: railScroller
            PageWheelHandler { page: railScroller }
            objectName: "settingsRail"
            x: 16
            y: 84
            width: root.compact ? 72 : 192
            height: Math.max(0, parent.height - y - 16)
            contentHeight: rail.implicitHeight + 16
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            clip: true
            ScrollBar.vertical: StyledScrollBar {}

            Column {
                id: rail
                width: railScroller.width
                spacing: 4

                Repeater {
                    model: root.titles.length

                    delegate: RippleButton {
                        id: destination
                        required property int index
                        readonly property bool selected: root.page === index

                        width: rail.width
                        implicitHeight: 48
                        leftPadding: 10
                        rightPadding: 10
                        text: root.titles[index]
                        tooltip: root.compact ? text : ""
                        buttonRadius: 14
                        bounce: false
                        colBackground: selected ? Appearance.colors.secondaryContainer : "transparent"
                        colBackgroundHover: selected ? Appearance.colors.secondaryContainerHover : Appearance.colors.layer1Hover
                        onClicked: root.page = index

                        contentItem: Item {
                            Item {
                                id: icon
                                width: 34
                                height: 34
                                x: root.compact ? (parent.width - width) / 2 : 0
                                anchors.verticalCenter: parent.verticalCenter

                                MaterialSymbol {
                                    anchors.centerIn: parent
                                    visible: !destination.selected
                                    text: root.symbols[destination.index]
                                    iconSize: 21
                                    color: Appearance.colors.onSurfaceVariant
                                }
                                ShapeIcon {
                                    anchors.centerIn: parent
                                    visible: destination.selected
                                    size: 34
                                    iconSize: 18
                                    shape: root.shapes[destination.index]
                                    symbol: root.symbols[destination.index]
                                    color: Appearance.colors.primary
                                    iconColor: Appearance.colors.onPrimary
                                    rotateOnHover: false
                                }
                            }
                            StyledText {
                                anchors.left: icon.right
                                anchors.leftMargin: 10
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                visible: !root.compact
                                elide: Text.ElideRight
                                text: destination.text
                                font.pixelSize: Appearance.fontSize.smallie
                                font.weight: destination.selected ? Font.DemiBold : Font.Medium
                                color: destination.selected ? Appearance.colors.onSecondaryContainer : Appearance.colors.onSurfaceVariant
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            x: railScroller.x + railScroller.width + 12
            y: 20
            width: 1
            height: parent.height - 40
            color: Qt.alpha(Appearance.colors.outlineVariant, 0.35)
        }

        // each page's own faint shapes behind its heading; the band ends where
        // the cards begin, so they never sit under a card
        Item {
            x: railScroller.x + railScroller.width + 13
            width: parent.width - x
            height: scroller.y
            clip: true

            Stickers {
                width: parent.width
                height: parent.height + 24
                key: root.titles[root.page]
                count: 4
                minimumSize: 56
                maximumSize: 120
                clipRadius: 28
            }
        }

        TitleText {
            x: scroller.x
            y: 24
            width: Math.max(0, closeButton.x - x - 16)
            text: root.titles[root.page]
            elide: Text.ElideRight
            font.pixelSize: Appearance.fontSize.headline
        }

        // A single clipped viewport keeps section cards away from the sheet's
        // rounded edge and the fixed header.
        Flickable {
            id: scroller
            PageWheelHandler { page: scroller }
            objectName: "settingsContentViewport"
            x: railScroller.x + railScroller.width + 24
            y: 88
            width: parent.width - x - 24
            height: Math.max(0, parent.height - y - 24)
            contentHeight: pages.implicitHeight + 16
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: StyledScrollBar {}

            SettingsPages {
                id: pages
                objectName: "settingsPages"
                width: scroller.width - 10
                page: root.page
                onPageChanged: {
                    scroller.cancelFlick()
                    scroller.contentY = 0
                    if (!Appearance.performanceMode) arrive.restart()
                }
                NumberAnimation {
                    id: arrive
                    objectName: "settingsPageArrival"
                    target: pages
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: Appearance.duration(160)
                }
                Connections {
                    target: Appearance
                    function onPerformanceModeChanged() { if (Appearance.performanceMode) arrive.complete() }
                }
            }
        }

        IconButton {
            id: closeButton
            objectName: "settingsCloseButton"
            anchors.top: parent.top
            anchors.topMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            size: 36
            symbol: "close"
            tooltip: qsTr("Close")
            onClicked: root.close()
        }
    }
}
