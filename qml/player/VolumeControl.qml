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

// the speaker button that slides out into a volume pill. a second click
// mutes; moving away folds it back. the sleep timer's fade locks the slider.
Item {
    id: root

    property MaterialScheme scheme: Appearance.colors
    property bool expanded: false
    readonly property string symbol: Player.volume <= 0.01 ? "volume_off"
        : Player.volume <= 0.34 ? "volume_mute"
        : Player.volume <= 0.67 ? "volume_down" : "volume_up"

    implicitWidth: 32
    implicitHeight: 32

    Rectangle {
        id: pill
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: root.expanded ? 172 : 32
        height: root.expanded ? 42 : 32
        radius: Appearance.rounding.normal
        color: Qt.alpha(root.scheme.surfaceContainerHighest, root.expanded ? 1 : 0)
        z: 3

        Behavior on width {
            ElementMoveSmall {}
        }
        Behavior on height {
            ElementMoveSmall {}
        }
        Behavior on color {
            ElementMoveFastColor {}
        }

        HoverHandler {
            id: pillHover
            onHoveredChanged: if (!hovered) root.expanded = false
        }

        StyledSlider {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: button.left
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            // fades with the pill, and only once the pill has room for it
            opacity: root.expanded && pill.width > 100 ? 1 : 0
            visible: opacity > 0
            scheme: root.scheme
            trackThickness: 8
            handleLength: 20
            showStopIndicator: false
            showValueTip: true
            enabled: !Player.volumeManagedBySleepTimer
            value: Player.volume
            onMoved: Player.setVolume(value)

            Behavior on opacity {
                NumberAnimation { duration: Appearance.duration(150) }
            }
        }

        IconButton {
            id: button
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            size: 32
            iconSize: 20
            symbol: root.symbol
            iconColor: root.expanded ? root.scheme.primary : root.scheme.onSurfaceVariant
            tooltip: root.expanded ? (Player.volume <= 0.01 ? qsTr("Unmute") : qsTr("Mute")) : qsTr("Show Volume")
            onClicked: {
                if (root.expanded) {
                    Player.toggleMute()
                } else {
                    root.expanded = true
                }
            }
        }
    }
}
