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

// text whose characters roll when they change, one at a time, as swiftui's
// numeric text transition does: a countdown moves only the digits that tick.
Row {
    id: root

    property string text
    property real pixelSize: Appearance.fontSize.normal
    property int weight: Font.Normal
    /// drawn round, as a title is (TitleText)
    property bool rounded: false
    property color color: Appearance.colors.onSurface
    /// new characters arrive from above, as a falling count does
    property bool countsDown: true

    Repeater {
        model: root.text.length

        delegate: Item {
            id: cell

            required property int index
            readonly property string character: root.text.charAt(index)

            width: current.implicitWidth
            height: current.implicitHeight
            clip: true

            onCharacterChanged: {
                previous.text = current.text
                current.text = character
                // with reduced motion the old one fades into the new in place
                if (Appearance.performanceMode || previous.text === current.text) {
                    return
                }
                roll.restart()
            }

            StyledText {
                id: previous
                opacity: 0
                tabularNumbers: true
                font.pixelSize: root.pixelSize
                font.weight: root.weight
                font.variableAxes: root.rounded ? ({ "ROND": 100, "wdth": 110, "wght": 720 }) : ({})
                color: root.color
            }
            // set by hand, not bound, so the old character is still here to roll away
            Component.onCompleted: current.text = character

            StyledText {
                id: current
                tabularNumbers: true
                font.pixelSize: root.pixelSize
                font.weight: root.weight
                font.variableAxes: root.rounded ? ({ "ROND": 100, "wdth": 110, "wght": 720 }) : ({})
                color: root.color
            }

            ParallelAnimation {
                id: roll

                readonly property real travel: Appearance.reduceMotion ? 0 : cell.height * (root.countsDown ? 1 : -1)

                NumberAnimation { target: current; property: "y"; from: -roll.travel; to: 0; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: current; property: "opacity"; from: 0; to: 1; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: previous; property: "y"; from: 0; to: roll.travel; duration: 250; easing.type: Easing.OutCubic }
                NumberAnimation { target: previous; property: "opacity"; from: 1; to: 0; duration: 250; easing.type: Easing.OutCubic }
            }
        }
    }
}
