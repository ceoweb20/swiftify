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

// end4's RippleButton: a state-layer background that tints on hover, ripples on
// press and can be toggled. right and middle clicks are reported separately.
T.AbstractButton {
    id: root

    property bool toggled: false
    property MaterialScheme scheme: Appearance.colors
    property real buttonRadius: Appearance.rounding.small
    property real buttonRadiusPressed: buttonRadius
    readonly property real effectiveRadius: root.down ? root.buttonRadiusPressed : root.buttonRadius
    property color colBackground: "transparent"
    property color colBackgroundHover: scheme.layer1Hover
    property color colBackgroundToggled: scheme.primary
    property color colBackgroundToggledHover: scheme.primaryHover
    property color colRipple: scheme.layer1Active
    property color colRippleToggled: scheme.primaryActive
    property bool rippleEnabled: true
    property bool pointingHandCursor: true
    property alias backgroundItem: backgroundRectangle
    /// shown on hover; created only while it is
    property string tooltip
    /// a press squishes the button and the release springs it back. wide
    /// rows barely move; a small button gives a proper squash.
    property bool bounce: true

    signal secondaryClicked(point position)
    signal middleClicked()

    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    opacity: enabled ? 1 : 0.38
    scale: bounce && down ? 1 - Math.min(0.1, 5 / Math.max(width, 1)) : 1

    Behavior on scale {
        enabled: !Appearance.performanceMode
        Bounce {}
    }
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: 8

    background: Rectangle {
        id: backgroundRectangle

        implicitHeight: 40
        radius: root.effectiveRadius
        color: {
            const rest = root.toggled ? root.colBackgroundToggled : root.colBackground
            const hover = root.toggled ? root.colBackgroundToggledHover : root.colBackgroundHover
            return root.hovered ? Appearance.fadeable(hover, rest) : Appearance.fadeable(rest, hover)
        }
        border.width: root.visualFocus ? 2 : 0
        border.color: root.scheme.secondary

        Behavior on color {
            ElementMoveFastColor {}
        }
        Behavior on radius {
            ElementMoveFast {}
        }

        Ripple {
            id: ripple
            anchors.fill: parent
            cornerRadius: backgroundRectangle.radius
            color: root.toggled ? root.colRippleToggled : root.colRipple
        }
    }

    onPressed: {
        if (rippleEnabled) {
            ripple.start(pressX - leftInset, pressY - topInset)
        }
    }
    onReleased: ripple.release()
    onCanceled: ripple.release()

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
        cursorShape: root.pointingHandCursor && root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        acceptedButtons: Qt.RightButton | Qt.MiddleButton
        gesturePolicy: TapHandler.WithinBounds
        onTapped: (eventPoint, button) => {
            if (button === Qt.RightButton) {
                root.secondaryClicked(eventPoint.position)
            } else {
                root.middleClicked()
            }
        }
    }
}
