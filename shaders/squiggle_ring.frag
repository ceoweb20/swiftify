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

#version 440
// a ring that waves as it goes round, in the way of material 3 expressive's
// wavy indicators: the stroke wanders either side of a circle a whole number
// of times, and `phase` sends the waves travelling round it. with `progress`
// under 1, only that share is wavy, clockwise from the top, and the rest is
// a plain thin track with a gap at each end.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    vec4 color;
    vec4 trackColor;
    // the circle the wave follows, from the centre, in pixels
    float radius;
    // how far the stroke strays to either side of it
    float amplitude;
    // whole waves round the ring
    float waves;
    float thickness;
    // radians; turns the waves round the ring
    float phase;
    // 0 to 1
    float progress;
};

const float TAU = 6.28318530718;

void main()
{
    vec2 p = qt_TexCoord0 * itemSize - 0.5 * itemSize;
    float r = length(p);
    // 0 at the top, growing clockwise
    float turn = atan(p.x, -p.y);
    turn = turn < 0.0 ? turn + TAU : turn;

    float angle = waves * turn - phase;
    float curve = radius + amplitude * sin(angle);
    // the curve's slope against the circle, so the stroke keeps its width
    // where the wave is steep
    float slope = amplitude * waves * cos(angle) / max(r, 1.0);
    float distanceToWave = abs(r - curve) / sqrt(1.0 + slope * slope);
    float wave = clamp(thickness * 0.5 - distanceToWave + 0.5, 0.0, 1.0);

    float whole = step(0.999, progress);
    float arcEnd = progress * TAU;
    // the arc's ends, measured along the ring in pixels
    float within = clamp((arcEnd - turn) * r + 0.5, 0.0, 1.0) * clamp(turn * r + 0.5, 0.0, 1.0);
    vec4 drawn = color * wave * mix(within, 1.0, whole);

    float gap = thickness * 1.5 + amplitude;
    float track = clamp(thickness * 0.4 - abs(r - radius) + 0.5, 0.0, 1.0);
    float beyond = clamp((turn - arcEnd) * r - gap + 0.5, 0.0, 1.0) * clamp((TAU - turn) * r - gap + 0.5, 0.0, 1.0);
    vec4 rest = trackColor * track * beyond * (1.0 - whole);

    fragColor = (drawn + rest * (1.0 - drawn.a)) * qt_Opacity;
}
