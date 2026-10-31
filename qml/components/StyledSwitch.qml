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
import QtQuick.Templates as T
import Swiftify

// the material 3 switch: the thumb grows when on, and squeezes while held.
T.Switch {
    id: root

    property MaterialScheme scheme: Appearance.colors

    implicitWidth: 52
    implicitHeight: 32
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.38

    indicator: Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? root.scheme.primary : root.scheme.surfaceContainerHighest
        border.width: root.visualFocus ? 2 : (root.checked ? 0 : 2)
        border.color: root.visualFocus ? root.scheme.secondary : root.scheme.outline

        Behavior on color {
            ElementMoveFastColor {}
        }

        Rectangle {
            property real size: root.pressed ? 28 : (root.checked ? 24 : 16)
            width: size
            height: size
            radius: size / 2
            anchors.verticalCenter: parent.verticalCenter
            x: root.checked ? parent.width - width - (parent.height - width) / 2 : (parent.height - width) / 2
            color: root.checked ? root.scheme.onPrimary : root.scheme.outline

            Behavior on x {
                ElementMoveSmall {}
            }
            Behavior on size {
                ElementMoveFast {}
            }
            Behavior on color {
                ElementMoveFastColor {}
            }
        }
    }

    contentItem: Item {}

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
