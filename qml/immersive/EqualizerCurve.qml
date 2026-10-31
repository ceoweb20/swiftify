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

// the equalizer's response (EqualizerCurve): each band as a gaussian bump,
// summed, so the line reads as what the ear gets rather than as ten points.
// a soft fill under it, a glow while the equalizer is on, a dot per band.
// it eases to new gains, and repaints only while they move. the glow is two
// wide faint strokes rather than a canvas shadow, which blurred the whole
// canvas on the cpu at every paint, every frame of a colour change.
Canvas {
    id: root

    property list<real> gains
    property real maximumGain: 12
    property bool active: true
    property color tint: "white"
    property color neutral: Qt.rgba(1, 1, 1, 0.55)

    property list<real> start
    property list<real> target
    property real progress: 1

    onGainsChanged: {
        start = shown()
        target = gains
        progress = 0
        ease.restart()
    }
    onProgressChanged: requestPaint()
    onActiveChanged: requestPaint()
    onTintChanged: requestPaint()
    Component.onCompleted: {
        target = gains
        start = gains
    }

    NumberAnimation {
        id: ease
        target: root
        property: "progress"
        to: 1
        duration: Appearance.motion(250)
        easing.type: Easing.OutCubic
    }

    function shown(): list<real> {
        const values = []
        for (let band = 0; band < target.length; ++band) {
            const from = band < start.length ? start[band] : 0
            values.push(from + (target[band] - from) * progress)
        }
        return values
    }

    function level(values: list<real>, position: real): real {
        const spacing = 1 / (values.length - 1)
        const width = spacing * 0.85
        let total = 0
        for (let band = 0; band < values.length; ++band) {
            const distance = (position - band * spacing) / width
            total += values[band] * Math.exp(-distance * distance)
        }
        return total
    }

    onPaint: {
        const context = getContext("2d")
        context.reset()
        const values = shown()
        if (values.length < 2 || width <= 0 || height <= 0) {
            return
        }
        const midline = height / 2
        const reach = height / 2 - 8
        let peak = 0
        for (let position = 0; position <= 1.0001; position += 0.02) {
            peak = Math.max(peak, Math.abs(level(values, position)))
        }
        const span = Math.max(maximumGain * 1.4, peak)
        const colour = active ? tint : neutral
        const at = (position) => midline - level(values, position) / span * reach

        context.strokeStyle = Qt.rgba(neutral.r, neutral.g, neutral.b, 0.3)
        context.lineWidth = 1
        context.setLineDash([3, 4])
        context.beginPath()
        context.moveTo(0, midline)
        context.lineTo(width, midline)
        context.stroke()
        context.setLineDash([])

        const steps = Math.max(Math.floor(width / 2), 24)
        const trace = () => {
            context.beginPath()
            for (let step = 0; step <= steps; ++step) {
                const position = step / steps
                if (step === 0) {
                    context.moveTo(0, at(0))
                } else {
                    context.lineTo(position * width, at(position))
                }
            }
        }

        trace()
        context.lineTo(width, midline)
        context.lineTo(0, midline)
        context.closePath()
        const fill = context.createLinearGradient(0, 0, 0, height)
        fill.addColorStop(0, Qt.rgba(colour.r, colour.g, colour.b, 0.34))
        fill.addColorStop(1, Qt.rgba(colour.r, colour.g, colour.b, 0.02))
        context.fillStyle = fill
        context.fill()

        context.lineCap = "round"
        context.lineJoin = "round"
        if (active) {
            context.strokeStyle = Qt.rgba(colour.r, colour.g, colour.b, 0.12)
            context.lineWidth = 11
            trace()
            context.stroke()
            context.strokeStyle = Qt.rgba(colour.r, colour.g, colour.b, 0.3)
            context.lineWidth = 5
            trace()
            context.stroke()
        }
        context.strokeStyle = Qt.rgba(colour.r, colour.g, colour.b, active ? 1 : 0.6)
        context.lineWidth = 2.5
        trace()
        context.stroke()

        context.fillStyle = colour
        const spacing = 1 / (values.length - 1)
        for (let band = 0; band < values.length; ++band) {
            const position = band * spacing
            const radius = Math.abs(values[band]) > 0.05 ? 4.5 : 3
            context.beginPath()
            context.arc(position * width, at(position), radius, 0, 2 * Math.PI)
            context.fill()
        }
    }
}
