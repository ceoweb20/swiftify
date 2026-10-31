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
// an icon's quad (MaterialSymbolItem). where its glyph sits in the window's
// symbol atlas, which of the atlas's planes it is in and its colour all ride
// in the vertices, so the scene graph draws many icons at once.

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 atlasPoint;
// a 1 in the channel of the plane the glyph is in
layout(location = 2) in vec4 plane;
// premultiplied
layout(location = 3) in vec4 color;

layout(location = 0) out vec2 vAtlasPoint;
layout(location = 1) flat out vec4 vPlane;
layout(location = 2) flat out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 modelView;
    mat4 projection;
    float opacity;
    float dpr;
};

void main()
{
    vAtlasPoint = atlasPoint;
    vPlane = plane;
    vColor = color;
    // on whole pixels, as qt's own text is (textmask.vert): a glyph filled
    // on the pixel grid stays sharp wherever its item falls
    vec4 placed = modelView * position;
    gl_Position = projection * vec4(floor(placed.xyz * dpr + 0.5) / dpr, placed.w);
}
