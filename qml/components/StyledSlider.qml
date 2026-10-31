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

// a material 3 expressive slider: the track splits around a slim handle, with
// a stop dot at the far end. horizontal or vertical (the equalizer's bands).
T.Slider {
    id: root

    property MaterialScheme scheme: Appearance.colors
    property real trackThickness: 16
    property real handleLength: trackThickness + 14
    property color activeColor: scheme.primary
    property color inactiveColor: scheme.secondaryContainer
    property color handleColor: activeColor
    property bool showStopIndicator: true
    property bool showValueTip: false
    property string valueText: Math.round(position * 100) + "%"
    readonly property real trackLength: horizontal ? availableWidth : availableHeight
    readonly property real handleThickness: pressed ? 2 : 4
    readonly property real handleAt: visualPosition * trackLength
    readonly property real gap: 4
    readonly property real outerRadius: Math.min(trackThickness / 2, 8)

    implicitWidth: horizontal ? 200 : handleLength
    implicitHeight: horizontal ? handleLength : 200
    padding: 0
    hoverEnabled: true
    opacity: enabled ? 1 : 0.38

    background: Item {
        x: root.leftPadding
        y: root.topPadding
        width: root.availableWidth
        height: root.availableHeight

        // the filled part runs from the start (left, or bottom) to the handle
        Rectangle {
            readonly property real length: Math.max(0, root.horizontal ? root.handleAt - root.gap - root.handleThickness / 2
                                                                       : root.trackLength - root.handleAt - root.gap - root.handleThickness / 2)
            visible: length > 0.5
            color: root.activeColor
            x: root.horizontal ? 0 : (parent.width - root.trackThickness) / 2
            y: root.horizontal ? (parent.height - root.trackThickness) / 2 : root.trackLength - length
            width: root.horizontal ? length : root.trackThickness
            height: root.horizontal ? root.trackThickness : length
            topLeftRadius: root.horizontal ? root.outerRadius : Appearance.rounding.unsharpen
            topRightRadius: Appearance.rounding.unsharpen
            bottomLeftRadius: root.outerRadius
            bottomRightRadius: root.horizontal ? Appearance.rounding.unsharpen : root.outerRadius
        }

        Rectangle {
            id: inactiveTrack
            readonly property real start: root.horizontal ? root.handleAt + root.gap + root.handleThickness / 2 : 0
            readonly property real length: Math.max(0, root.horizontal ? root.trackLength - start
                                                                       : root.handleAt - root.gap - root.handleThickness / 2)
            visible: length > 0.5
            color: root.inactiveColor
            x: root.horizontal ? start : (parent.width - root.trackThickness) / 2
            y: root.horizontal ? (parent.height - root.trackThickness) / 2 : 0
            width: root.horizontal ? length : root.trackThickness
            height: root.horizontal ? root.trackThickness : length
            topLeftRadius: root.horizontal ? Appearance.rounding.unsharpen : root.outerRadius
            topRightRadius: root.outerRadius
            bottomLeftRadius: Appearance.rounding.unsharpen
            bottomRightRadius: root.horizontal ? root.outerRadius : Appearance.rounding.unsharpen

            Rectangle {
                visible: root.showStopIndicator && inactiveTrack.length > 12
                width: 4
                height: 4
                radius: 2
                color: root.scheme.onSecondaryContainer
                x: root.horizontal ? inactiveTrack.width - 6 - width / 2 : (inactiveTrack.width - width) / 2
                y: root.horizontal ? (inactiveTrack.height - height) / 2 : 6 - height / 2
            }
        }
    }

    handle: Rectangle {
        x: root.leftPadding + (root.horizontal ? root.handleAt - width / 2 : (root.availableWidth - width) / 2)
        y: root.topPadding + (root.horizontal ? (root.availableHeight - height) / 2 : root.handleAt - height / 2)
        width: root.horizontal ? root.handleThickness : root.handleLength
        height: root.horizontal ? root.handleLength : root.handleThickness
        radius: Appearance.rounding.full
        color: root.handleColor

        Behavior on width {
            ElementMoveFast {}
        }
        Behavior on height {
            ElementMoveFast {}
        }

        StyledToolTip {
            visible: root.showValueTip && root.pressed
            delay: 0
            text: root.valueText
        }
    }

    // Handler needs to be on a child, otherwise the slider takes every mouse button instead of only the left one.
    Item {
        anchors.fill: parent

        HoverHandler {
            cursorShape: root.pressed ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        }
    }
}
