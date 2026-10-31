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

#include <QColor>
#include <QList>

#include <algorithm>

namespace swiftify {

/// how a visualizer lays out and moves its bars (SpectrumLayerStyle)
struct SpectrumStyle {
    enum class Layout { Compact, Mirrored, Ascending };
    enum class Alignment { Center, Bottom };

    Layout layout = Layout::Compact;
    /// bars for compact and ascending layouts; per side for mirrored ones
    int count = 6;
    float spacing = 2.6f;
    float minimumHeight = 3;
    float baseLevel = 0.08f;
    float levelScale = 0.92f;
    float responseExponent = 1;
    Alignment alignment = Alignment::Center;
    float inactiveOpacity = 0.45f;
    float glowOpacity = 0.32f;
    float glowRadius = 2;
    bool symmetricGradient = false;
    /// release time constant, seconds
    float animationDuration = 0.075f;
    /// rise time constant: a hit has to land before the next poll retargets it
    float riseDuration = 0.025f;
    /// how much of its range a bar loses away from the bass end
    float bassRangeFalloff = 0;
    bool radial = false;
    float innerRadiusRatio = 0.66f;
    float gapRatio = 0.032f;
    /// colours that replace the artwork's
    QList<QColor> paletteOverride;
    /// the field behind the bars, following their tips; 0 draws none
    float haloOpacity = 0;
    /// the halo's soft edge, as a fraction of a ring's radius or a row's height
    float haloBlur = 0.035f;
    bool drawsBars = true;

    int barCount() const { return layout == Layout::Mirrored ? count * 2 : count; }

    SpectrumStyle asWave() const
    {
        SpectrumStyle style = *this;
        style.haloOpacity = 1;
        style.haloBlur = 0.014f;
        style.drawsBars = false;
        return style;
    }

    static SpectrumStyle compact(float height)
    {
        SpectrumStyle style;
        style.minimumHeight = std::min(4.0f, std::max(2.0f, height * 0.2f));
        return style;
    }

    /// the immersive player's band; deliberately the same as the ios one
    static SpectrumStyle immersive()
    {
        SpectrumStyle style;
        style.layout = Layout::Mirrored;
        style.count = 24;
        style.spacing = 2.5f;
        style.minimumHeight = 3;
        style.baseLevel = 0.02f;
        style.levelScale = 1.12f;
        style.responseExponent = 1.1f;
        style.alignment = Alignment::Bottom;
        style.inactiveOpacity = 0.4f;
        style.glowOpacity = 0.48f;
        style.glowRadius = 5;
        style.symmetricGradient = true;
        style.animationDuration = 0.045f;
        style.bassRangeFalloff = 0.12f;
        style.riseDuration = 0.016f;
        return style;
    }

    /// the immersive band as one row, bass on the left
    static SpectrumStyle continuous()
    {
        SpectrumStyle style = immersive();
        style.layout = Layout::Ascending;
        style.count = 48;
        style.symmetricGradient = false;
        return style;
    }

    static SpectrumStyle equalizer()
    {
        // The palette follows PlayerStyle: the cover's extracted colours, or
        // the selected theme when no cover palette is available.
        SpectrumStyle style = immersive();
        style.layout = Layout::Ascending;
        style.count = 40;
        style.glowOpacity = 0.85f;
        style.glowRadius = 9;
        style.symmetricGradient = false;
        return style;
    }

    static SpectrumStyle ring()
    {
        SpectrumStyle style = immersive();
        style.count = 36;
        style.spacing = 2;
        style.minimumHeight = 2;
        style.glowOpacity = 0.5f;
        style.glowRadius = 4;
        style.radial = true;
        return style;
    }

};

} // namespace swiftify
