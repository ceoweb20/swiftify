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

// what a page shows with nothing in it: end4's page placeholder, a symbol on a
// tonal cookie over a title and a line of explanation.
Column {
    id: root

    property string symbol: "music_note"
    /// the badge's shape, picked for what the placeholder says
    property string shape: {
        switch (symbol) {
        case "search_off": return "ghostish"
        case "music_note": return "heart"
        case "lyrics": return "bun"
        case "queue_music": return "clamShell"
        case "push_pin": return "clover4"
        case "album": return "sunny"
        case "star": return "heart"
        case "cloud_off":
        case "wifi_off": return "pixelCircle"
        default: return "cookie9"
        }
    }
    property string title
    property string description
    property MaterialScheme scheme: Appearance.colors
    property bool ready: false

    spacing: 10
    width: Math.min(parent ? parent.width : 400, 420)

    Component.onCompleted: ready = true
    onVisibleChanged: {
        if (!visible) arrive.complete()
        else if (ready && !Appearance.performanceMode) arrive.restart()
    }

    Connections {
        target: Appearance
        function onPerformanceModeChanged() { if (Appearance.performanceMode) arrive.complete() }
        function onReduceMotionChanged() { if (Appearance.reduceMotion) arrive.complete() }
    }

    MaterialShape {
        id: badge
        anchors.horizontalCenter: parent.horizontalCenter
        width: 72
        height: 72
        shape: root.shape
        color: root.scheme.secondaryContainer

        // little shapes about the badge, behind it
        Stickers {
            anchors.centerIn: parent
            width: 190
            height: 120
            z: -1
            key: root.title
            count: 3
            minimumSize: 16
            maximumSize: 30
            strength: 1.8
            scheme: root.scheme
        }

        ParallelAnimation {
            id: arrive
            objectName: "emptyStateArrival"
            NumberAnimation { target: badge; property: "opacity"; from: 0; to: 1; duration: Appearance.duration(180); easing.type: Easing.OutCubic }
            NumberAnimation { target: badge; property: "scale"; from: Appearance.reduceMotion ? 1 : 0.96; to: 1; duration: Appearance.motion(220); easing.type: Easing.OutCubic }
        }

        MaterialSymbol {
            anchors.centerIn: parent
            text: root.symbol
            iconSize: 34
            fill: 1
            color: root.scheme.onSecondaryContainer
        }
    }

    TitleText {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: root.title
        font.pixelSize: Appearance.fontSize.larger
        color: root.scheme.onSurface
        wrapMode: Text.Wrap
    }

    StyledText {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        visible: root.description.length > 0
        text: root.description
        color: root.scheme.onSurfaceVariant
        wrapMode: Text.Wrap
    }
}
