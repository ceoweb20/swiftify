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

#include "artwork/ArtworkPalette.h"

#include <QBuffer>
#include <QHash>
#include <QImageReader>

#include <algorithm>
#include <cmath>
#include <limits>

namespace swiftify::palette {

namespace {

// fine enough that a small, vivid detail survives as its own colour.
constexpr int kGridSize = 32;
constexpr double kMinimumSeparation = 0.14;

struct Pixel {
    double red = 0;
    double green = 0;
    double blue = 0;

    int quantizedKey() const
    {
        return int(std::floor(red * 5)) << 16 | int(std::floor(green * 5)) << 8 | int(std::floor(blue * 5));
    }

    double paletteScore(double chromaWeight) const
    {
        const double luminance = red * 0.2126 + green * 0.7152 + blue * 0.0722;
        const double chroma = std::max({red, green, blue}) - std::min({red, green, blue});
        const double balancedExposure = std::max(0.15, 1 - std::abs(luminance - 0.55) * 1.6);
        return (0.3 + chroma * chromaWeight) * (0.45 + balancedExposure);
    }

    Pixel clamped() const
    {
        return {std::clamp(red, 0.0, 1.0), std::clamp(green, 0.0, 1.0), std::clamp(blue, 0.0, 1.0)};
    }

    Pixel mixed(const Pixel &other, double amount) const
    {
        const double t = std::clamp(amount, 0.0, 1.0);
        return {red + (other.red - red) * t, green + (other.green - green) * t, blue + (other.blue - blue) * t};
    }

    Pixel adjustedForDisplay() const
    {
        const double average = (red + green + blue) / 3;
        Pixel adjusted = Pixel {average + (red - average) * 1.25, average + (green - average) * 1.25,
                                average + (blue - average) * 1.25}
                             .clamped();
        const double brightness = std::max({adjusted.red, adjusted.green, adjusted.blue});
        if (brightness < 0.48) {
            adjusted = adjusted.mixed({1, 1, 1}, (0.48 - brightness) * 0.8);
        } else if (brightness > 0.94) {
            adjusted = adjusted.mixed({0.72, 0.72, 0.72}, 0.2);
        }
        return adjusted.clamped();
    }

    double distance(const Pixel &other) const
    {
        const double r = red - other.red;
        const double g = green - other.green;
        const double b = blue - other.blue;
        return std::sqrt((r * r + g * g + b * b) / 3);
    }

    QColor color() const { return QColor::fromRgbF(float(red), float(green), float(blue)); }
};

struct Bucket {
    int count = 0;
    double red = 0;
    double green = 0;
    double blue = 0;

    void add(const Pixel &pixel)
    {
        ++count;
        red += pixel.red;
        green += pixel.green;
        blue += pixel.blue;
    }

