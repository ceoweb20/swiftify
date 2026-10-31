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

// a rounded panel's corners, painted over in the colour behind it: what
// fills the panel clips square, so a sticker or a cover in flight reaching
// into a corner showed past the rounding. four small pieces, where cutting
// to the rounded shape itself would draw the whole panel through a texture
Item {
    id: root

    property real radius
    /// what shows behind the panel
    property color color

    // each corner a ring round the corner's centre, from the panel's edge
    // out past the square's corner, seen only within the corner's square
    component Corner: Item {
        id: corner

        /// where the corner's centre is, from the square's top left: 0 or 1
        /// a side across and down
        required property real across
        required property real down

        width: root.radius
        height: root.radius
        clip: true

        Rectangle {
            x: corner.across * root.radius - 2 * root.radius
            y: corner.down * root.radius - 2 * root.radius
            width: 4 * root.radius
            height: 4 * root.radius
            radius: 2 * root.radius
            color: "transparent"
            border.width: root.radius
            border.color: root.color
        }
    }

    Corner { across: 1; down: 1 }
    Corner { x: root.width - width; across: 0; down: 1 }
    Corner { y: root.height - height; across: 1; down: 0 }
    Corner { x: root.width - width; y: root.height - height; across: 0; down: 0 }
}
