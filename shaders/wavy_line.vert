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
// a wavy divider's quad (WavyLineItem). the wave's measures and colour ride
// in the vertices, so the scene graph draws many dividers at once.

layout(location = 0) in vec4 position;
// from the line's left end, across its middle, in pixels
layout(location = 1) in vec2 local;
// the line's length, the wave's amplitude, wavelength and phase
layout(location = 2) in vec4 wave;
// premultiplied
layout(location = 3) in vec4 color;
layout(location = 4) in float thickness;

layout(location = 0) out vec2 vLocal;
layout(location = 1) flat out vec4 vWave;
layout(location = 2) flat out vec4 vColor;
layout(location = 3) flat out float vThickness;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main()
{
    vLocal = local;
    vWave = wave;
    vColor = color;
    vThickness = thickness;
    gl_Position = qt_Matrix * position;
}
