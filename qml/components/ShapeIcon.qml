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

// a symbol on a material shape (end4's MaterialShapeWrappedMaterialSymbol).
// a new shape morphs in with a springy overshoot; hovered, it turns a little
// and the symbol leans the other way, or it morphs into `hoverShape` if it
// has one. still at rest.
Item {
    id: root

    property string shape: "cookie9"
    /// keeps its shape with no shapes (MaterialShape.keepsShape)
    property bool keepsShape: false
    /// what it springs into while hovered; none, and it only turns
    property string hoverShape
    property bool hovered: false
    property bool rotateOnHover: true
    property string symbol
    property real size: 40
    property real iconSize: Math.round(size * 0.5)
    property real fill: 1
    property color color: Appearance.colors.secondaryContainer
    property color iconColor: Appearance.colors.onSecondaryContainer

    readonly property string target: hovered && hoverShape.length > 0 ? hoverShape : shape
    property string fromShape: target
    property real progress: 1
    // it arrives as it is; only later changes morph
    property bool settled: false

    implicitWidth: size
    implicitHeight: size

    Component.onCompleted: settled = true

    onTargetChanged: {
        if (!settled || Appearance.performanceMode) {
            fromShape = target
            progress = 1
            return
        }
        // from whichever shape is showing most
        fromShape = progress >= 0.5 ? body.morphTo : body.shape
        progress = 0
        settle.restart()
    }

    NumberAnimation {
        id: settle
        target: root
        property: "progress"
        to: 1
        duration: 460
        easing.type: Easing.OutBack
        easing.overshoot: 2.2
    }

    MaterialShape {
        id: body
        anchors.fill: parent
        keepsShape: root.keepsShape
        shape: root.fromShape
        morphTo: root.target
        morph: root.progress
        color: root.color
        // room for the overshoot
        extent: 0.92
        spinAngle: root.rotateOnHover && root.hovered ? 24 : 0

        Behavior on spinAngle {
            enabled: !Appearance.performanceMode
            Bounce {}
        }
        Behavior on color {
            ElementMoveFastColor {}
        }
    }

    MaterialSymbol {
        objectName: "shapeIconSymbol"
        anchors.centerIn: parent
        text: root.symbol
        iconSize: root.iconSize
        fill: root.fill
        color: root.iconColor
        rotation: root.rotateOnHover && root.hovered ? -10 : 0

        Behavior on rotation {
            enabled: !Appearance.performanceMode
            Bounce {}
        }
    }
}
