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
// a shape's quad. everything that tells one shape from another rides in the
// vertices, so the scene graph can draw many at once (MaterialShapeItem).

layout(location = 0) in vec4 position;
// where in the shape's own unit square, already turned by its spin
layout(location = 1) in vec2 shapePoint;
// the shape's side in pixels, how far it has morphed, and its two cells
layout(location = 2) in vec4 params;
// premultiplied
layout(location = 3) in vec4 color;
// where it is in the parent it may be cut to, and that parent's width,
// height, corner radius and whether it cuts at all
layout(location = 4) in vec2 clipPoint;
layout(location = 5) in vec4 clipBox;

layout(location = 0) out vec2 vShapePoint;
layout(location = 1) flat out vec4 vParams;
layout(location = 2) flat out vec4 vColor;
layout(location = 3) out vec2 vClipPoint;
layout(location = 4) flat out vec4 vClipBox;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main()
{
    vShapePoint = shapePoint;
    vParams = params;
    vColor = color;
    vClipPoint = clipPoint;
    vClipBox = clipBox;
    gl_Position = qt_Matrix * position;
}
