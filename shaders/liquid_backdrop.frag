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
// the immersive player's fluid field (LiquidBackdrop.metal): layered value
// noise, warped twice, and coloured from the cover's palette. the field
// depends on the item's size, not the texture's, so it is drawn small and
// looks the same at any resolution once blurred.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 itemSize;
    float time;
    // sub-bass, bass, upper bass, vocal
    vec4 energy;
    float lead;
    vec4 c0;
    vec4 c1;
    vec4 c2;
    vec4 c3;
    vec4 c4;
};

// value noise. cheap enough to run several octaves per pixel at 30 hz.
float liquidHash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float liquidNoise(vec2 p)
{
    vec2 cell = floor(p);
    vec2 f = fract(p);
    // smoothstep the cell interpolation, so octaves stack without visible grid.
    f = f * f * (3.0 - 2.0 * f);

    float a = liquidHash(cell);
    float b = liquidHash(cell + vec2(1.0, 0.0));
    float c = liquidHash(cell + vec2(0.0, 1.0));
    float d = liquidHash(cell + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec2 liquidRotate(vec2 p, float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float liquidFbm(vec2 p)
{
    float total = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 5; octave++) {
        total += amplitude * liquidNoise(p);
        p *= 2.03;
        amplitude *= 0.5;
    }
    return total;
}

void main()
{
    float subBass = energy.x;
    float bass = energy.y;
    float upperBass = energy.z;
    float vocal = energy.w;

    vec2 position = qt_TexCoord0 * itemSize;
    vec2 uv = position / max(itemSize.y, 1.0);
    vec2 centred = uv - vec2(itemSize.x / max(itemSize.y, 1.0), 1.0) * 0.5;

    // offsetting a noise sample by a constant translates the field.
    vec2 domainA = liquidRotate(centred, time * 0.011) * 1.6;
    vec2 domainB = liquidRotate(centred, time * -0.008) * 1.6;

    float warpQ = 3.2 * (1.0 + 0.30 * sin(time * 0.037)) + (subBass + bass) * 0.14;
    float warpR = 3.2 * (1.0 + 0.30 * cos(time * 0.029)) + upperBass * 0.10;

    vec2 flowA = vec2(sin(time * 0.053), cos(time * 0.041)) * (0.18 + subBass * 0.30);
    vec2 flowB = vec2(cos(time * 0.037), sin(time * 0.061)) * (0.18 + bass * 0.30);
    vec2 flowC = vec2(sin(time * 0.029), sin(time * 0.047)) * (0.16 + upperBass * 0.28);
    vec2 flowD = vec2(cos(time * 0.043), cos(time * 0.031)) * (0.16 + vocal * 0.38);
    vec2 flowE = vec2(sin(time * 0.067), cos(time * 0.023)) * (0.14 + lead * 0.42);

    vec2 q = vec2(liquidFbm(domainA + flowA),
                  liquidFbm(domainA + vec2(5.2, 1.3) + flowB));

    vec2 r = vec2(liquidFbm(domainB + warpQ * q + vec2(1.7, 9.2) + flowC),
                  liquidFbm(domainB + warpQ * q + vec2(8.3, 2.8) + flowD));

    float pattern = liquidFbm(domainA + warpR * r + flowE);

    // layered against three different fields rather than one, so every colour
    // owns a region instead of the later ones washing over the earlier.
    vec4 base = mix(c0, c1, smoothstep(0.22, 0.58, pattern));
    base = mix(base, c2, smoothstep(0.52, 0.86, pattern));
    base = mix(base, c3, smoothstep(0.46, 0.92, r.x));
    base = mix(base, c4, smoothstep(0.50, 0.94, q.y));

    // a bright filament where the warped field turns over on itself, which is
    // what reads as the edge of a poured band.
    float seam = smoothstep(0.86, 1.0, abs(r.x - r.y) * 2.4);
    base += vec4(vec3(seam * 0.5), 0.0);

    // vignette, so the field settles into the dark ground at the edges.
    float falloff = 1.0 - smoothstep(0.35, 0.95, length(qt_TexCoord0 - 0.5));

    vec4 ground = vec4(0.009, 0.009, 0.012, 1.0);
    fragColor = mix(ground, base, 0.35 + 0.4 * falloff) * qt_Opacity;
}
