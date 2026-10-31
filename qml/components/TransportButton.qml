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

// a transport icon with the swift app's one-shot symbol effects: previous and
// next nudge in their direction, shuffle and repeat bounce. performance mode
// turns them off. a shape can spring in behind it while it is on, or while
// it is hovered (an arrow, pointing the way it goes); a button with a shape
// has it for its hover and press, with no round state layer under it.
IconButton {
    id: root

    enum Effect {
        None,
        NudgeLeft,
        NudgeRight,
        Bounce
    }

    property int effect: TransportButton.None
    /// what springs in behind it while toggled on
    property string toggledShape
    /// what springs up behind it on hover, turned to hoverAngle
    property string hoverShape
    property real hoverAngle: 0
    readonly property bool hasShape: toggledShape.length > 0 || hoverShape.length > 0
    readonly property bool showsShape: (toggled && toggledShape.length > 0) || (hovered && enabled && hasShape)

    colBackgroundHover: hasShape ? "transparent" : scheme.layer1Hover
    colBackgroundToggledHover: hasShape ? "transparent" : scheme.layer1Hover
    rippleEnabled: !hasShape

    onClicked: {
        if (Appearance.performanceMode) {
            return
        }
        if (effect === TransportButton.Bounce) {
            bounce.restart()
        } else if (effect !== TransportButton.None) {
            nudge.restart()
        }
    }

    transform: Translate {
        id: shift
    }

    MaterialShape {
        anchors.centerIn: parent
        width: root.width + 8
        height: root.height + 8
        z: -2
        visible: opacity > 0
        opacity: root.showsShape ? 1 : 0
        scale: root.showsShape ? 1 : 0.35
        extent: 0.9
        shape: root.hoverShape.length > 0 && !(root.toggled && root.toggledShape.length > 0) ? root.hoverShape
             : (root.toggledShape.length > 0 ? root.toggledShape : "circle")
        spinAngle: root.hoverShape.length > 0 && !(root.toggled && root.toggledShape.length > 0) ? root.hoverAngle : 0
        color: root.toggled ? (root.down ? root.scheme.secondaryContainerActive
                                         : root.hovered ? root.scheme.secondaryContainerHover : root.scheme.secondaryContainer)
                            : (root.down ? root.scheme.surfaceContainerHighestActive : root.scheme.surfaceContainerHighest)

        Behavior on opacity {
            NumberAnimation { duration: Appearance.duration(140); easing.type: Easing.OutCubic }
        }
        Behavior on scale {
            enabled: !Appearance.performanceMode
            Bounce {}
        }
        Behavior on color {
            ElementMoveFastColor {}
        }
    }

    SequentialAnimation {
        id: nudge
        NumberAnimation {
            target: shift
            property: "x"
            to: root.effect === TransportButton.NudgeLeft ? -4 : 4
            duration: 90
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: shift
            property: "x"
            to: 0
            duration: 350
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Appearance.curves.expressiveFastSpatial
        }
    }

    SequentialAnimation {
        id: bounce
        NumberAnimation { target: root; property: "scale"; to: 1.15; duration: 110; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "scale"; to: 1; duration: 400; easing.type: Easing.BezierSpline; easing.bezierCurve: Appearance.curves.expressiveDefaultSpatial }
    }
}
