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

// a button that is a material shape rather than a rounded box: it morphs to
// `morphTo` while `morphed`, can turn with the shared spin, and squishes
// when pressed and springs back.
T.AbstractButton {
    id: root

    property string shape: "circle"
    property string morphTo: shape
    property bool morphed: false
    property color color: Appearance.colors.primary
    property color hoverColor: color
    /// degrees of turn per radian of the shared spin; 0 holds still
    property real spinRate: 0
    property string tooltip

    property real morph: morphed ? 1 : 0

    Behavior on morph {
        enabled: !Appearance.performanceMode
        SpringAnimation { spring: 3.5; damping: 0.3; epsilon: 0.002 }
    }

    padding: 0
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.38
    scale: down ? 0.88 : 1

    Behavior on scale {
        enabled: !Appearance.performanceMode
        SpringAnimation { spring: 7; damping: 0.24; epsilon: 0.002 }
    }

    // the shape is drawn in a larger item than the button and reaches only
    // the button's size, so a spring that carries it past its size never
    // has a lobe cut off at the edge
    background: Item {
        MaterialShape {
            readonly property real room: Math.round(parent.width * 0.16)

            anchors.fill: parent
            anchors.margins: -room
            extent: parent.width / Math.max(1, parent.width + 2 * room)
            shape: root.shape
            morphTo: root.morphTo
            morph: root.morph
            color: root.hovered ? root.hoverColor : root.color
            spinRate: root.spinRate

            Behavior on color {
                ElementMoveFastColor {}
            }
        }
    }

    // as big as the button, so the tooltip places itself by the button
    Loader {
        anchors.fill: parent
        active: root.tooltip.length > 0 && root.hovered
        sourceComponent: StyledToolTip {
            visible: true
            text: root.tooltip
        }
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
