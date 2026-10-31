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
#include <QObject>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QSizeF>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// one generated cover, drawn at whatever size it is given: the editor's
/// carousel tiles (CoverCanvas)
class PlaylistCoverItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(PlaylistCover)

    Q_PROPERTY(QList<QColor> colors READ colors WRITE setColors NOTIFY coverChanged FINAL)
    Q_PROPERTY(int design READ design WRITE setDesign NOTIFY coverChanged FINAL)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY coverChanged FINAL)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY coverChanged FINAL)

public:
    explicit PlaylistCoverItem(QQuickItem *parent = nullptr);

    QList<QColor> colors() const { return m_colors; }
    void setColors(const QList<QColor> &colors);
    int design() const { return m_design; }
    void setDesign(int design);
    QString title() const { return m_title; }
    void setTitle(const QString &title);
    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);

    void paint(QPainter *painter) override;

signals:
    void coverChanged();

private:
    QList<QColor> m_colors;
    int m_design = 0;
    QString m_title;
    qreal m_radius = 0;
};

/// what the playlist editor uploads: a generated design as a jpeg, or a
/// picked photo, cropped to the square its owner framed (the editor's cover
/// half, and CoverCropper's arithmetic). covers are square and spotify keeps
/// no original, so where the square falls is the person's call.
class CoverMaker : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<QColor> colors READ colors WRITE setColors NOTIFY colorsChanged FINAL)
    Q_PROPERTY(int designCount READ designCount CONSTANT FINAL)
    /// the photo to crop; an empty url lets it go
    Q_PROPERTY(QUrl photo READ photo WRITE setPhoto NOTIFY photoChanged FINAL)
    Q_PROPERTY(bool photoValid READ isPhotoValid NOTIFY photoChanged FINAL)
    /// upright, at most 2048 px on the longer side: what the crop is measured on
    Q_PROPERTY(QSizeF photoSize READ photoSize NOTIFY photoChanged FINAL)

public:
    explicit CoverMaker(QObject *parent = nullptr);

    QList<QColor> colors() const { return m_colors; }
    void setColors(const QList<QColor> &colors);
    int designCount() const { return 10; }
    QUrl photo() const { return m_photo; }
    void setPhoto(const QUrl &photo);
    bool isPhotoValid() const { return !m_image.isNull(); }
    QSizeF photoSize() const { return m_image.size(); }

    Q_INVOKABLE QByteArray designJpeg(int design, const QString &title) const;
    Q_INVOKABLE QSizeF clampedOffset(QSizeF proposed, qreal viewport, qreal zoom) const;
    Q_INVOKABLE QRectF cropRect(qreal viewport, qreal zoom, QSizeF offset) const;
    /// the framed square of the photo, under spotify's size limit; empty if
    /// it cannot be made
    Q_INVOKABLE QByteArray cropPhoto(qreal viewport, qreal zoom, QSizeF offset) const;
    /// a jpeg as something an Image can show
    Q_INVOKABLE static QUrl preview(const QByteArray &jpeg);

signals:
    void colorsChanged();
    void photoChanged();

private:
    QList<QColor> m_colors;
    QUrl m_photo;
    QImage m_image;
};

} // namespace swiftify
