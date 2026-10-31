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

// minimise, maximise and close, for compositors that do not draw their own,
// each on a shape of its own as the top bar's other buttons are
Row {
    id: root

    required property Window window
    property bool compact: false
    property MaterialScheme scheme: Appearance.colors
    layoutDirection: Settings.windowControlsOnRight ? Qt.RightToLeft : Qt.LeftToRight

    spacing: 4

    ToolbarButton {
        objectName: "closeWindowButton"
        scheme: root.scheme
        size: root.compact ? 28 : 36
        iconSize: root.compact ? 15 : 18
        shape: "cookie9"
        shapeHoverColor: root.scheme.errorContainer
        shapePressedColor: root.scheme.errorContainerActive
        symbol: "close"
        tooltip: qsTr("Close window")
        onClicked: root.window.close()
    }
    ToolbarButton {
        objectName: "maximizeWindowButton"
        scheme: root.scheme
        size: root.compact ? 28 : 36
        iconSize: root.compact ? 14 : 16
        shape: "cookie6"
        symbol: root.window.visibility === Window.Maximized ? "close_fullscreen" : "open_in_full"
        tooltip: root.window.visibility === Window.Maximized ? qsTr("Restore") : qsTr("Maximize")
        onClicked: root.window.visibility === Window.Maximized ? root.window.showNormal() : root.window.showMaximized()
    }
    ToolbarButton {
        objectName: "minimizeWindowButton"
        scheme: root.scheme
        size: root.compact ? 28 : 36
        iconSize: root.compact ? 15 : 18
        shape: "pill"
        turnsOnHover: false
        symbol: "remove"
        tooltip: qsTr("Minimize")
        onClicked: root.window.showMinimized()
    }
}
