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

// one step of the PageHost's stack: the page for a destination, built while
// it is kept, and the cover a click on it sent flying to the next page's
// hero, so that cover can fly home again when the next page is left
Item {
    id: slot

    final property Component page
    /// an old step is let go, to be built afresh if back ever reaches it
    final property bool built: true
    final property ArtworkImage returnCover: null
    final property url returnSource
    final property bool returnCircular: false
    readonly property Item view: loader.item as Item

    Loader {
        id: loader
        anchors.fill: parent
        active: slot.built
        sourceComponent: slot.page
    }
}
