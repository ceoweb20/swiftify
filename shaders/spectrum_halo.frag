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
// the halo or wave, its colours already premultiplied, cut along a round
// cover's squiggle line as the bars are (spectrum_bars.frag)

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vItem;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 cut;
    vec4 wave;
};

float outsideTheLine(vec2 point)
{
    if (wave.z < 0.5) {
        return 1.0;
    }
    vec2 p = point - cut.xy;
    float angle = wave.x * atan(p.x, -p.y) - wave.y;
    float curve = cut.z + cut.w * sin(angle);
    float slope = cut.w * wave.x * cos(angle) / max(curve, 1.0);
    return clamp((length(p) - curve) / sqrt(1.0 + slope * slope) + 0.5, 0.0, 1.0);
}

void main()
{
    fragColor = vColor * (qt_Opacity * outsideTheLine(vItem));
}
