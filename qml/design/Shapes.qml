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

pragma Singleton
pragma ComponentBehavior: Bound
import QtQuick

// material 3 expressive's shape library: all 35 of material's shapes and two
// of the app's own, as cells of the distance-field atlas MaterialShape draws
// from (tools/shape_atlas draws it; `names` is its order, checked by the
// tests against the atlas MaterialShapeItem finds cells in). the immersive
// player's reveal still draws a few of them from their polar parameters
// (params), which suits a shape grown over a whole window.
QtObject {
    /// in atlas order: material's 35, then the app's own
    readonly property list<string> names: [
        "circle", "square", "slanted", "arch", "fan", "arrow", "semiCircle", "oval", "pill", "triangle", "diamond",
        "clamShell", "pentagon", "gem", "sunny", "verySunny", "cookie4", "cookie6", "cookie7", "cookie9", "cookie12",
        "ghostish", "clover4", "clover8", "burst", "softBurst", "boom", "softBoom", "flower", "puffy", "puffyDiamond",
        "pixelCircle", "pixelTriangle", "bun", "heart", "softCookie", "squircle"
    ]

    /// (lobes, depth, sharpness, kind), for shape_mask.frag: lobed shapes
    /// bulge from a circle, depth is how far the valleys sink as a share of
    /// the radius, and sharpness under 1 puffs the lobes while over 1 points
    /// them; kind 1 is a superellipse, with sharpness as its exponent
    function params(name: string): vector4d {
        switch (name) {
        case "cookie4": return Qt.vector4d(4, 0.12, 1, 0)
        case "cookie6": return Qt.vector4d(6, 0.1, 1, 0)
        case "cookie7": return Qt.vector4d(7, 0.1, 1, 0)
        case "cookie9": return Qt.vector4d(9, 0.09, 1, 0)
        case "cookie12": return Qt.vector4d(12, 0.07, 1, 0)
        // a paused play button: fewer bumps than a cookie, and gentler ones
        case "softCookie": return Qt.vector4d(5, 0.06, 1.15, 0)
        case "clover4": return Qt.vector4d(4, 0.42, 0.55, 0)
        case "clover8": return Qt.vector4d(8, 0.28, 0.6, 0)
        case "sunny": return Qt.vector4d(8, 0.14, 2.2, 0)
        case "verySunny": return Qt.vector4d(8, 0.26, 2.6, 0)
        case "softBurst": return Qt.vector4d(10, 0.16, 1.5, 0)
        case "burst": return Qt.vector4d(12, 0.24, 2.8, 0)
        case "flower": return Qt.vector4d(8, 0.32, 0.45, 0)
        case "puffy": return Qt.vector4d(6, 0.18, 0.5, 0)
        case "pentagon": return Qt.vector4d(5, 0.12, 3, 0)
        case "gem": return Qt.vector4d(6, 0.08, 4, 0)
        case "squircle": return Qt.vector4d(0, 0, 4, 1)
        default: return Qt.vector4d(0, 0, 1, 0)
        }
    }
}
