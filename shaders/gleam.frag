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
// a soft gleam running along a line, as under a title while its song loads:
// a bright band that comes in from beyond one end and goes out past the
// other, fading in and out towards the line's ends, so it never stops at an
// edge or jumps back to the start in sight.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 color;
    // the middle of the band, as a share of the line's length; it runs from
    // below 0 to above 1
    float position;
    // the band's half width, as a share of the line's length
    float halfWidth;
    // how far in from each end the line fades, as a share of its length
    float edge;
};

void main()
{
    float x = qt_TexCoord0.x;
    float offset = (x - position) / halfWidth;
    // a smooth bump, brightest in the middle and nothing at its sides
    float band = clamp(1.0 - offset * offset, 0.0, 1.0);
    band *= band;
    float ends = smoothstep(0.0, edge, x) * smoothstep(0.0, edge, 1.0 - x);
    fragColor = color * (band * ends * qt_Opacity);
}
