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
// material 3 expressive's progress line: the played part runs as a wave, the
// rest as a straight track, with a gap around the handle. one pass.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    float progress;
    float amplitude;
    float wavelength;
    float phase;
    float thickness;
    float handleWidth;
    float handleHeight;
    float gap;
    vec4 activeColor;
    vec4 trackColor;
    vec4 handleColor;
};

const float TAU = 6.28318530718;

float segmentCoverage(float distanceToLine)
{
    return clamp(0.5 * thickness + 0.5 - distanceToLine, 0.0, 1.0);
}

void main()
{
    vec2 point = vec2(qt_TexCoord0.x * itemSize.x, (qt_TexCoord0.y - 0.5) * itemSize.y);
    float half_ = 0.5 * thickness;
    float handleX = progress * itemSize.x;
    float handleSpace = handleWidth > 0.0 ? gap + 0.5 * handleWidth : 0.0;

    // the played part, clamped so its ends come out round
    float activeEnd = handleX - handleSpace;
    float played = 0.0;
    if (activeEnd > half_) {
        float x = clamp(point.x, half_, activeEnd);
        float angle = TAU * x / wavelength + phase;
        float y = amplitude * sin(angle);
        float slope = amplitude * TAU / wavelength * cos(angle);
        vec2 offset = vec2(point.x - x, (point.y - y) / sqrt(1.0 + slope * slope));
        played = segmentCoverage(length(offset));
    }

    // what is left to play
    float trackStart = handleX + (handleWidth > 0.0 ? handleSpace : gap);
    float track = 0.0;
    if (trackStart < itemSize.x - half_) {
        float x = clamp(point.x, trackStart, itemSize.x - half_);
        track = segmentCoverage(length(vec2(point.x - x, point.y)));
    }

    float handle = 0.0;
    if (handleWidth > 0.0) {
        vec2 q = abs(point - vec2(handleX, 0.0)) - vec2(0.5 * handleWidth, 0.5 * handleHeight) + 0.5 * handleWidth;
        float d = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - 0.5 * handleWidth;
        handle = clamp(0.5 - d, 0.0, 1.0);
    }

    vec4 color = trackColor * track;
    color = color * (1.0 - played) + activeColor * played;
    color = color * (1.0 - handle) + handleColor * handle;
    fragColor = color * qt_Opacity;
}
