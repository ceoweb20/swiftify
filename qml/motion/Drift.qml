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

// a phase that runs while `active`: it eases up to speed and back down to
// rest rather than starting and stopping dead, and carries on from wherever
// it stopped. a looping animation from 0 snapped back to the start each time
// it was set going again. it asks for frames only while it moves.
FrameAnimation {
    id: root

    property bool active: false
    /// units a second at full speed; negative runs it backwards
    property real speed: 1
    /// where the value wraps round to 0
    property real period: 2 * Math.PI
    property real value: 0
    /// everything drifting runs while hovered; with the pointer left lying
    /// still it eases to rest (App.pointerResting)
    property real rate: active && !Appearance.reduceMotion && !App.pointerResting ? speed : 0

    Behavior on rate {
        NumberAnimation { duration: 650; easing.type: Easing.InOutSine }
    }

    running: Math.abs(rate) > 0.0001
    onTriggered: value = (value + rate * frameTime) % period
}
