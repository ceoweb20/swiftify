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
// the material press ripple, clipped to its button's rounded shape in one pass
// so buttons never need an offscreen layer or an opacity mask.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    vec2 center;
    float radius;
    float cornerRadius;
    vec4 color;
};

float roundedBox(vec2 point, vec2 halfSize, float corner)
{
    vec2 q = abs(point) - halfSize + corner;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - corner;
}

void main()
{
    vec2 point = qt_TexCoord0 * itemSize;
    float corner = min(cornerRadius, 0.5 * min(itemSize.x, itemSize.y));
    float inside = clamp(0.5 - roundedBox(point - 0.5 * itemSize, 0.5 * itemSize, corner), 0.0, 1.0);
    // solid through 60% of the radius, then a soft edge, like end4's gradient ripple.
    float distanceFromCenter = length(point - center);
    float disc = 1.0 - smoothstep(0.6 * radius, radius, distanceFromCenter);
    fragColor = color * (inside * disc * qt_Opacity);
}
