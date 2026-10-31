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
// material 3 expressive's shapes, read from a distance-field atlas
// (tools/shape_atlas): each cell holds a shape's signed distance from its
// outline, around the unit square it is normalised into. two shapes and a
// blend between their distances give a morph from one to the other, and a
// blend carried past 1 by a spring exaggerates the second for a moment.

layout(location = 0) in vec2 vShapePoint;
layout(location = 1) flat in vec4 vParams;
layout(location = 2) flat in vec4 vColor;
layout(location = 3) in vec2 vClipPoint;
layout(location = 4) flat in vec4 vClipBox;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

layout(binding = 1) uniform sampler2D atlas;

// the atlas as the app packs it (ShapeAtlasLayout.h): 4 by 3 cells of 128
// texels, four shapes to a cell, one in each channel, each shape's unit
// square 100 texels across, 14 in from its cell's edge
const vec2 ATLAS = vec2(4.0 * 128.0, 3.0 * 128.0);
// the furthest distance a cell holds, in shape units
const float RANGE = 0.14;

// the distance at p, in shape units, from the shape at `index` in the
// catalogue (Shapes.qml)
float distanceIn(float index, vec2 p)
{
    float shape = floor(index + 0.5);
    float place = floor(shape / 4.0);
    vec2 origin = (vec2(mod(place, 4.0), floor(place / 4.0)) * 128.0 + 14.0) / ATLAS;
    vec4 channel = vec4(equal(vec4(mod(shape, 4.0)), vec4(0.0, 1.0, 2.0, 3.0)));
    // further out than the cell reaches, carry on in a straight line
    vec2 inner = clamp(p, vec2(-RANGE + 0.01), vec2(1.0 + RANGE - 0.01));
    float value = dot(texture(atlas, origin + inner * (100.0 / ATLAS)), channel);
    return (0.5 - value) * 2.0 * RANGE + length(p - inner);
}

void main()
{
    float side = vParams.x;
    float d = mix(distanceIn(vParams.z, vShapePoint), distanceIn(vParams.w, vShapePoint), vParams.y) * side;
    float coverage = clamp(0.5 - d, 0.0, 1.0);
    // cut to the rounded rectangle of the parent it sits in, if it is
    if (vClipBox.w > 0.5) {
        vec2 halfSize = vClipBox.xy * 0.5;
        vec2 q = abs(vClipPoint - halfSize) - halfSize + vClipBox.z;
        float outside = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - vClipBox.z;
        coverage *= clamp(0.5 - outside, 0.0, 1.0);
    }
    fragColor = vColor * (coverage * qt_Opacity);
}
