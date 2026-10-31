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

// a press ripple clipped to a rounded shape by its shader, so the button under
// it needs no layer. start() at the press, release() when it ends.
ShaderEffect {
    id: root

    property real cornerRadius: 0
    property color color: "transparent"
    property point center: Qt.point(0, 0)
    property real radius: 0
    readonly property size itemSize: Qt.size(width, height)

    opacity: 0
    visible: opacity > 0 && radius > 0
    fragmentShader: "qrc:/shaders/ripple.frag.qsb"

    function start(x: real, y: real) {
        fade.stop()
        center = Qt.point(x, y)
        const dx = Math.max(x, width - x)
        const dy = Math.max(y, height - y)
        spread.to = Math.sqrt(dx * dx + dy * dy)
        opacity = 1
        spread.restart()
    }

    function release() {
        fade.restart()
    }

    NumberAnimation {
        id: spread
        target: root
        property: "radius"
        from: 0
        duration: Appearance.motion(1200)
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Appearance.curves.standardDecel
    }

    NumberAnimation {
        id: fade
        target: root
        property: "opacity"
        to: 0
        duration: Appearance.duration(700)
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Appearance.curves.standardDecel
    }
}
