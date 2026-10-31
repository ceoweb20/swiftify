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

// while the saved login is checked (SpotifyStartupLoadingView)
Item {
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: -height / 2
        width: parent.width * 1.4
        height: width
        radius: width / 2
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.5; color: Qt.rgba(Appearance.colors.primary.r, Appearance.colors.primary.g, Appearance.colors.primary.b, 0.16) }
            GradientStop { position: 1; color: "transparent" }
        }
        opacity: 0.8
    }

    Stickers {
        anchors.fill: parent
        key: "startup"
        count: 6
    }

    Rectangle {
        id: panel
        objectName: "startupStatusPanel"
        anchors.centerIn: parent
        width: Math.min(484, parent.width - 2 * Appearance.sizes.pageMargin)
        height: status.implicitHeight + 64
        radius: 28
        color: Appearance.colors.surfaceContainerHigh
        border.width: 1
        border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.4)
    }

    Column {
        id: status
        anchors.centerIn: parent
        width: panel.width - 48
        spacing: 20

        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 64
            height: 64

            Image {
                anchors.fill: parent
                source: "qrc:/qt/qml/Swiftify/resources/swiftify-icon.png"
                sourceSize: Qt.size(128, 128)
                smooth: true
            }
        }

        TitleText {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Swiftify"
            font.pixelSize: Appearance.fontSize.headline
            font.weight: Font.Bold
        }

        LoadingSpinner {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 44
            height: 44
        }

        StyledText {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTr("Checking your saved Spotify login…")
            color: Appearance.colors.onSurfaceVariant
        }
    }
}
