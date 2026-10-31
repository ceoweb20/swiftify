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

// a title in end4's manner: google sans flex drawn fully round, a touch wide
// and heavy through its variable axes, so headings read bubbly while body
// text stays plain. without google sans flex it is simply bold.
StyledText {
    /// display, headline or title: the page's name, a hero's, a section's
    property string level: "title"

    font.pixelSize: level === "display" ? Appearance.fontSize.display
                  : level === "headline" ? Appearance.fontSize.headline : Appearance.fontSize.title
    font.weight: Font.Bold
    font.letterSpacing: level === "display" ? -0.4 : 0
    font.variableAxes: ({ "ROND": 100, "wdth": 110, "wght": 720 })
}
