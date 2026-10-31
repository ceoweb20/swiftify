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
// a rounded rectangle's soft shadow, as qt quick effects' RectangularShadow
// draws it: solid over a core a little inside the rectangle, then fading as
// the square of a smoothstep out from the core's edge.

layout(location = 0) in vec2 vFromCentre;
layout(location = 1) flat in vec4 vBox;
layout(location = 2) flat in vec4 vColor;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main()
{
    float radius = vBox.z;
    float outside = length(max(abs(vFromCentre) - vBox.xy + vec2(radius), vec2(0.0))) - radius;
    float a = 1.0 - smoothstep(0.0, max(vBox.w, 0.0001), outside);
    fragColor = vColor * (qt_Opacity * a * a);
}
