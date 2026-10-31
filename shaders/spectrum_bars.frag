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
// the visualizer's bars (SpectrumBars.metal): each bar's rounded outline as a
// signed distance, over its own gaussian glow, integrated across the bar in
// closed form and sampled along it. one draw for every bar.

layout(location = 0) in vec2 vLocal;
layout(location = 1) flat in vec4 vColor;
// width, length, tip radius, foot radius
layout(location = 2) flat in vec4 vShape;
layout(location = 3) in vec2 vItem;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float pixelsPerPoint;
    float glowSigma;
    float glowOpacity;
    // round around a cover, the bars are cut along its squiggle line: the
    // centre (x, y), the line's circle and how far it strays either side
    vec4 cut;
    // the waves round the circle, their turn in radians, and whether to cut
    vec4 wave;
};

// how much of a point is outside the squiggle line (ArtworkImage's sine)
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

const float PI = 3.14159265359;
const float SQRT1_2 = 0.70710678118;

vec2 barRadii(vec4 shape)
{
    float limit = min(shape.x, shape.y) * 0.5;
    return vec2(clamp(shape.z, 0.0, limit), clamp(shape.w, 0.0, limit));
}

// signed distance from the outline, and how wide a pixel is across it
vec2 barDistance(vec2 point, vec4 shape)
{
    vec2 halfSize = shape.xy * 0.5;
    vec2 radii = barRadii(shape);
    vec2 centred = point - vec2(0.0, halfSize.y);
    float radius = centred.y > 0.0 ? radii.x : radii.y;
    vec2 q = abs(centred) - halfSize + radius;
    float distance = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
    vec2 normal = (q.x > 0.0 && q.y > 0.0) ? normalize(q) : (q.x > q.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
    return vec2(distance, abs(normal.x) + abs(normal.y));
}

// abramowitz and stegun 7.1.27
vec2 erfApprox(vec2 x)
{
    vec2 s = sign(x);
    vec2 a = abs(x);
    vec2 t = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
    t *= t;
    return s - s / (t * t);
}

float gaussian(float x, float sigma)
{
    return exp(-(x * x) / (2.0 * sigma * sigma)) / (sqrt(2.0 * PI) * sigma);
}

float shadowRow(float x, float row, float sigma, float radius, vec2 halfSize)
{
    float delta = min(halfSize.y - radius - abs(row), 0.0);
    float curved = halfSize.x - radius + sqrt(max(0.0, radius * radius - delta * delta));
    vec2 integral = 0.5 + 0.5 * erfApprox((x + vec2(-curved, curved)) * (SQRT1_2 / sigma));
    return integral.y - integral.x;
}

// the outline convolved with a gaussian, after evan wallace's rounded shadows
float barShadow(vec2 point, vec4 shape, float sigma)
{
    vec2 halfSize = shape.xy * 0.5;
    vec2 radii = barRadii(shape);
    vec2 centred = point - vec2(0.0, halfSize.y);
    float low = centred.y - halfSize.y;
    float high = centred.y + halfSize.y;
    float start = clamp(-3.0 * sigma, low, high);
    float end = clamp(3.0 * sigma, low, high);
    const int samples = 12;
    float step = (end - start) / float(samples);
    float offset = start + step * 0.5;
    float value = 0.0;
    for (int index = 0; index < samples; index++) {
        float row = centred.y - offset;
        float radius = row > 0.0 ? radii.x : radii.y;
        value += shadowRow(centred.x, row, sigma, radius, halfSize) * gaussian(offset, sigma) * step;
        offset += step;
    }
    return value;
}

void main()
{
    vec2 outline = barDistance(vLocal, vShape);
    float ramp = outline.y / max(pixelsPerPoint, 1.0);
    float body = clamp(0.5 - outline.x / ramp, 0.0, 1.0);
    float glow = 0.0;
    if (glowSigma > 0.0 && glowOpacity > 0.0) {
        glow = clamp(barShadow(vLocal, vShape, glowSigma), 0.0, 1.0) * glowOpacity;
    }
    float alpha = (body + glow * (1.0 - body)) * vColor.a * qt_Opacity * outsideTheLine(vItem);
    fragColor = vec4(vColor.rgb * alpha, alpha);
}
