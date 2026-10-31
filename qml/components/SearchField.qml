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

// a material 3 search bar: a full pill with a leading magnifier and a clear
// button once there is something to clear.
Rectangle {
    id: root

    property alias text: input.text
    readonly property bool editing: input.activeFocus
    property string placeholder: qsTr("Search")
    signal accepted()

    function focusField() {
        input.forceActiveFocus()
        input.selectAll()
    }

    implicitWidth: 260
    implicitHeight: 40
    radius: height / 2
    color: input.activeFocus ? Qt.alpha(Appearance.colors.surfaceContainerHighest, 0.5)
                             : Appearance.colors.surfaceContainerHigh
    border.width: input.activeFocus ? 2 : 0
    border.color: Appearance.colors.primary

    Behavior on color {
        ElementMoveFastColor {}
    }

    TapHandler {
        onTapped: if (!root.editing) root.focusField()
    }

    MaterialSymbol {
        id: icon
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: "search"
        iconSize: 20
        color: Appearance.colors.onSurfaceVariant
    }

    TextInput {
        id: input
        x: icon.x + icon.width + 10
        width: Math.max(0, (clear.visible ? clear.x : root.width) - 12 - x)
        anchors.verticalCenter: parent.verticalCenter
        clip: true
        selectByMouse: true
        color: Appearance.colors.onSurface
        selectionColor: Appearance.colors.primary
        selectedTextColor: Appearance.colors.onPrimary
        font.family: Appearance.fontFamily
        font.pixelSize: Appearance.fontSize.small
        onAccepted: root.accepted()
        // escape belongs to the field while it is typed in, before the
        // window's escape takes it back a page
        Keys.onShortcutOverride: (event) => event.accepted = event.key === Qt.Key_Escape
        Keys.onEscapePressed: {
            if (text.length > 0) {
                text = ""
            } else {
                focus = false
            }
        }

        StyledText {
            width: input.width
            elide: Text.ElideRight
            anchors.verticalCenter: parent.verticalCenter
            Reveal.shown: input.text.length === 0
            text: root.placeholder
            color: Appearance.colors.onSurfaceVariant
            font.pixelSize: Appearance.fontSize.small
        }
    }

    IconButton {
        id: clear
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        Reveal.shown: input.text.length > 0 && root.width >= 100
        symbol: "close"
        size: 32
        iconSize: 18
        onClicked: {
            input.text = ""
            input.forceActiveFocus()
        }
    }
}
