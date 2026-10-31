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

#include <algorithm>

namespace swiftify::color {

/// end4's ColorUtils.mix: `amount` of `a`, the rest of `b`, alpha included.
inline QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    const qreal t = std::clamp(amount, 0.0, 1.0);
    return QColor::fromRgbF(float(a.redF() * t + b.redF() * (1 - t)),
                            float(a.greenF() * t + b.greenF() * (1 - t)),
                            float(a.blueF() * t + b.blueF() * (1 - t)),
                            float(a.alphaF() * t + b.alphaF() * (1 - t)));
}

inline QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(float(std::clamp(alpha, 0.0, 1.0)));
    return color;
}

/// relative luminance, rec. 709 weights on the gamma-encoded channels, which is
/// what the swift palette code compares against.
inline qreal luminance709(const QColor &color)
{
    return 0.2126 * color.redF() + 0.7152 * color.greenF() + 0.0722 * color.blueF();
}

} // namespace swiftify::color
