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
// artwork clipped to a rounded rectangle (or circle) with an optional inner
// border, cropped to fill, in one pass with no layer or mask. with
// `waviness`, the edge is scalloped: a wave measured along the outline, so
// the scallops run evenly down the sides and round the corners and meet
// without a seam. it costs nothing while the waviness is zero.
//
// with `ringWaves`, a round cover's edge is itself a squiggle: a line drawn
// along a sine round its circle, r = middle + amplitude * sin(waves * angle),
// and the cover cut along the same sine moved straight in, crest under crest,
// so what is behind shows through between the two. `ringPhase` turns both.
//
// below 1, `reveal` shows the cover from the right edge with a soft front,
// all of it at 1.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    float radius;
    float borderWidth;
    vec4 borderColor;
    vec2 uvScale;
    vec2 uvOffset;
    // how deep the scallops cut, in pixels; 0 is a plain edge
    float waviness;
    // one scallop's length along the outline, in pixels
    float wavelength;
    // radians; runs the scallops round the outline
    float phase;
    // whole waves round a round cover's edge; 0 for a plain edge
    float ringWaves;
    // how far the edge strays to either side of its circle, in pixels
    float ringAmplitude;
    float ringThickness;
    // radians; turns the waves round the edge
    float ringPhase;
    vec4 ringColor;
    float reveal;
};

layout(binding = 1) uniform sampler2D source;

const float PI = 3.14159265;
const float REVEAL_SOFTNESS = 0.4;

float revealed()
{
    if (reveal >= 1.0) {
        return 1.0;
    }
    float front = 1.0 - reveal * (1.0 + REVEAL_SOFTNESS);
    return smoothstep(front, front + REVEAL_SOFTNESS, qt_TexCoord0.x);
}

// how many scallops run round the outline. counted on the item's own edge
// rather than the cut-in one: the cut moves as the waviness springs, and a
// count taken on it gained or lost a scallop whenever it crossed a half, so
// the edge jumped mid-spring
float scallopCount(vec2 halfSize)
{
    float corner = min(radius, min(halfSize.x, halfSize.y));
    vec2 straight = halfSize - vec2(corner);
    float perimeter = 4.0 * (straight.x + straight.y) + 2.0 * PI * corner;
    return max(1.0, floor(perimeter / max(wavelength, 1.0) + 0.5));
}

float edgeDistance(vec2 p, vec2 halfSize, float corner, float scallops)
{
    vec2 straight = halfSize - vec2(corner);
    vec2 q = abs(p) - straight;
    float box = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - corner;
    if (abs(waviness) < 0.001) {
        return box;
    }

    // where the nearest point of the outline is, as a length round it,
    // clockwise from the top middle
    float quarter = 0.5 * PI * corner;
    float perimeter = 4.0 * (straight.x + straight.y) + 4.0 * quarter;
    float along;
    if (q.x > 0.0 && q.y > 0.0) {
        vec2 v = abs(p) - straight;
        float turn = atan(v.x, v.y); // 0 at the top or bottom, a quarter at the side
        if (p.y < 0.0 && p.x >= 0.0) {
            along = straight.x + corner * turn;
        } else if (p.y >= 0.0 && p.x >= 0.0) {
            along = straight.x + quarter + 2.0 * straight.y + corner * (0.5 * PI - turn);
        } else if (p.y >= 0.0) {
            along = 3.0 * straight.x + 2.0 * quarter + 2.0 * straight.y + corner * turn;
        } else {
            along = 3.0 * straight.x + 3.0 * quarter + 4.0 * straight.y + corner * (0.5 * PI - turn);
        }
    } else if (q.y > q.x) {
        // along the top, left to right, or the bottom, right to left
        along = p.y < 0.0 ? p.x : straight.x + 2.0 * quarter + 2.0 * straight.y + (straight.x - p.x);
    } else {
        // down one side or up the other
        along = p.x >= 0.0 ? straight.x + quarter + (p.y + straight.y)
                           : 3.0 * straight.x + 3.0 * quarter + 2.0 * straight.y + (straight.y - p.y);
    }
    float wave = 0.5 + 0.5 * cos(2.0 * PI * scallops * along / perimeter - phase);
    return box - waviness * wave;
}

void main()
{
    if (ringWaves > 0.5) {
        vec2 p = qt_TexCoord0 * itemSize - 0.5 * itemSize;
        float r = length(p);
        // the circle the wave follows, kept far enough in that the line's
        // outer half and the wave's peaks stay inside the item
        float middle = 0.5 * min(itemSize.x, itemSize.y) - ringAmplitude - 0.5 * ringThickness - 0.75;
        float angle = ringWaves * atan(p.x, -p.y) - ringPhase;
        float wave = ringAmplitude * sin(angle);
        // dr/dθ of the sine; over the radius it is the slope along the circle
        float rise = ringAmplitude * ringWaves * cos(angle);

        // the line, as a distance from its sine. the slope is measured on
        // the sine's own circle: by the point's radius it blew up near the
        // middle and drew a speck there
        float curve = middle + wave;
        float slope = rise / max(curve, 1.0);
        float d = (r - curve) / sqrt(1.0 + slope * slope);
        float line = clamp(0.5 * ringThickness - abs(d) + 0.5, 0.0, 1.0) * ringColor.a;

        // the cover's edge: the same sine, two line widths further in
        float edge = curve - 2.0 * ringThickness;
        float edgeSlope = rise / max(edge, 1.0);
        float inside = (r - edge) / sqrt(1.0 + edgeSlope * edgeSlope);
        vec4 art = texture(source, qt_TexCoord0 * uvScale + uvOffset) * clamp(0.5 - inside, 0.0, 1.0);
        fragColor = (art * (1.0 - line) + vec4(ringColor.rgb, 1.0) * line) * (qt_Opacity * revealed());
        return;
    }

    // the scallops' peaks reach the item's edge, their valleys cut in
    vec2 halfSize = 0.5 * itemSize - vec2(abs(waviness));
    vec2 point = qt_TexCoord0 * itemSize - 0.5 * itemSize;
    float corner = min(radius, min(halfSize.x, halfSize.y));
    float distanceToEdge = edgeDistance(point, halfSize, corner, scallopCount(0.5 * itemSize));
    float coverage = clamp(0.5 - distanceToEdge, 0.0, 1.0);
    vec4 color = texture(source, qt_TexCoord0 * uvScale + uvOffset);
    if (borderWidth > 0.0) {
        float inner = clamp(0.5 - (distanceToEdge + borderWidth), 0.0, 1.0);
        float band = coverage - inner;
        color = color * (1.0 - band * borderColor.a) + borderColor * band;
    }
    fragColor = color * (coverage * qt_Opacity * revealed());
}
