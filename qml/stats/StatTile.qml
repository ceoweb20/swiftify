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

// one figure of the overview: a shape with its icon, the figure, and what it
// counts (listening-stats' overview cards)
Rectangle {
    id: root

    property string value
    property string label
    property string symbol
    property string iconShape: "cookie9"
    property bool accent: false
    readonly property bool compact: width < 180

    implicitHeight: compact ? 124 : 96
    radius: 20
    color: accent ? Appearance.colors.primaryContainer : Appearance.colors.surfaceContainerLow
    border.width: 0.75
    border.color: Qt.alpha(Appearance.colors.outlineVariant, accent ? 0 : 0.35)

    ShapeIcon {
        id: icon
        x: root.compact ? 14 : 16
        y: root.compact ? 12 : (root.height - height) / 2
        size: root.compact ? 28 : 44
        iconSize: root.compact ? 17 : 22
        shape: root.iconShape
        symbol: root.symbol
        hovered: hover.hovered
        color: root.accent ? Appearance.colors.primary : Appearance.colors.secondaryContainer
        iconColor: root.accent ? Appearance.colors.onPrimary : Appearance.colors.onSecondaryContainer
    }

    Column {
        x: root.compact ? 14 : icon.x + icon.width + 14
        y: root.compact ? icon.y + icon.height + 6 : (root.height - implicitHeight) / 2
        width: Math.max(0, root.width - x - 14)
        spacing: 2

        TitleText {
            width: parent.width
            text: root.value
            maximumLineCount: 1
            wrapMode: Text.NoWrap
            elide: Text.ElideRight
            font.pixelSize: Appearance.fontSize.huge
            font.weight: Font.Bold
            color: root.accent ? Appearance.colors.onPrimaryContainer : Appearance.colors.onSurface
        }
        StyledText {
            width: parent.width
            text: root.label
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            font.pixelSize: Appearance.fontSize.smaller
            color: root.accent ? Appearance.colors.onPrimaryContainer : Appearance.colors.onSurfaceVariant
        }
    }

    HoverHandler {
        id: hover
    }
}
