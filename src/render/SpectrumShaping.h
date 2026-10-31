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

#include <array>

/// the level and colour arithmetic of AudioSpectrumView.swift, kept apart
/// from drawing so it can be tested.
namespace swiftify::spectrum {

/// maps 48 analysis bands onto `perSideCount` bars, giving the bass more of
/// them. a bar covering several bands takes the strongest, so a narrow peak
/// survives the reduction.
QList<float> bassExpanded(const QList<float> &levels, int perSideCount);
/// the six-bar meter: rms with a little of the peak, per group of bands
QList<float> compact(const QList<float> &levels, int count);
/// a gaussian sweeping across the bars while a song loads
/// a bump that sweeps across `count` bars through the first part of a
/// cycle, then rests off the end for the last `rest` of it (0 to 1), all
/// bars at their floor. `phase` runs 0 to 1 over the cycle
QList<float> loadingPulse(int count, float phase, float rest = 0);
/// a ring's: two bumps from the top, down either side at once to meet at the
/// bottom, then the rest. `count` is every bar round the ring, clockwise from
/// the top, as a mirrored layout lays them
QList<float> ringLoadingPulse(int count, float phase, float rest);

using Rgba = std::array<float, 4>;

Rgba rgba(const QColor &color);
QColor color(const Rgba &rgba);

} // namespace swiftify::spectrum
