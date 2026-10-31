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

// a library shortcut on home (HomeLibraryShortcut)
T.AbstractButton {
    id: root

    property string title
    property string subtitle
    property string symbol
    property string iconShape: "cookie9"
    /// keeps its shape with no shapes: the favourite songs' heart
    property bool keepsShape: false

    implicitHeight: 64
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    scale: down ? 0.95 : (hovered ? 1.02 : 1)

    Behavior on scale {
        enabled: !Appearance.performanceMode
        Bounce {}
    }

    // hovered, its edge turns to running scallops
    background: WavyClip {
        radius: 16
        wavy: root.hovered
        depth: 2.5
        wavelength: 18

        Rectangle {
            anchors.fill: parent
            radius: 16
            // two steps up from the page (surfaceContainerLow), so it stands off it
            color: root.hovered ? Appearance.colors.surfaceContainerHighest : Appearance.colors.surfaceContainerHigh

            Behavior on color {
                ElementMoveFastColor {}
            }
        }
    }

    contentItem: Item {
        ShapeIcon {
            id: badge
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            size: 44
            iconSize: 21
            shape: root.iconShape
            keepsShape: root.keepsShape
            symbol: root.symbol
            color: Appearance.colors.primaryContainer
            iconColor: Appearance.colors.onPrimaryContainer
            hovered: root.hovered
        }

        Column {
            anchors.left: badge.right
            anchors.leftMargin: 12
            anchors.right: chevron.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            StyledText {
                width: parent.width
                text: root.title
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.small
                font.weight: Font.DemiBold
            }
            StyledText {
                width: parent.width
                text: root.subtitle
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.smaller
                color: Appearance.colors.onSurfaceVariant
            }
        }

        MaterialSymbol {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: "chevron_right"
            iconSize: 18
            color: Appearance.colors.outline
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
