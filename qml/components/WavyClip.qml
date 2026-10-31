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

// what it holds, cut to a rounded rectangle whose edge turns to running
// scallops (rounded_image.frag) while `wavy`, springing in and out. at rest
// the content draws itself as usual: only while it waves does it go through
// a texture, so a grid of these costs nothing until one is hovered. with no
// shapes (Settings.noShapes) its edge stays plain.
Item {
    id: root

    default property alias content: holder.data
    property real radius: Appearance.sizes.artworkRadius
    property bool wavy: false
    /// how deep the scallops cut while wavy, in pixels
    property real depth: 3.5
    property real wavelength: 22
    property real waviness: wavy && !Settings.noShapes ? depth : 0
    readonly property real phase: drift.value
    readonly property bool waving: Math.abs(waviness) > 0.01

    Behavior on waviness {
        enabled: !Appearance.performanceMode
        Bounce {}
    }

    Drift {
        id: drift
        active: root.wavy && root.visible && !Settings.noShapes
        speed: 2 * Math.PI / 2.4
    }

    Item {
        id: holder
        anchors.fill: parent
    }

    ShaderEffectSource {
        id: texture
        visible: false
        sourceItem: root.waving ? holder : null
        hideSource: true
    }

    ShaderEffect {
        anchors.fill: parent
        visible: root.waving

        property variant source: texture
        property size itemSize: Qt.size(width, height)
        property real radius: root.radius
        property real borderWidth: 0
        property color borderColor: "transparent"
        property size uvScale: Qt.size(1, 1)
        property point uvOffset: Qt.point(0, 0)
        property real waviness: root.waviness
        property real wavelength: root.wavelength
        property real phase: root.phase
        property real ringWaves: 0
        property real ringAmplitude: 0
        property real ringThickness: 0
        property real ringPhase: 0
        property color ringColor: "transparent"
        property real reveal: 1

        fragmentShader: "qrc:/shaders/rounded_image.frag.qsb"
    }
}
