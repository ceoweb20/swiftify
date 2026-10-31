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

// the play button wherever music starts: a soft five-bump cookie while
// paused that springs into a nine-scallop one, turning, while music plays.
// the glyph pops as it changes.
ShapeButton {
    id: root

    property bool playing: Player.playing
    property color iconColor: Appearance.colors.onPrimary
    property real iconSize: Math.round(width * 0.52)

    shape: "softCookie"
    morphTo: "cookie9"
    morphed: playing
    spinRate: 25
    tooltip: playing ? qsTr("Pause") : qsTr("Play")

    onPlayingChanged: if (!Appearance.performanceMode) pop.restart()
    // starting music throws out a little burst of cookies
    onClicked: if (!playing) UiState.burst(root, "play")

    contentItem: Item {
        MaterialSymbol {
            id: glyph
            anchors.centerIn: parent
            text: root.playing ? "pause" : "play_arrow"
            iconSize: root.iconSize
            fill: 1
            color: root.iconColor
        }
    }

    NumberAnimation {
        id: pop
        target: glyph
        property: "scale"
        from: 0.55
        to: 1
        duration: 480
        easing.type: Easing.OutBack
        easing.overshoot: 2.6
    }
}
