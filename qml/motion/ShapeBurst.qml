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

// a little burst of material shapes from a point: hearts for a favourite,
// cookies for play, clovers for a pin, flowers for a playlist. they fly
// out, tumble, fall a little and fade in under a second, and the burst takes
// itself away when it is done, so nothing is left running (UiState.burst).
Item {
    id: root

    /// hearts, play, pin or playlist
    property string kind: "play"
    property int count: 10
    property MaterialScheme scheme: Appearance.colors
    /// 0 to 1 over the burst's life
    property real t

    readonly property var shapes: ({
        hearts: ["heart"],
        play: ["cookie9", "sunny", "cookie6", "softBurst"],
        pin: ["clover4", "clover8"],
        playlist: ["flower", "puffy", "cookie7"]
    })
    readonly property list<color> tones: kind === "hearts"
        ? [scheme.primary, scheme.tertiary, scheme.error]
        : [scheme.primary, scheme.tertiary, scheme.secondary]

    // each piece's direction, reach, size, shape, colour and tumble
    readonly property var pieces: {
        const kindShapes = shapes[kind] ?? shapes.play
        const list = []
        for (let index = 0; index < count; ++index) {
            const angle = 2 * Math.PI * (index + 0.6 * Math.random()) / count - Math.PI / 2
            list.push({
                dx: Math.cos(angle),
                dy: Math.sin(angle),
                reach: 34 + 46 * Math.random(),
                size: 9 + 10 * Math.random(),
                shape: kindShapes[Math.floor(Math.random() * kindShapes.length)],
                tone: Math.floor(Math.random() * 3),
                turn: (Math.random() < 0.5 ? -1 : 1) * (120 + 260 * Math.random())
            })
        }
        return list
    }

    width: 0
    height: 0

    NumberAnimation on t {
        from: 0
        to: 1
        duration: 720
        easing.type: Easing.Linear
        onFinished: root.destroy()
    }

    Repeater {
        model: root.count

        MaterialShape {
            id: piece

            required property int index
            readonly property var spec: root.pieces[index]
            // out fast, then easing off
            readonly property real travel: 1 - Math.pow(1 - root.t, 3)

            width: spec.size
            height: spec.size
            x: spec.dx * spec.reach * travel - width / 2
            // and a little gravity
            y: spec.dy * spec.reach * travel + 46 * root.t * root.t - height / 2
            shape: spec.shape
            // a favourite's hearts stay hearts with no shapes
            keepsShape: root.kind === "hearts"
            color: root.tones[spec.tone]
            spinAngle: spec.turn * root.t
            scale: root.t < 0.18 ? 0.3 + 0.7 * root.t / 0.18 : 1 - 0.45 * (root.t - 0.18) / 0.82
            opacity: root.t < 0.55 ? 1 : 1 - (root.t - 0.55) / 0.45
        }
    }
}
