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

// Narrow settings pages place choices under their label instead of squeezing
// both into one line. The controls keep their full touch target size.
Item {
    id: root

    property string text
    property alias segments: choices.segments
    property alias currentIndex: choices.currentIndex
    readonly property bool stacked: width < 500
    signal activated(int index)

    width: parent ? parent.width : 0
    implicitHeight: stacked ? caption.implicitHeight + choices.implicitHeight + 28
                            : Math.max(52, choices.implicitHeight + 16)
    opacity: enabled ? 1 : 0.38

    StyledText {
        id: caption
        x: 12
        y: root.stacked ? 8 : (root.height - height) / 2
        width: root.stacked ? root.width - 24 : Math.max(0, choices.x - x - 12)
        text: root.text
        wrapMode: root.stacked ? Text.Wrap : Text.NoWrap
        elide: root.stacked ? Text.ElideNone : Text.ElideRight
    }

    SegmentedControl {
        id: choices
        width: root.stacked ? root.width - 24 : implicitWidth
        x: root.stacked ? 12 : root.width - width - 12
        y: root.stacked ? caption.y + caption.implicitHeight + 8 : (root.height - height) / 2
        segmentWidth: root.stacked && segments.length > 0 ? (width - 2 * inset) / segments.length : 0
        segmentHeight: 32
        enabled: root.enabled
        onActivated: (index) => root.activated(index)
    }
}
