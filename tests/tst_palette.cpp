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

#include <QPainter>
#include <QTest>

#include <algorithm>

using namespace swiftify;

class PaletteTest : public QObject {
    Q_OBJECT

private slots:
    void alwaysReturnsFiveColorsWithWeightsSummingToOne()
    {
        QImage image(300, 300, QImage::Format_RGB32);
        image.fill(QColor(30, 90, 200));
        const auto result = palette::extract(image);
        QVERIFY(result);
        QCOMPARE(result->colors.size(), 5);
        QCOMPARE(result->weights.size(), 5);
        double total = 0;
        for (double weight : result->weights) {
            total += weight;
        }
        QVERIFY(qAbs(total - 1) < 1e-9);
        // a single colour stands for the whole cover; the fillers for none
        QVERIFY(result->weights.first() > 0.99);
    }

    void theLeadColourIsWhatTheCoverIsMostlyMadeOf()
    {
        // mostly muted green with a small vivid red patch
        QImage image(320, 320, QImage::Format_RGB32);
        image.fill(QColor(70, 110, 80));
        QPainter painter(&image);
        painter.fillRect(0, 0, 60, 60, QColor(240, 20, 30));
        painter.end();

        const auto result = palette::extract(image);
        QVERIFY(result);
        const QColor lead = result->colors.first();
        QVERIFY2(lead.green() > lead.red(), qPrintable(lead.name()));
        // the accent is still picked, as a separate colour
        const bool hasRed = std::any_of(result->colors.begin(), result->colors.end(),
                                        [](const QColor &color) { return color.red() > 180 && color.green() < 120; });
        QVERIFY(hasRed);
        QVERIFY(result->weights.first() > result->weights.at(1));
    }

    void darkColoursAreLiftedForDisplay()
    {
        QImage image(64, 64, QImage::Format_RGB32);
        image.fill(QColor(10, 10, 30));
        const auto result = palette::extract(image);
        QVERIFY(result);
        const QColor lead = result->colors.first();
        QVERIFY(std::max({lead.redF(), lead.greenF(), lead.blueF()}) >= 0.3);
    }

    void transparentCoversHaveNoPalette()
    {
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QVERIFY(!palette::extract(image));
    }
};

QTEST_GUILESS_MAIN(PaletteTest)
#include "tst_palette.moc"
