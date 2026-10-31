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
// a soft shadow's quad (SoftShadowItem). what tells one shadow from another
// rides in the vertices, so the scene graph can draw many at once.

layout(location = 0) in vec4 position;
// from the rectangle's centre
layout(location = 1) in vec2 fromCentre;
// the solid core's half width and height, its corner radius, and how far
// past it the shadow fades
layout(location = 2) in vec4 box;
// premultiplied
layout(location = 3) in vec4 color;

layout(location = 0) out vec2 vFromCentre;
layout(location = 1) flat out vec4 vBox;
layout(location = 2) flat out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main()
{
    vFromCentre = fromCentre;
    vBox = box;
    vColor = color;
    gl_Position = qt_Matrix * position;
}
