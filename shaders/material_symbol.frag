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
// an icon's coverage, read from the plane of the symbol atlas it was filled
// into (MaterialSymbolItem): four planes share each texel, one a channel

layout(location = 0) in vec2 vAtlasPoint;
layout(location = 1) flat in vec4 vPlane;
layout(location = 2) flat in vec4 vColor;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 modelView;
    mat4 projection;
    float opacity;
    float dpr;
};

layout(binding = 1) uniform sampler2D atlas;

void main()
{
    fragColor = vColor * (dot(texture(atlas, vAtlasPoint), vPlane) * opacity);
}
