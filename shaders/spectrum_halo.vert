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
// the halo or wave behind the bars: a vertex-coloured mesh, as qt's own
// vertex colour material draws it, with the item's own coordinates handed on
// for the cut along a round cover's squiggle.

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vItem;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 cut;
    vec4 wave;
};

void main()
{
    vColor = color;
    vItem = position.xy;
    gl_Position = qt_Matrix * position;
}
