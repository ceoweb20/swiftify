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

#pragma once

#include <array>

// how the shape atlas is laid out, shared by the tool that draws it, the
// tests that check it against Shapes.qml, and MaterialShapeItem, which finds
// a shape's cell by its place here. material_shape.frag works the same
// numbers out for itself.
namespace swiftify::shapeatlas {

/// a cell's side, in texels
constexpr int kCell = 128;
/// the margin round a shape inside its cell, for its outer distances
constexpr int kPadding = 14;
/// the unit square the shape is normalised into
constexpr int kShapeArea = kCell - 2 * kPadding;
/// the furthest distance a cell holds, in shape units
constexpr double kRange = double(kPadding) / kShapeArea;
constexpr int kColumns = 7;
constexpr int kRows = 6;
/// as the app packs it, four shapes to a cell, one in each channel
/// (MaterialShapeItem, material_shape.frag)
constexpr int kPackedColumns = 4;
constexpr int kPackedRows = 3;

struct PolarShape {
    double lobes = 0;
    double depth = 0;
    double sharpness = 1;
    bool superellipse = false;
};

struct Entry {
    const char *name;
    /// the material-shapes.js getter, or null for a polar shape of our own
    const char *getter;
    PolarShape polar;
};

/// in atlas order: material 3's 35, then the app's own
inline const std::array<Entry, 37> &entries()
{
    static const std::array<Entry, 37> list {{
        {"circle", "getCircle", {}},
        {"square", "getSquare", {}},
        {"slanted", "getSlanted", {}},
        {"arch", "getArch", {}},
        {"fan", "getFan", {}},
        {"arrow", "getArrow", {}},
        {"semiCircle", "getSemiCircle", {}},
        {"oval", "getOval", {}},
        {"pill", "getPill", {}},
        {"triangle", "getTriangle", {}},
        {"diamond", "getDiamond", {}},
        {"clamShell", "getClamShell", {}},
        {"pentagon", "getPentagon", {}},
        {"gem", "getGem", {}},
        {"sunny", "getSunny", {}},
        {"verySunny", "getVerySunny", {}},
        {"cookie4", "getCookie4Sided", {}},
        {"cookie6", "getCookie6Sided", {}},
        {"cookie7", "getCookie7Sided", {}},
        {"cookie9", "getCookie9Sided", {}},
        {"cookie12", "getCookie12Sided", {}},
        {"ghostish", "getGhostish", {}},
        {"clover4", "getClover4Leaf", {}},
        {"clover8", "getClover8Leaf", {}},
        {"burst", "getBurst", {}},
        {"softBurst", "getSoftBurst", {}},
        {"boom", "getBoom", {}},
        {"softBoom", "getSoftBoom", {}},
        {"flower", "getFlower", {}},
        {"puffy", "getPuffy", {}},
        {"puffyDiamond", "getPuffyDiamond", {}},
        {"pixelCircle", "getPixelCircle", {}},
        {"pixelTriangle", "getPixelTriangle", {}},
        {"bun", "getBun", {}},
        {"heart", "getHeart", {}},
        // a paused play button: fewer bumps than a cookie, and gentler ones
        {"softCookie", nullptr, {5, 0.06, 1.15, false}},
        {"squircle", nullptr, {0, 0, 4, true}},
    }};
    return list;
}

} // namespace swiftify::shapeatlas
