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

#include "artwork/CoverMaker.h"

#include "artwork/PlaylistCovers.h"
#include "spotify/WebApi.h"

#include <QPainter>
#include <QQuickWindow>
#include <QtMath>

#include <algorithm>

namespace swiftify {

PlaylistCoverItem::PlaylistCoverItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &PlaylistCoverItem::coverChanged, this, [this] { update(); });
}

void PlaylistCoverItem::setColors(const QList<QColor> &colors)
{
    if (m_colors == colors) {
        return;
    }
    m_colors = colors;
    emit coverChanged();
}

void PlaylistCoverItem::setDesign(int design)
{
    if (m_design == design) {
        return;
    }
    m_design = design;
    emit coverChanged();
}

void PlaylistCoverItem::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }
    m_title = title;
    emit coverChanged();
}

void PlaylistCoverItem::setRadius(qreal radius)
{
    if (qFuzzyCompare(m_radius, radius)) {
        return;
    }
    m_radius = radius;
    emit coverChanged();
}

void PlaylistCoverItem::paint(QPainter *painter)
{
    const QList<covers::Design> designs = covers::designs(m_colors);
    const covers::Design &design = designs.at(std::clamp(m_design, 0, int(designs.size()) - 1));
    const qreal edge = std::min(width(), height());
    if (edge <= 0) {
        return;
    }
    // drawn whole, then laid in as the fill of a rounded square: a clip path
    // is not antialiased, and the corners would step
    const qreal scale = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const QImage image = covers::render(design, m_title, qCeil(edge * scale));
    QBrush brush(image);
    brush.setTransform(QTransform::fromScale(edge / image.width(), edge / image.height()));
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(brush);
    painter->drawRoundedRect(QRectF(0, 0, edge, edge), m_radius, m_radius);
}

CoverMaker::CoverMaker(QObject *parent)
    : QObject(parent)
{
}

void CoverMaker::setColors(const QList<QColor> &colors)
{
    if (m_colors == colors) {
        return;
    }
    m_colors = colors;
    emit colorsChanged();
}

void CoverMaker::setPhoto(const QUrl &photo)
{
    if (m_photo == photo) {
        return;
    }
    m_photo = photo;
    m_image = photo.isLocalFile() ? covers::loadPhoto(photo.toLocalFile()) : QImage();
    emit photoChanged();
}

QByteArray CoverMaker::designJpeg(int design, const QString &title) const
{
    const QList<covers::Design> designs = covers::designs(m_colors);
    if (design < 0 || design >= designs.size()) {
        return {};
    }
    return covers::jpeg(designs.at(design), title);
}

QSizeF CoverMaker::clampedOffset(QSizeF proposed, qreal viewport, qreal zoom) const
{
    return covers::clampedOffset(proposed, m_image.size(), viewport, zoom);
}

QRectF CoverMaker::cropRect(qreal viewport, qreal zoom, QSizeF offset) const
{
    return covers::cropRect(m_image.size(), viewport, zoom, offset);
}

QByteArray CoverMaker::cropPhoto(qreal viewport, qreal zoom, QSizeF offset) const
{
    if (m_image.isNull()) {
        return {};
    }
    return covers::squareJpeg(m_image, cropRect(viewport, zoom, offset), WebApi::kMaximumCoverBytes);
}

QUrl CoverMaker::preview(const QByteArray &jpeg)
{
    return jpeg.isEmpty() ? QUrl() : QUrl(QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(jpeg.toBase64()));
}

} // namespace swiftify
