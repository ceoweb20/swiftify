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
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

class AmbientClock;

/// one of material 3 expressive's shapes (Shapes.qml), filled, antialiased by
/// its distance field from the shape atlas, and morphing into another as
/// `morph` goes 0 to 1. a spring that carries `morph` past 1 exaggerates the
/// second shape for a moment, which reads as the shape bouncing into place.
///
/// every shape reads the one atlas, and what sets one apart from another
/// (which shapes, how far between them, its turn, its colour) rides in its
/// four vertices, so the scene graph draws all the shapes in a window that
/// do not overlap something else between them as one batch. as a
/// ShaderEffect each was a draw of its own, set up again on every frame the
/// window drew, and the window draws every frame while music plays.
class MaterialShapeItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(MaterialShape)

    Q_PROPERTY(QString shape READ shape WRITE setShape NOTIFY shapeChanged FINAL)
    /// the shape it morphs into; the shape itself until one is given
    Q_PROPERTY(QString morphTo READ morphTo WRITE setMorphTo RESET resetMorphTo NOTIFY morphToChanged FINAL)
    /// 0 is `shape`, 1 is `morphTo`; past 1 overshoots
    Q_PROPERTY(qreal morph READ morph WRITE setMorph NOTIFY morphChanged FINAL)
    /// transparent, and so not drawn, until one is given
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    /// degrees
    Q_PROPERTY(qreal spinAngle READ spinAngle WRITE setSpinAngle NOTIFY spinAngleChanged FINAL)
    /// degrees it turns on from spinAngle for each unit of the music's spin
    /// (UiState.spin), following the clock itself while it shows: a binding
    /// on the spin was script run on every tick for every shape that turned
    Q_PROPERTY(qreal spinRate READ spinRate WRITE setSpinRate NOTIFY spinRateChanged FINAL)
    /// the share of the item the shape fills; under 1 leaves room to overshoot
    Q_PROPERTY(qreal extent READ extent WRITE setExtent NOTIFY extentChanged FINAL)
    /// drawn as its shape even with no shapes (Settings.noShapes), when
    /// every other shape is a plain circle: the favourite songs' heart
    Q_PROPERTY(bool keepsShape READ keepsShape WRITE setKeepsShape NOTIFY keepsShapeChanged FINAL)
    /// cut to its parent's bounds, the corners rounded this much, as the
    /// rounded card or panel it sits in looks: clip cuts square, and a shape
    /// reaching into a corner showed past the rounding. below 0 it is left
    /// whole. cut in its own shader, so the shapes still draw as one batch
    Q_PROPERTY(qreal parentClipRadius READ parentClipRadius WRITE setParentClipRadius NOTIFY parentClipRadiusChanged FINAL)

public:
    explicit MaterialShapeItem(QQuickItem *parent = nullptr);

    /// a shape's cell in the atlas; an unknown name is the circle's
    static int cellOf(const QString &name);

    QString shape() const { return m_shape; }
    void setShape(const QString &shape);
    QString morphTo() const { return m_morphTo.isNull() ? m_shape : m_morphTo; }
    void setMorphTo(const QString &shape);
    void resetMorphTo();
    qreal morph() const { return m_morph; }
    void setMorph(qreal morph);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    qreal spinAngle() const { return m_spinAngle; }
    void setSpinAngle(qreal degrees);
    qreal spinRate() const { return m_spinRate; }
    void setSpinRate(qreal rate);
    qreal extent() const { return m_extent; }
    void setExtent(qreal extent);
    bool keepsShape() const { return m_keepsShape; }
    void setKeepsShape(bool keeps);
    qreal parentClipRadius() const { return m_parentClipRadius; }
    void setParentClipRadius(qreal radius);

signals:
    void shapeChanged();
    void morphToChanged();
    void morphChanged();
    void colorChanged();
    void spinAngleChanged();
    void spinRateChanged();
    void extentChanged();
    void keepsShapeChanged();
    void parentClipRadiusChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;

private:
    /// follows the spin clock while it turns and shows, and only then
    void followSpin();
    /// a plain circle, as every shape is with no shapes but those it keeps
    bool isPlain() const;
    /// redrawn as its parent's size changes, while it is cut to it
    void watchParent();

    QString m_shape = QStringLiteral("circle");
    /// null until set, while it follows the shape
    QString m_morphTo;
    int m_cellA = 0;
    int m_cellB = 0;
    qreal m_morph = 0;
    QColor m_color = Qt::transparent;
    qreal m_spinAngle = 0;
    qreal m_spinRate = 0;
    QPointer<AmbientClock> m_followed;
    qreal m_extent = 1;
    bool m_keepsShape = false;
    qreal m_parentClipRadius = -1;
    QMetaObject::Connection m_parentWidth;
    QMetaObject::Connection m_parentHeight;
};

} // namespace swiftify
