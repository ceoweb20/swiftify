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

#include "artwork/PlaylistCovers.h"
#include "artwork/ArtworkCache.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QDir>
#include <QNetworkAccessManager>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QTest>

#include <random>

using namespace swiftify;

/// PlaylistCoverArt's rules: ten designs from any palette, a crop that never
/// leaves a gap, and a jpeg that always fits spotify's 256 kB.
class CoversTest : public QObject {
    Q_OBJECT

private slots:
    void fallbackCacheAndRealArtwork()
    {
        QNetworkAccessManager network;
        ArtworkCache cache(&network);
        const QString identity = QString::fromUtf8("local:Тихий вечер & + # / 🌙");
        QUrl url(QStringLiteral("swiftify-cover:/dark"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("key"), identity);
        url.setQuery(query);
        QImage generated;
        cache.image(url, 40, [&](const QImage &image) { generated = image; });
        QTRY_VERIFY(!generated.isNull());
        QCOMPARE(generated, covers::fallback(identity, true, ArtworkCache::bucket(40)));
        // Same URL and size bucket is a synchronous memory-cache hit.
        bool cached = false;
        cache.image(url, 48, [&](const QImage &image) {
            cached = true;
            QCOMPARE(image, generated);
        });
        QVERIFY(cached);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage photo(64, 64, QImage::Format_ARGB32_Premultiplied);
        photo.fill(QColor("#28518a"));
        const QString path = directory.filePath(QStringLiteral("chosen-cover.png"));
        QVERIFY(photo.save(path));
        QImage loaded;
        cache.image(QUrl::fromLocalFile(path), 64, [&](const QImage &image) { loaded = image; });
        QTRY_VERIFY(!loaded.isNull());
        QCOMPARE(loaded, photo);
        bool failed = false;
        cache.image(QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing.png"))), 64,
                    [&](const QImage &image) { failed = true; QVERIFY(image.isNull()); });
        QTRY_VERIFY(failed);
    }

    void fallbackIsStableAndVaried()
    {
        QSet<QByteArray> sleeves;
        for (int index = 0; index < 24; ++index) {
            const QString key = QStringLiteral("local:playlist-%1").arg(index);
            const QImage image = covers::fallback(key, true, 192);
            QCOMPARE(image, covers::fallback(key, true, 192));
            QCOMPARE(image.size(), QSize(192, 192));
            QVERIFY(image != covers::fallback(key, false, 192));
            sleeves.insert(QByteArray(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()));
            // At navigation sizes both the ink ground and printed shape survive.
            const QImage small = covers::fallback(key, true, 40);
            int minimum = 255;
            int maximum = 0;
            for (int y = 0; y < 40; ++y) {
                for (int x = 0; x < 40; ++x) {
                    const QColor color = small.pixelColor(x, y);
                    QCOMPARE(color.alpha(), 255);
                    minimum = std::min(minimum, qGray(color.rgb()));
                    maximum = std::max(maximum, qGray(color.rgb()));
                }
            }
            QVERIFY(maximum - minimum > 70);
        }
        QCOMPARE(sleeves.size(), 24);
        const QString unicode = QString::fromUtf8("Тихий вечер 🌙");
        QCOMPARE(covers::fallback(unicode, true, 64), covers::fallback(unicode, true, 64));
        QVERIFY(covers::fallback({}, true, 0).isNull());
        QVERIFY(!covers::fallback({}, false, 64).isNull());
    }

    void fallbackContactSheet()
    {
        // Opt-in native QPainter preview, with the production rasterizer and
        // card proportions. No files are written during an ordinary test run.
        const QString directory = qEnvironmentVariable("SWIFTIFY_COVER_PREVIEW_DIR");
        if (directory.isEmpty()) {
            return;
        }
        QVERIFY(QDir().mkpath(directory));
        const QStringList titles = {QStringLiteral("Quiet mornings"), QString::fromUtf8("Тихий вечер"),
                                    QStringLiteral("Side streets"), QStringLiteral("Soft focus"),
                                    QStringLiteral("Sunday ritual"), QStringLiteral("After hours")};
        const QStringList keys = {QStringLiteral("local:playlist-0"), QStringLiteral("local:playlist-1"),
                                  QStringLiteral("local:playlist-2"), QStringLiteral("local:playlist-3"),
                                  QStringLiteral("local:playlist-4"), QStringLiteral("local:playlist-5")};
        for (const bool dark : {true, false}) {
            QImage sheet(1120, 884, QImage::Format_RGB32);
            sheet.fill(QColor(dark ? "#191619" : "#faf4f1"));
            QPainter painter(&sheet);
            QFont font;
            font.setPixelSize(30);
            font.setWeight(QFont::DemiBold);
            painter.setFont(font);
            painter.setPen(QColor(dark ? "#f2e5e9" : "#2b2327"));
            painter.drawText(QRect(48, 30, 1000, 48), Qt::AlignVCenter, QStringLiteral("Your playlists"));
            font.setPixelSize(15);
            painter.setFont(font);
            painter.setPen(QColor(dark ? "#c1b4ba" : "#675c62"));
            painter.drawText(48, 101, QStringLiteral("Personal sleeves. A familiar place for every collection."));
            for (int index = 0; index < 6; ++index) {
                const int x = 48 + index % 3 * 348;
                const int y = 138 + index / 3 * 342;
                painter.save();
                QPainterPath clip;
                clip.addRoundedRect(QRectF(x, y, 240, 240), 17, 17);
                painter.setClipPath(clip);
                painter.drawImage(QRect(x, y, 240, 240), covers::fallback(keys[index], dark, 480));
                painter.restore();
                font.setPixelSize(17);
                font.setWeight(QFont::DemiBold);
                painter.setFont(font);
                painter.setPen(QColor(dark ? "#f2e5e9" : "#2b2327"));
                painter.drawText(x, y + 268, titles[index]);
                font.setPixelSize(13);
                font.setWeight(QFont::Normal);
                painter.setFont(font);
                painter.setPen(QColor(dark ? "#c1b4ba" : "#675c62"));
                painter.drawText(x, y + 290, QStringLiteral("Playlist · 24 songs"));
                painter.drawImage(QRect(x + 258, y + 8, 48, 48), covers::fallback(keys[index], dark, 96));
            }
            painter.end();
            QVERIFY(sheet.save(directory + (dark ? QStringLiteral("/covers-dark.png") : QStringLiteral("/covers-light.png"))));
        }
    }

    void tenDesignsAlternatingGrounds()
    {
        const QList<covers::Design> designs = covers::designs({});
        QCOMPARE(designs.size(), 10);
        for (int index = 0; index < 10; ++index) {
            QCOMPARE(designs.at(index).id, index);
            QCOMPARE(designs.at(index).dark, index % 2 == 0);
            QCOMPARE(designs.at(index).palette.size(), 5);
        }
        // each leads with a different colour
        QVERIFY(designs.at(0).palette.first() != designs.at(1).palette.first());
    }

    void aThinPaletteWidensByHue()
    {
        const QList<QColor> palette = covers::expandedPalette({QColor(40, 160, 90)});
        QCOMPARE(palette.size(), 5);
        QCOMPARE(palette.first(), QColor(40, 160, 90));
        for (int index = 1; index < 5; ++index) {
            QVERIFY(palette.at(index).hsvHueF() != palette.at(index - 1).hsvHueF());
            // never washed out or black
            QVERIFY(palette.at(index).hsvSaturationF() >= 0.41f);
            QVERIFY(palette.at(index).valueF() >= 0.49f);
        }
        // and nothing at all falls back on the accent pair
        QCOMPARE(covers::expandedPalette({}).first(), QColor(0xff, 0x3d, 0x8a));
    }

    void everyDesignDraws()
    {
        for (const covers::Design &design : covers::designs({QColor(200, 60, 40), QColor(30, 90, 200)})) {
            const QImage image = covers::render(design, QStringLiteral("Late Night Drives"), 128);
            QCOMPARE(image.size(), QSize(128, 128));
            // something besides the ground made it onto the cover
            QSet<QRgb> colours;
            for (int y = 0; y < 128; y += 8) {
                for (int x = 0; x < 128; x += 8) {
                    colours.insert(image.pixel(x, y));
                }
            }
            QVERIFY2(colours.size() > 8, qPrintable(QString::number(design.id)));
        }
    }

    void aDesignFitsTheUploadLimit()
    {
        for (const covers::Design &design : covers::designs({})) {
            const QByteArray jpeg = covers::jpeg(design, QStringLiteral("A playlist with rather a long name indeed"));
            QVERIFY(!jpeg.isEmpty());
            QVERIFY(jpeg.size() <= 256'000);
            QBuffer buffer;
            buffer.setData(jpeg);
            QCOMPARE(QImageReader(&buffer).size(), QSize(covers::kEdge, covers::kEdge));
        }
    }

    void theCropNeverOpensAGap()
    {
        const QSizeF image(3000, 2000);
        const qreal viewport = 380;
        // at zoom 1 the short side fills the square: no vertical play at all
        QCOMPARE(covers::clampedOffset({500, 500}, image, viewport, 1), QSizeF(95, 0));
        // zoomed in, the photo may move as far as it overhangs, no further
        const QSizeF limit = covers::clampedOffset({1e6, -1e6}, image, viewport, 2);
        QCOMPARE(limit, QSizeF((3000 * 0.38 - 380) / 2, -(2000 * 0.38 - 380) / 2));
        // the framed square at the far edge sits exactly on the image's edge
        const QRectF crop = covers::cropRect(image, viewport, 2, limit);
        QVERIFY(std::abs(crop.left()) < 1e-6);
        QVERIFY(std::abs(crop.bottom() - 2000) < 1e-6);
        QCOMPARE(crop.width(), crop.height());
        // centred, it is the middle of the image
        QCOMPARE(covers::cropRect(image, viewport, 1, {0, 0}), covers::centredSquare(image));
    }

    void aNoisyPhotoStillFits()
    {
        // noise is the worst case for jpeg; the quality ladder brings it in
        QImage noise(2048, 1536, QImage::Format_RGB32);
        std::mt19937 random(7);
        for (int y = 0; y < noise.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(noise.scanLine(y));
            for (int x = 0; x < noise.width(); ++x) {
                line[x] = qRgb(int(random() % 256), int(random() % 256), int(random() % 256));
            }
        }
        const QByteArray jpeg = covers::squareJpeg(noise, {}, 256'000);
        if (!jpeg.isEmpty()) {
            QVERIFY(jpeg.size() <= 256'000);
        }
        // and an ordinary photo lands first time
        QImage smooth(1200, 900, QImage::Format_RGB32);
        smooth.fill(QColor(120, 140, 160));
        QVERIFY(!covers::squareJpeg(smooth, {}, 256'000).isEmpty());
    }
};

QTEST_MAIN(CoversTest)
#include "tst_covers.moc"
