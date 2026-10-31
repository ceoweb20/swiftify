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

#include "render/SpectrumShaping.h"

#include <algorithm>
#include <cmath>

namespace swiftify::spectrum {

QList<float> bassExpanded(const QList<float> &levels, int perSideCount)
{
    if (levels.isEmpty() || perSideCount <= 0) {
        return {};
    }
    constexpr float bassBarFraction = 0.46f;
    constexpr float bassBandFraction = 0.30f;
    const float lastBand = float(levels.size() - 1);

    QList<float> positions(perSideCount);
    for (int index = 0; index < perSideCount; ++index) {
        const float progress = perSideCount == 1 ? 0.0f : float(index) / float(perSideCount - 1);
        const float source = progress <= bassBarFraction
            ? progress / bassBarFraction * bassBandFraction
            : bassBandFraction + (progress - bassBarFraction) / (1 - bassBarFraction) * (1 - bassBandFraction);
        positions[index] = source * lastBand;
    }

    const auto interpolated = [&](float position) {
        const int lower = std::clamp(int(std::floor(position)), 0, int(levels.size() - 1));
        const int upper = std::min(lower + 1, int(levels.size() - 1));
        const float blend = position - float(lower);
        return std::clamp(levels[lower] * (1 - blend) + levels[upper] * blend, 0.0f, 1.0f);
    };

    QList<float> result(perSideCount);
    for (int index = 0; index < perSideCount; ++index) {
        const float position = positions[index];
        const float lower = index == 0 ? 0.0f : (positions[index - 1] + position) / 2;
        const float upper = index == perSideCount - 1 ? lastBand : (position + positions[index + 1]) / 2;
        if (upper - lower <= 1) {
            result[index] = interpolated(position);
            continue;
        }
        // point sampling would skip a narrow vocal or synth peak entirely.
        float peak = std::max(interpolated(lower), interpolated(upper));
        for (int source = int(std::ceil(lower)); source <= int(std::floor(upper)); ++source) {
            peak = std::max(peak, levels[source]);
        }
        result[index] = std::clamp(peak, 0.0f, 1.0f);
    }
    return result;
}

QList<float> compact(const QList<float> &levels, int count)
{
    if (count <= 0) {
        return {};
    }
    if (levels.isEmpty()) {
        return QList<float>(count, 0.0f);
    }
    const int bands = int(levels.size());
    QList<float> result(count);
    for (int index = 0; index < count; ++index) {
        const int lower = index * bands / count;
        const int upper = std::min(std::max(lower + 1, (index + 1) * bands / count), bands);
        float peak = 0;
        float squares = 0;
        for (int band = lower; band < upper; ++band) {
            peak = std::max(peak, levels[band]);
            squares += levels[band] * levels[band];
        }
        const float rms = std::sqrt(squares / float(std::max(upper - lower, 1)));
        result[index] = std::clamp(rms * 0.72f + peak * 0.28f, 0.0f, 1.0f);
    }
    return result;
}

QList<float> loadingPulse(int count, float phase, float rest)
{
    if (count <= 0) {
        return {};
    }
    // the sweep takes what the rest leaves of the cycle; through the rest the
    // bump waits off the far end, where nothing of it shows
    const float sweeping = 1.0f - std::clamp(rest, 0.0f, 0.95f);
    const float progress = std::clamp(phase / sweeping, 0.0f, 1.0f);
    const float width = std::max(0.9f, float(count) * 0.06f);
    // it comes in from, and goes out to, far enough off either end that
    // none of it shows there: the wait is still
    const float margin = width * 2.5f;
    const float start = -margin;
    const float end = float(count - 1) + margin;
    const float center = start + (end - start) * progress;
    QList<float> result(count);
    for (int index = 0; index < count; ++index) {
        const float distance = std::abs(float(index) - center) / width;
        result[index] = 0.06f + std::exp(-(distance * distance) * 1.8f) * 0.88f;
    }
    return result;
}

QList<float> ringLoadingPulse(int count, float phase, float rest)
{
    if (count <= 0) {
        return {};
    }
    // one side from the top down, and the other its mirror
    const int half = (count + 1) / 2;
    QList<float> ring = loadingPulse(half, phase, rest);
    for (int index = count - half - 1; index >= 0; --index) {
        ring.append(ring[index]);
    }
    return ring;
}

Rgba rgba(const QColor &value)
{
    return {value.redF(), value.greenF(), value.blueF(), value.alphaF()};
}

QColor color(const Rgba &value)
{
    return QColor::fromRgbF(value[0], value[1], value[2], value[3]);
}

} // namespace swiftify::spectrum
