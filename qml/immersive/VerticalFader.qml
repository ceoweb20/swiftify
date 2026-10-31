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

// one equalizer band (VerticalFader): a thin track filled from its middle, and
// a white cap that glows in the accent. dragging moves the value by how far
// the pointer travels, not to where it is, so a grab never jumps; arrow keys
// and the wheel step it.
Item {
    id: root

    property real value: 0
    property real from: -12
    property real to: 12
    property real stepSize: 0.5
    property bool glowing: true
    property color accent: "white"
    property color trackColor: Qt.rgba(1, 1, 1, 0.12)

    /// the user moved it; the owner stores the value
    signal moved(real value)

    readonly property real capWidth: 26
    readonly property real capHeight: 15
    readonly property real travel: Math.max(1, height - capHeight)
    readonly property real fraction: to > from ? Math.min(1, Math.max(0, (value - from) / (to - from))) : 0.5
    readonly property real targetDisplacement: (0.5 - fraction) * travel
    property real displacement: targetDisplacement

    implicitWidth: 44
    implicitHeight: 132
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.4

    Behavior on displacement {
        enabled: !drag.pressed
        NumberAnimation { duration: Appearance.motion(250); easing.type: Easing.OutCubic }
    }

    function quantized(proposed: real): real {
        const clamped = Math.min(Math.max(proposed, root.from), root.to)
        return root.stepSize > 0 ? Math.round(clamped / root.stepSize) * root.stepSize : clamped
    }

    function step(direction: int) {
        const next = quantized(value + direction * stepSize)
        if (next !== value) {
            moved(next)
        }
    }

    Keys.onUpPressed: step(1)
    Keys.onDownPressed: step(-1)

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 5
        height: parent.height
        radius: width / 2
        color: root.trackColor
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.height / 2 + Math.min(0, root.displacement)
        width: 5
        height: Math.abs(root.displacement)
        radius: width / 2
        color: root.accent
    }

    Item {
        id: cap
        anchors.horizontalCenter: parent.horizontalCenter
        y: (root.height - root.capHeight) / 2 + root.displacement
        width: root.capWidth
        height: root.capHeight

        SoftShadow {
            anchors.fill: parent
            radius: height / 2
            blur: 6
            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, root.glowing ? 0.4 : 0)

            Behavior on color {
                ColorAnimation { duration: Appearance.duration(220) }
            }
        }
        SoftShadow {
            anchors.fill: parent
            radius: height / 2
            blur: 3
            offset: Qt.vector2d(0, 1)
            color: Qt.rgba(0, 0, 0, 0.3)
        }
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: "white"
            border.width: root.activeFocus ? 2 : 0
            border.color: root.accent

            Rectangle {
                anchors.centerIn: parent
                width: root.capWidth - 12
                height: 2
                radius: 1
                color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.55)
            }
        }
    }

    MouseArea {
        id: drag

        property real originValue
        property real originY

        anchors.fill: parent
        cursorShape: Qt.SizeVerCursor
        // a drag up or down is the fader's, not the scrolling panel's round it
        preventStealing: true
        onPressed: (mouse) => {
            root.forceActiveFocus()
            originValue = root.value
            originY = mouse.y
        }
        onPositionChanged: (mouse) => {
            const moved = -(mouse.y - originY) / root.travel * (root.to - root.from)
            const next = root.quantized(originValue + moved)
            if (next !== root.value) {
                root.moved(next)
            }
        }
        onWheel: (wheel) => {
            if (wheel.angleDelta.y !== 0) {
                root.step(wheel.angleDelta.y > 0 ? 1 : -1)
            }
        }
    }
}
