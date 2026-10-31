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

// a material 3 common button: filled, tonal, outlined, text or elevated, with
// an optional leading symbol. Buttons keep their pill shape while pressing;
// RippleButton handles the shrink and bounce animation.
RippleButton {
    id: root

    enum Variant {
        Filled,
        Tonal,
        Outlined,
        Text,
        Elevated
    }

    property int variant: TextButton.Filled
    property string symbol
    property bool busy: false
    property bool wavy: false
    property bool iconOnly: false
    property real compactSize: 40
    readonly property real naturalWidth: label.implicitWidth + (symbol.length > 0 ? 26 + 16 : 24) + 24
    /// a burst of shapes when clicked (UiState.burst): play, pin, ...
    property string burst
    property color labelColor: {
        switch (variant) {
        case TextButton.Filled: return scheme.onPrimary
        case TextButton.Tonal: return scheme.onSecondaryContainer
        default: return scheme.primary
        }
    }

    implicitWidth: iconOnly ? compactSize : naturalWidth
    implicitHeight: iconOnly ? compactSize : 40
    leftPadding: iconOnly ? 0 : (symbol.length > 0 ? 16 : 24)
    rightPadding: iconOnly ? 0 : 24
    buttonRadius: Appearance.rounding.full
    rippleEnabled: !wavy || iconOnly
    backgroundItem.visible: !wavy || iconOnly
    colBackground: {
        switch (variant) {
        case TextButton.Filled: return scheme.primary
        case TextButton.Tonal: return scheme.secondaryContainer
        case TextButton.Elevated: return scheme.surfaceContainerLow
        default: return "transparent"
        }
    }
    colBackgroundHover: {
        switch (variant) {
        case TextButton.Filled: return scheme.primaryHover
        case TextButton.Tonal: return scheme.secondaryContainerHover
        case TextButton.Elevated: return scheme.layer1Hover
        default: return Qt.rgba(scheme.primary.r, scheme.primary.g, scheme.primary.b, 0.08)
        }
    }
    colRipple: {
        switch (variant) {
        case TextButton.Filled: return scheme.primaryActive
        case TextButton.Tonal: return scheme.secondaryContainerActive
        default: return Qt.rgba(scheme.primary.r, scheme.primary.g, scheme.primary.b, 0.14)
        }
    }
    backgroundItem.border.width: variant === TextButton.Outlined ? 1 : (visualFocus ? 2 : 0)
    backgroundItem.border.color: variant === TextButton.Outlined && !visualFocus ? scheme.outlineVariant : scheme.secondary

    WavyPill {
        anchors.fill: parent
        z: -1
        visible: root.wavy && !root.iconOnly
        hovered: root.hovered
        pressed: root.down
        color: root.down ? root.colRipple : (root.hovered ? root.colBackgroundHover : root.colBackground)

        Behavior on color {
            ElementMoveFastColor {}
        }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            opacity: root.busy ? 0 : 1

            MaterialSymbol {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.symbol.length > 0
                text: root.symbol
                iconSize: root.iconOnly ? Math.min(22, root.height * 0.55) : 18
                fill: 1
                color: root.labelColor
            }

            StyledText {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.iconOnly
                text: root.text
                font.pixelSize: Appearance.fontSize.small
                font.weight: Font.DemiBold
                color: root.labelColor
            }
        }

        LoadingSpinner {
            anchors.centerIn: parent
            width: 20
            height: 20
            running: root.busy
            Reveal.shown: root.busy
            color: root.labelColor
            trackColor: "transparent"
        }
    }

    Loader {
        anchors.fill: parent
        active: root.iconOnly && root.hovered
        sourceComponent: StyledToolTip {
            visible: true
            text: root.text
        }
    }

    onClicked: if (burst.length > 0) UiState.burst(root, burst)
}
