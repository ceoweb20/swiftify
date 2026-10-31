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
// a pill whose edge is scalloped all the way round: the distance to a rounded
// box, pushed out by a wave measured along the box's own outline, so the
// scallops run evenly down the straight sides and round the ends alike and
// meet without a seam.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    vec4 color;
    // how far the scallops stand out from the pill
    float amplitude;
    // the length of one scallop along the outline, in pixels
    float wavelength;
    // radians; runs the scallops round the outline
    float phase;
    float cornerRadius;
};

const float PI = 3.14159265;

void main()
{
    vec2 halfSize = 0.5 * itemSize - vec2(amplitude + 0.75);
    vec2 p = qt_TexCoord0 * itemSize - 0.5 * itemSize;
    float radius = min(min(halfSize.x, halfSize.y), max(0.0, cornerRadius - amplitude - 0.75));
    // the straight parts' half lengths
    vec2 straight = halfSize - vec2(radius);

    vec2 q = abs(p) - straight;
    float box = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;

    // where the nearest point of the outline is, as a length round it,
    // clockwise from the top middle
    float quarter = 0.5 * PI * radius;
    float perimeter = 4.0 * (straight.x + straight.y) + 4.0 * quarter;
    float along;
    if (q.x > 0.0 && q.y > 0.0) {
        // round one of the four corners
        vec2 v = abs(p) - straight;
        float turn = atan(v.x, v.y); // 0 at the top or bottom, a quarter at the side
        if (p.y < 0.0 && p.x >= 0.0) {
            along = straight.x + radius * turn;
        } else if (p.y >= 0.0 && p.x >= 0.0) {
            along = straight.x + quarter + 2.0 * straight.y + radius * (0.5 * PI - turn);
        } else if (p.y >= 0.0) {
            along = 3.0 * straight.x + 2.0 * quarter + 2.0 * straight.y + radius * turn;
        } else {
            along = 3.0 * straight.x + 3.0 * quarter + 4.0 * straight.y + radius * (0.5 * PI - turn);
        }
    } else if (q.y > q.x) {
        // along the top, left to right, or the bottom, right to left
        along = p.y < 0.0 ? p.x : straight.x + 2.0 * quarter + 2.0 * straight.y + (straight.x - p.x);
    } else {
        // down one side or up the other
        along = p.x >= 0.0 ? straight.x + quarter + (p.y + straight.y)
                           : 3.0 * straight.x + 3.0 * quarter + 2.0 * straight.y + (straight.y - p.y);
    }

    // counted round the pill at its full size, so the count holds while the
    // amplitude springs about. counted on the moving outline, a scallop came
    // or went whenever it crossed a half, and a press made the edge jump
    vec2 outer = 0.5 * itemSize - vec2(0.75);
    float outerRadius = min(min(outer.x, outer.y), max(0.0, cornerRadius - 0.75));
    vec2 outerStraight = outer - vec2(outerRadius);
    float outerPerimeter = 4.0 * (outerStraight.x + outerStraight.y) + 2.0 * PI * outerRadius;
    float scallops = max(1.0, floor(outerPerimeter / max(wavelength, 1.0) + 0.5));
    float wave = 0.5 + 0.5 * cos(2.0 * PI * scallops * along / perimeter - phase);
    float d = box - amplitude * wave;
    fragColor = color * (clamp(0.5 - d, 0.0, 1.0) * qt_Opacity);
}
