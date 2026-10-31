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

// a pill with a scalloped edge (wavy_pill.frag), for the buttons that play
// things: its scallops deepen and start running round the edge while hovered.
// with no shapes (Settings.noShapes) it is a plain pill.
ShaderEffect {
    id: root

    property color color: Appearance.colors.primary
    property bool hovered: false
    property bool pressed: false

    property size itemSize: Qt.size(width, height)
    property real cornerRadius: Math.min(width, height) / 2
    property real amplitude: Settings.noShapes ? 0 : (pressed ? 4 : (hovered ? 3.2 : 2.2))
    property real wavelength: 15
    readonly property real phase: drift.value

    Behavior on amplitude {
        enabled: !Appearance.performanceMode
        Bounce {}
    }

    // the scallops start running round on hover and slow to a stop where
    // they are when the pointer leaves
    Drift {
        id: drift
        active: root.hovered && root.visible && !Settings.noShapes
        speed: 2 * Math.PI / 1.6
    }

    fragmentShader: "qrc:/shaders/wavy_pill.frag.qsb"
}
