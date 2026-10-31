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

T.AbstractButton {
    id: root

    property song song
    property bool current: false
    property MaterialScheme scheme: Appearance.colors

    implicitHeight: 54
    hoverEnabled: true
    // click focus would let other mouse buttons grab and press it
    focusPolicy: Qt.TabFocus
    padding: 6
    leftPadding: 10
    rightPadding: 10
    enabled: !current

    background: Rectangle {
        radius: 10
        color: root.hovered ? root.scheme.layer1Hover : "transparent"
    }

    contentItem: Item {
        opacity: root.song.isPlayable ? 1 : 0.38

        ArtworkImage {
            id: art
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 42
            height: 42
            radius: 6
            source: root.song.artworkUrl
        }

        Column {
            anchors.left: art.right
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            TitleText {
                objectName: "queueRowTitle"
                width: parent.width
                text: root.song.name
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.smallie
                color: root.current ? root.scheme.primary : root.scheme.onSurface
            }
            StyledText {
                width: parent.width
                text: root.song.artists
                elide: Text.ElideRight
                font.pixelSize: Appearance.fontSize.smaller
                color: root.scheme.onSurfaceVariant
            }
        }
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
