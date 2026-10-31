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

// builds the distance-field atlas MaterialShape draws from: every Material 3
// Expressive shape, taken from the vendored rounded-polygon-qmljs library as
// cubic outlines (run here through qml's own javascript engine), plus the two
// shapes of the app's own, drawn from the polar formula the shader used to
// work from. each shape gets a cell: the signed distance from its outline,
// in shape units, around the unit square it is normalised into.
//
//   shape_atlas <rounded-polygon-qmljs directory> <output png>

#include "ShapeAtlasLayout.h"

#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QPointF>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantList>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace swiftify::shapeatlas;

namespace {

constexpr double kPi = 3.14159265358979323846;
/// line segments each cubic is flattened into
constexpr int kCubicSteps = 24;
/// points round a polar shape's outline
constexpr int kPolarSteps = 720;

using Polyline = std::vector<QPointF>;

QPointF cubicAt(const double *c, double t)
{
    const double u = 1 - t;
    const double a = u * u * u, b = 3 * u * u * t, d = 3 * u * t * t, e = t * t * t;
    return {a * c[0] + b * c[2] + d * c[4] + e * c[6], a * c[1] + b * c[3] + d * c[5] + e * c[7]};
}

Polyline flatten(const QVariantList &numbers)
{
    Polyline line;
    for (qsizetype start = 0; start + 8 <= numbers.size(); start += 8) {
        double c[8];
        for (int i = 0; i < 8; ++i) {
            c[i] = numbers.at(start + i).toDouble();
        }
        for (int step = 0; step < kCubicSteps; ++step) {
            line.push_back(cubicAt(c, double(step) / kCubicSteps));
        }
    }
    return line;
}

/// material_shape.frag's old polar edge: lobes bulging from a circle, or a
/// superellipse, reaching 1 at its furthest
double polarEdge(const PolarShape &shape, double theta)
{
    if (shape.superellipse) {
        const double c = std::pow(std::abs(std::cos(theta)), shape.sharpness);
        const double s = std::pow(std::abs(std::sin(theta)), shape.sharpness);
        return 1 / std::pow(c + s, 1 / shape.sharpness);
    }
    const double lobe = std::pow(0.5 + 0.5 * std::cos(shape.lobes * theta), shape.sharpness);
    return 1 - shape.depth + shape.depth * lobe;
}

Polyline polar(const PolarShape &shape)
{
    Polyline line;
    for (int step = 0; step < kPolarSteps; ++step) {
        const double theta = 2 * kPi * step / kPolarSteps;
        const double r = 0.5 * polarEdge(shape, theta);
        line.emplace_back(0.5 + r * std::cos(theta), 0.5 + r * std::sin(theta));
    }
    return line;
}

double segmentDistance(QPointF p, QPointF a, QPointF b)
{
    const QPointF ab = b - a;
    const double length = QPointF::dotProduct(ab, ab);
    const double t = length > 0 ? std::clamp(QPointF::dotProduct(p - a, ab) / length, 0.0, 1.0) : 0.0;
    const QPointF nearest = a + t * ab;
    return std::hypot(p.x() - nearest.x(), p.y() - nearest.y());
}

/// negative inside, by the even-odd rule
double signedDistance(QPointF p, const Polyline &line)
{
    double nearest = 1e9;
    bool inside = false;
    for (size_t i = 0, j = line.size() - 1; i < line.size(); j = i++) {
        const QPointF a = line[j], b = line[i];
        nearest = std::min(nearest, segmentDistance(p, a, b));
        if ((a.y() > p.y()) != (b.y() > p.y())
            && p.x() < (b.x() - a.x()) * (p.y() - a.y()) / (b.y() - a.y()) + a.x()) {
            inside = !inside;
        }
    }
    return inside ? -nearest : nearest;
}

void drawCell(QImage &atlas, int index, const Polyline &line)
{
    const int cellX = (index % kColumns) * kCell;
    const int cellY = (index / kColumns) * kCell;
    for (int y = 0; y < kCell; ++y) {
        uchar *row = atlas.scanLine(cellY + y) + cellX;
        for (int x = 0; x < kCell; ++x) {
            const QPointF p((x + 0.5 - kPadding) / kShapeArea, (y + 0.5 - kPadding) / kShapeArea);
            const double d = signedDistance(p, line);
            // 0.5 on the outline, rising inside, falling out to the range
            const double value = std::clamp(0.5 - d / (2 * kRange), 0.0, 1.0);
            row[x] = uchar(std::lround(value * 255));
        }
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() != 3) {
        std::fprintf(stderr, "usage: shape_atlas <rounded-polygon-qmljs directory> <output png>\n");
        return 2;
    }

    QQmlEngine engine;
    QQmlComponent component(&engine);
    // the library's own relative imports resolve from beside it
    component.setData("import QtQml\n"
                      "import \"material-shapes.js\" as MaterialShapes\n"
                      "QtObject {\n"
                      "    function outline(getter) {\n"
                      "        const numbers = []\n"
                      "        for (const cubic of MaterialShapes[getter]().cubics) {\n"
                      "            for (const value of cubic.points) numbers.push(value)\n"
                      "        }\n"
                      "        return numbers\n"
                      "    }\n"
                      "}\n",
                      QUrl::fromLocalFile(QDir(arguments.at(1)).absoluteFilePath(QStringLiteral("atlas.qml"))));
    std::unique_ptr<QObject> library(component.create());
    if (!library) {
        std::fprintf(stderr, "shape_atlas: %s\n", qPrintable(component.errorString()));
        return 1;
    }

    QImage atlas(kColumns * kCell, kRows * kCell, QImage::Format_Grayscale8);
    atlas.fill(0);
    QStringList names;
    int index = 0;
    for (const Entry &entry : entries()) {
        Polyline line;
        if (entry.getter) {
            QVariant numbers;
            QMetaObject::invokeMethod(library.get(), "outline", Q_RETURN_ARG(QVariant, numbers),
                                      Q_ARG(QVariant, QString::fromLatin1(entry.getter)));
            line = flatten(numbers.toList());
        } else {
            line = polar(entry.polar);
        }
        if (line.size() < 3) {
            std::fprintf(stderr, "shape_atlas: no outline for %s\n", entry.name);
            return 1;
        }
        drawCell(atlas, index++, line);
        names << QString::fromLatin1(entry.name);
    }
    // the order, for the test that holds Shapes.qml to it
    atlas.setText(QStringLiteral("names"), names.join(u','));
    // a filled view of every cell, to look at while working on the tool
    if (const QString preview = qEnvironmentVariable("SHAPE_ATLAS_PREVIEW"); !preview.isEmpty()) {
        QImage filled = atlas.copy();
        for (int y = 0; y < filled.height(); ++y) {
            uchar *row = filled.scanLine(y);
            for (int x = 0; x < filled.width(); ++x) {
                row[x] = row[x] >= 128 ? 255 : 0;
            }
        }
        filled.save(preview, "PNG");
    }
    if (!atlas.save(arguments.at(2), "PNG")) {
        std::fprintf(stderr, "shape_atlas: could not write %s\n", qPrintable(arguments.at(2)));
        return 1;
    }
    return 0;
}
