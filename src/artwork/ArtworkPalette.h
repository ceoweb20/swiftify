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
#include <QImage>
#include <QList>

#include <optional>

namespace swiftify {

/// colours taken from a cover, and how much of the cover each stands for
/// (fractions summing to 1; a colour derived to fill out the set stands for none).
struct WeightedPalette {
    QList<QColor> colors;
    QList<double> weights;
};

/// ArtworkPalette.swift: five colours for the backdrop, the bars and the
/// glows. the lead colour is what the cover is mostly made of; the rest are the
/// strongest colours unlike everything already picked.
namespace palette {

constexpr int kDesiredCount = 5;

std::optional<WeightedPalette> extract(const QImage &cover);
/// decodes `encoded` at thumbnail size first, which is all the palette needs
std::optional<WeightedPalette> extract(const QByteArray &encoded);

} // namespace palette
} // namespace swiftify
