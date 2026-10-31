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

// One surface per group: heading and controls share the same rounded card.
Rectangle {
    id: root

    property string title
    property string symbol
    readonly property list<string> shapes: ["cookie9", "clover4", "sunny", "flower", "bun", "heart", "puffy", "gem"]
    property string iconShape: {
        let hash = 0
        for (let index = 0; index < title.length; ++index) hash = (hash * 31 + title.charCodeAt(index)) >>> 0
        return shapes[hash % shapes.length]
    }
    default property alias content: rows.data

    objectName: "settingsSection"
    width: parent ? parent.width : 0
    implicitHeight: rows.y + rows.implicitHeight + 12
    radius: 20
    color: Appearance.colors.surfaceContainerHigh
    border.width: 1
    border.color: Qt.alpha(Appearance.colors.outlineVariant, 0.35)
    clip: true

    Row {
        id: heading
        x: 16
        y: 16
        width: parent.width - 32
        spacing: 10

        ShapeIcon {
            id: icon
            anchors.verticalCenter: parent.verticalCenter
            visible: root.symbol.length > 0
            size: 28
            iconSize: 16
            shape: root.iconShape
            symbol: root.symbol
            color: Appearance.colors.secondaryContainer
            iconColor: Appearance.colors.onSecondaryContainer
            rotateOnHover: false
        }
        TitleText {
            width: heading.width - (icon.visible ? icon.width + heading.spacing : 0)
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            wrapMode: Text.Wrap
            font.pixelSize: Appearance.fontSize.large
        }
    }

    Column {
        id: rows
        objectName: "settingsSectionRows"
        x: 12
        y: heading.y + heading.height + 10
        width: parent.width - 24
        spacing: 4
    }
}
