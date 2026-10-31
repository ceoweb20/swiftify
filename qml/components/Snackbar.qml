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

// the error banner (ErrorBanner in the swift app) as a material snackbar: an
// error container with selectable text and a close button. it goes on its own
// after eight seconds.
Item {
    id: root

    property string message
    property int serial: 0
    readonly property bool shown: message.length > 0
    signal dismissed()

    implicitWidth: Math.min(560, row.implicitWidth + 32)
    implicitHeight: row.implicitHeight + 24
    visible: opacity > 0
    opacity: shown ? 1 : 0
    scale: shown ? 1 : 0.94

    Behavior on opacity {
        NumberAnimation { duration: Appearance.duration(root.shown ? 200 : 150) }
    }
    Behavior on scale {
        ElementMoveSmall {}
    }

    onSerialChanged: if (shown && visible) timer.restart()

    Timer {
        id: timer
        interval: 8000
        running: root.shown && root.visible && !Session.evaluatingStoredLogin
        onTriggered: root.dismissed()
    }

    Rectangle {
        anchors.fill: parent
        radius: Appearance.rounding.normal
        color: Appearance.colors.errorContainer
    }

    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        x: 16
        width: parent.width - 24
        spacing: 12

        ShapeIcon {
            anchors.verticalCenter: parent.verticalCenter
            size: 32
            iconSize: 18
            shape: "softBoom"
            symbol: "warning"
            color: Appearance.colors.error
            iconColor: Appearance.colors.onError
        }

        TextEdit {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, 440)
            text: root.message
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            color: Appearance.colors.onErrorContainer
            selectionColor: Appearance.colors.error
            selectedTextColor: Appearance.colors.onError
            font.family: Appearance.fontFamily
            font.pixelSize: Appearance.fontSize.small
        }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            symbol: "close"
            size: 32
            iconSize: 20
            iconColor: Appearance.colors.onErrorContainer
            onClicked: root.dismissed()
        }
    }
}
