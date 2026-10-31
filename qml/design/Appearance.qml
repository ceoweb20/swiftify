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

pragma Singleton
pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

// design tokens, after end4's Appearance.qml: rounding, type, motion curves and
// sizes. colours live in Theme (C++), which follows the system scheme.
QtObject {
    id: root

    final readonly property MaterialScheme colors: Theme.colors
    /// nothing moves of its own accord, over and over: the shapes that turn
    /// with the music, the edges that run while hovered, the seek line's wave
    /// and the backdrop's flow hold still (Settings.reduceMotion). what
    /// answers a click, a hover or a change still moves. performance mode
    /// includes it
    final readonly property bool reduceMotion: Settings.reduceMotion || Settings.performanceMode
    /// as little drawn as can be: nothing animates, not even fades
    /// (Settings.performanceMode)
    final readonly property bool performanceMode: Settings.performanceMode

    component Rounding: QtObject {
        final readonly property int unsharpen: 2
        final readonly property int unsharpenMore: 6
        final readonly property int verySmall: 8
        final readonly property int small: 12
        final readonly property int normal: 17
        final readonly property int large: 23
        final readonly property int veryLarge: 30
        final readonly property int full: 9999
        final readonly property int window: 18
    }
    final readonly property Rounding rounding: Rounding {}

    component FontSizes: QtObject {
        final readonly property int smallest: 10
        final readonly property int smaller: 12
        final readonly property int smallie: 13
        final readonly property int small: 15
        final readonly property int normal: 16
        final readonly property int large: 17
        final readonly property int larger: 19
        final readonly property int huge: 22
        final readonly property int hugeAss: 23
        final readonly property int title: 22
        final readonly property int headline: 30
        final readonly property int display: 36
    }
    final readonly property FontSizes fontSize: FontSizes {}

    // end4's google sans flex when installed (see Fonts.cpp for the fallbacks).
    final readonly property string fontFamily: Theme.fontFamily
    final readonly property string monoFontFamily: Theme.monoFontFamily
    final readonly property string iconFontFamily: "Material Symbols Rounded"

    // end4's animationCurves. bezier control points for Easing.BezierSpline.
    component Curves: QtObject {
        final readonly property list<real> expressiveFastSpatial: [0.42, 1.67, 0.21, 0.90, 1, 1]
        final readonly property list<real> expressiveDefaultSpatial: [0.38, 1.21, 0.22, 1.00, 1, 1]
        final readonly property list<real> expressiveSlowSpatial: [0.39, 1.29, 0.35, 0.98, 1, 1]
        final readonly property list<real> expressiveEffects: [0.34, 0.80, 0.34, 1.00, 1, 1]
        final readonly property list<real> emphasized: [0.05, 0, 2 / 15, 0.06, 1 / 6, 0.4, 5 / 24, 0.82, 0.25, 1, 1, 1]
        final readonly property list<real> emphasizedAccel: [0.3, 0, 0.8, 0.15, 1, 1]
        final readonly property list<real> emphasizedDecel: [0.05, 0.7, 0.1, 1, 1, 1]
        final readonly property list<real> standard: [0.2, 0, 0, 1, 1, 1]
        final readonly property list<real> standardAccel: [0.3, 0, 1, 1, 1, 1]
        final readonly property list<real> standardDecel: [0, 0, 0, 1, 1, 1]
    }
    final readonly property Curves curves: Curves {}

    /// a fade's or a colour's duration in ms; performance mode collapses it
    function duration(milliseconds: int): int {
        return performanceMode ? 0 : milliseconds
    }

    /// a move's, a growth's or a turn's duration in ms; performance mode
    /// collapses it. reduced motion keeps it: it stills only what loops
    function motion(milliseconds: int): int {
        return performanceMode ? 0 : milliseconds
    }

    /// a state colour to ease to or from. "transparent" is black with no
    /// alpha, and a colour animation from it to a light colour runs through
    /// grey, a flash of dark on the way in; clear, it takes `other`'s colour
    /// at no alpha instead, so the ease only fades
    function fadeable(color: color, other: color): color {
        return color.a > 0 ? color : Qt.alpha(other, 0)
    }

    component Sizes: QtObject {
        final readonly property real elevationMargin: 10
        final readonly property real pageMargin: 32
        final readonly property real sectionGap: 32
        final readonly property real shelfGap: 16
        final readonly property real cardSize: 176
        final readonly property real headerArtwork: 232
        final readonly property real artworkRadius: 12
        final readonly property real smallArtworkRadius: 7
        final readonly property real sideSheetWidth: 320
        final readonly property real topBarHeight: 64
        final readonly property real playerBarClearance: 120
    }
    final readonly property Sizes sizes: Sizes {}
}
