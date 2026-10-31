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

// big, faint material shapes scattered behind something, after end4-pC's
// desktop stickers but kept quiet: tonal, low in contrast, and partly cut off
// by whatever clips them. the same key always lays them out the same way, so
// a page keeps its own. each turns at its own pace and way off the shared
// spin clock, so they hold still while nothing plays. with no shapes
// (Settings.noShapes) there are none.
Item {
    id: root

    /// what the layout is drawn from: a page's or a collection's name
    property string key: "swiftify"
    property int count: 4
    property real minimumSize: 70
    property real maximumSize: 200
    property MaterialScheme scheme: Appearance.colors
    /// how strongly they show, on top of the theme's own quiet level
    property real strength: 1
    readonly property real alpha: (scheme.dark ? 0.32 : 0.5) * strength
    /// cuts them to its own bounds with corners this round, as the rounded
    /// card holding it looks; below 0 they are left to whatever clips them
    property real clipRadius: -1

    readonly property list<string> shapes: ["heart", "ghostish", "bun", "flower", "sunny", "cookie9", "clover4",
                                            "softBoom", "puffy", "pixelCircle", "softBurst", "clamShell", "arch",
                                            "fan", "gem", "cookie6", "clover8", "verySunny", "puffyDiamond", "burst"]

    /// a string to a 32-bit seed (fnv-1a)
    function seedOf(text: string): int {
        let hash = 2166136261
        for (let index = 0; index < text.length; ++index) {
            hash ^= text.charCodeAt(index)
            hash = Math.imul(hash, 16777619)
        }
        return hash >>> 0
    }

    /// each sticker's place, size, shape, colour and turn, from the key alone
    readonly property var layout: {
        // mulberry32, so the same key always gives the same scatter
        let state = seedOf(key)
        const next = () => {
            state = (state + 0x6d2b79f5) >>> 0
            let t = state
            t = Math.imul(t ^ (t >>> 15), t | 1)
            t ^= t + Math.imul(t ^ (t >>> 7), t | 61)
            return ((t ^ (t >>> 14)) >>> 0) / 4294967296
        }
        const stickers = []
        for (let index = 0; index < count; ++index) {
            // spread across the width a slot each, jittered within it
            stickers.push({
                x: (index + 0.15 + 0.7 * next()) / count,
                y: -0.15 + 1.25 * next(),
                size: minimumSize + (maximumSize - minimumSize) * next(),
                shape: shapes[Math.floor(next() * shapes.length)],
                tone: Math.floor(next() * 3),
                angle: next() * 360,
                // degrees per radian of the spin clock, either way round
                pace: (next() < 0.5 ? -1 : 1) * (12 + 26 * next())
            })
        }
        return stickers
    }

    function tone(which: int): color {
        const base = which === 0 ? scheme.primaryContainer : (which === 1 ? scheme.tertiaryContainer : scheme.secondaryContainer)
        return Qt.alpha(base, alpha)
    }

    Repeater {
        model: Settings.noShapes ? 0 : root.count

        MaterialShape {
            id: sticker

            required property int index
            readonly property var spec: root.layout[index]

            width: spec.size
            height: spec.size
            x: spec.x * root.width - width / 2
            y: spec.y * root.height - height / 2
            shape: spec.shape
            color: root.tone(spec.tone)
            // turns with the music while it shows, still under reduced motion
            spinAngle: spec.angle
            spinRate: Appearance.reduceMotion ? 0 : spec.pace
            parentClipRadius: root.clipRadius
        }
    }
}
