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

// the catchphrase pill: a sparkle and a phrase, gone after a couple of seconds.
Item {
    id: root

    property string text
    function show(message: string) {
        root.text = message
        root.opacity = 1
        root.scale = 1
        hide.restart()
    }

    implicitWidth: row.implicitWidth + 36
    implicitHeight: 48
    visible: opacity > 0
    opacity: 0
    scale: 0.9

    Behavior on opacity {
        NumberAnimation { duration: Appearance.duration(250); easing.type: Easing.OutCubic }
    }
    Behavior on scale {
        ElementMoveSmall {}
    }

    Timer {
        id: hide
        interval: 2200
        onTriggered: {
            root.opacity = 0
            root.scale = 0.9
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Appearance.colors.inverseSurface
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10

        ShapeIcon {
            anchors.verticalCenter: parent.verticalCenter
            size: 30
            iconSize: 17
            shape: "softBurst"
            symbol: "auto_awesome"
            color: Appearance.colors.inversePrimary
            iconColor: Appearance.colors.inverseSurface
        }

        StyledText {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: Appearance.colors.inverseOnSurface
            font.pixelSize: Appearance.fontSize.normal
            font.weight: Font.DemiBold
        }
    }
}
