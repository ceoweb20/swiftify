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
#include <QQuickItem>
#include <QVector2D>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// a rounded rectangle's soft shadow or glow, drawn as qt quick effects'
/// RectangularShadow draws one: solid over the rectangle and fading out to
/// nothing `blur` pixels past its edge.
///
/// as a RectangularShadow every one was an effect of its own and a draw of
/// its own, and a shelf of cards drew a shadow under each. here all a
/// shadow's particulars ride in its vertices, so the scene graph draws the
/// shadows that do not overlap something between them as one.
class SoftShadowItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(SoftShadow)

    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged FINAL)
    /// how far past the edge it fades out, in pixels
    Q_PROPERTY(qreal blur READ blur WRITE setBlur NOTIFY blurChanged FINAL)
    /// how far it is moved from the rectangle it falls from
    Q_PROPERTY(QVector2D offset READ offset WRITE setOffset NOTIFY offsetChanged FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)

public:
    explicit SoftShadowItem(QQuickItem *parent = nullptr);

    qreal radius() const { return m_radius; }
    void setRadius(qreal radius);
    qreal blur() const { return m_blur; }
    void setBlur(qreal blur);
    QVector2D offset() const { return m_offset; }
    void setOffset(const QVector2D &offset);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);

signals:
    void radiusChanged();
    void blurChanged();
    void offsetChanged();
    void colorChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    qreal m_radius = 0;
    qreal m_blur = 10;
    QVector2D m_offset;
    QColor m_color = Qt::black;
};

} // namespace swiftify
