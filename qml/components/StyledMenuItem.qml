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

T.MenuItem {
    id: root

    property string symbol
    property string trailingSymbol
    property bool destructive: false
    /// a tick while checked, without the item toggling itself: for a choice
    /// the owner keeps, so the tick always follows the owner's binding
    property bool checkmark: false
    readonly property MaterialScheme scheme: (root.menu as StyledMenu)?.scheme ?? Appearance.colors
    readonly property color foreground: destructive ? scheme.error : scheme.onSurface

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 40
    leftPadding: 12
    rightPadding: subMenu !== null || trailingSymbol.length > 0 ? 36 : 16
    spacing: 12
    hoverEnabled: true
    opacity: enabled ? 1 : 0.38

    // as wide as its words want; given less, by a menu at its widest, they
    // end in an ellipsis rather than running out of sight
    contentItem: Item {
        implicitWidth: (icon.visible ? icon.implicitWidth + root.spacing : 0) + label.implicitWidth
        implicitHeight: Math.max(icon.implicitHeight, label.implicitHeight)

        MaterialSymbol {
            id: icon
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            visible: root.symbol.length > 0 || root.checkable || root.checkmark
            text: root.checkable || root.checkmark ? (root.checked ? "check" : "") : root.symbol
            iconSize: 20
            color: root.destructive ? root.foreground : root.scheme.onSurfaceVariant
        }

        StyledText {
            id: label
            anchors.left: icon.visible ? icon.right : parent.left
            anchors.leftMargin: icon.visible ? root.spacing : 0
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            elide: Text.ElideRight
            color: root.foreground
            font.pixelSize: Appearance.fontSize.small
        }
    }

    arrow: MaterialSymbol {
        x: root.width - width - 10
        y: (root.height - height) / 2
        visible: root.subMenu !== null || root.trailingSymbol.length > 0
        text: root.subMenu !== null ? "chevron_right" : root.trailingSymbol
        iconSize: 20
        color: root.scheme.onSurfaceVariant
    }

    background: Rectangle {
        implicitWidth: 200
        radius: Appearance.rounding.small
        color: root.down ? root.scheme.layer2Active
             : Qt.alpha(root.scheme.layer2Hover, root.highlighted || root.hovered ? 1 : 0)

        Behavior on color {
            ElementMoveFastColor {}
        }
    }
}