    Pixel average() const
    {
        const double divisor = std::max(count, 1);
        return {red / divisor, green / divisor, blue / divisor};
    }
};

struct Candidate {
    Pixel pixel;
    double prominence = 0;
    double score = 0;
};

/// how far a candidate sits from the nearest colour already chosen, so a
/// strong near-duplicate loses to a weaker, genuinely different one.
double diversityScore(const Candidate &candidate, const QList<Pixel> &picked)
{
    double separation = std::numeric_limits<double>::max();
    for (const Pixel &pixel : picked) {
        separation = std::min(separation, pixel.distance(candidate.pixel));
    }
    if (picked.isEmpty()) {
        separation = 0;
    }
    return candidate.score * (0.35 + separation * 2.8);
}

} // namespace

std::optional<WeightedPalette> extract(const QImage &cover)
{
    if (cover.isNull()) {
        return std::nullopt;
    }
    const QImage grid = cover.convertToFormat(QImage::Format_RGBA8888_Premultiplied)
                            .scaled(kGridSize, kGridSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QHash<int, Bucket> buckets;
    QList<Pixel> samples;
    samples.reserve(kGridSize * kGridSize);
    for (int row = 0; row < grid.height(); ++row) {
        const uchar *line = grid.constScanLine(row);
        for (int column = 0; column < grid.width(); ++column) {
            const uchar *rgba = line + column * 4;
            if (rgba[3] <= 127) {
                continue;
            }
            const Pixel pixel {rgba[0] / 255.0, rgba[1] / 255.0, rgba[2] / 255.0};
            buckets[pixel.quantizedKey()].add(pixel);
            samples.append(pixel);
        }
    }
    if (buckets.isEmpty()) {
        return std::nullopt;
    }

    QList<Candidate> candidates;
    for (const Bucket &bucket : std::as_const(buckets)) {
        const Pixel pixel = bucket.average();
        const double count = bucket.count;
        // area counts with diminishing returns for the rest of the set, so a
        // large muted field cannot outweigh every accent on size alone.
        candidates.append({pixel, pixel.paletteScore(1.7) * count, pixel.paletteScore(3) * std::sqrt(count)});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) { return a.score > b.score; });

    // the lead colour is what the cover is mostly made of, so a small vivid
    // accent must not take its place.
    const auto primary = std::max_element(candidates.begin(), candidates.end(),
                                          [](const Candidate &a, const Candidate &b) { return a.prominence < b.prominence; });
    QList<Pixel> picked {primary->pixel};

    // farthest-point selection: each pick is the strongest candidate that is
    // also unlike everything already chosen.
    while (picked.size() < kDesiredCount) {
        const Candidate *next = nullptr;
        double bestScore = -1;
        for (const Candidate &candidate : candidates) {
            const bool separated = std::all_of(picked.begin(), picked.end(), [&](const Pixel &pixel) {
                return pixel.distance(candidate.pixel) >= kMinimumSeparation;
            });
            if (!separated) {
                continue;
            }
            const double score = diversityScore(candidate, picked);
            if (!next || score > bestScore) {
                next = &candidate;
                bestScore = score;
            }
        }
        if (!next) {
            break;
        }
        picked.append(next->pixel);
    }

    // every sample counts toward the pick nearest it, which keeps a mostly
    // white cover reading as mostly white rather than as its one red accent.
    QList<double> shares(picked.size(), 0.0);
    for (const Pixel &sample : samples) {
        qsizetype nearest = 0;
        for (qsizetype index = 1; index < picked.size(); ++index) {
            if (picked.at(index).distance(sample) < picked.at(nearest).distance(sample)) {
                nearest = index;
            }
        }
        shares[nearest] += 1;
    }
    double total = 0;
    for (double share : shares) {
        total += share;
    }
    total = std::max(total, 1.0);

    // artwork that genuinely holds few colours still needs a full set, so the
    // remainder is derived from what was found rather than invented.
    QList<Pixel> palette;
    for (const Pixel &pixel : picked) {
        palette.append(pixel.adjustedForDisplay());
    }
    double tintAmount = 0.28;
    while (palette.size() < kDesiredCount) {
        const Pixel source = palette.at(palette.size() % std::max<qsizetype>(picked.size(), 1));
        const Pixel blend = palette.size() % 2 == 0 ? Pixel {1, 1, 1} : Pixel {0, 0, 0};
        palette.append(source.mixed(blend, tintAmount).adjustedForDisplay());
        tintAmount = std::min(0.55, tintAmount + 0.12);
    }

    WeightedPalette result;
    for (const Pixel &pixel : palette) {
        result.colors.append(pixel.color());
    }
    for (double share : shares) {
        result.weights.append(share / total);
    }
    while (result.weights.size() < result.colors.size()) {
        result.weights.append(0);
    }
    return result;
}

std::optional<WeightedPalette> extract(const QByteArray &encoded)
{
    QBuffer buffer;
    buffer.setData(encoded);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid() && std::max(size.width(), size.height()) > 4 * kGridSize) {
        // decode near the size the grid needs; the final smooth scale does the rest.
        reader.setScaledSize(size.scaled(4 * kGridSize, 4 * kGridSize, Qt::KeepAspectRatio));
    }
    return extract(reader.read());
}

} // namespace swiftify::palette
