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

#include "render/MaterialShapeItem.h"

#include "ShapeAtlasLayout.h"
#include "app/Settings.h"
#include "motion/AmbientClock.h"

#include <QHash>
#include <QMutex>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGTexture>

#include <algorithm>
#include <cmath>

namespace swiftify {

namespace {

/// the atlas as the shaders read it. it holds one channel, a distance, and a
/// texture made from an image has four: so four shapes share each cell, one
/// in each channel, a quarter of the memory the atlas took as it was drawn
QImage packedAtlas()
{
    using namespace shapeatlas;
    QImage drawn(QStringLiteral(":/shapes/shape_atlas.png"));
    drawn.convertTo(QImage::Format_Grayscale8);
    // premultiplied, so nothing multiplies the other three channels by the
    // fourth on the way to the gpu
    QImage packed(kPackedColumns * kCell, kPackedRows * kCell, QImage::Format_RGBA8888_Premultiplied);
    packed.fill(0);
    if (drawn.width() < kColumns * kCell || drawn.height() < kRows * kCell) {
        return packed;
    }
    for (int index = 0; index < int(entries().size()); ++index) {
        const int place = index / 4;
        const int channel = index % 4;
        for (int y = 0; y < kCell; ++y) {
            const uchar *from = drawn.constScanLine((index / kColumns) * kCell + y) + (index % kColumns) * kCell;
            uchar *to = packed.scanLine((place / kPackedColumns) * kCell + y) + (place % kPackedColumns) * kCell * 4 + channel;
            for (int x = 0; x < kCell; ++x) {
                to[x * 4] = from[x];
            }
        }
    }
    return packed;
}

/// one atlas a window, shared by all its shapes so they batch, and let go of
/// with the window's scene graph. windows draw on threads of their own.
QMutex atlasLock;
QHash<QQuickWindow *, QSGTexture *> atlases;

QSGTexture *atlasFor(QQuickWindow *window)
{
    QMutexLocker locker(&atlasLock);
    if (QSGTexture *atlas = atlases.value(window)) {
        return atlas;
    }
    QSGTexture *atlas = window->createTextureFromImage(packedAtlas());
    atlas->setFiltering(QSGTexture::Linear);
    atlas->setHorizontalWrapMode(QSGTexture::ClampToEdge);
    atlas->setVerticalWrapMode(QSGTexture::ClampToEdge);
    atlases.insert(window, atlas);
    QObject::connect(window, &QQuickWindow::sceneGraphInvalidated, window, [window] {
        QMutexLocker locker(&atlasLock);
        delete atlases.take(window);
    }, Qt::DirectConnection);
    return atlas;
}

class ShapeMaterial : public QSGMaterial {
public:
    explicit ShapeMaterial(QSGTexture *atlas)
        : atlas(atlas)
    {
        setFlag(Blending);
    }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType kind;
        return &kind;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    int compare(const QSGMaterial *other) const override
    {
        const QSGTexture *those = static_cast<const ShapeMaterial *>(other)->atlas;
        return atlas == those ? 0 : (atlas < those ? -1 : 1);
    }

    QSGTexture *atlas;
};

class ShapeShader : public QSGMaterialShader {
public:
    ShapeShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/material_shape.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/material_shape.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override
    {
        bool changed = false;
        char *data = state.uniformData()->data();
        if (state.isMatrixDirty()) {
            memcpy(data, state.combinedMatrix().constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            memcpy(data + 64, &opacity, 4);
            changed = true;
        }
        return changed;
    }

    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture, QSGMaterial *newMaterial,
                            QSGMaterial *) override
    {
        if (binding != 1) {
            return;
        }
        QSGTexture *atlas = static_cast<ShapeMaterial *>(newMaterial)->atlas;
        // uploaded the first time it is drawn, as qt's own materials do it
        atlas->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = atlas;
    }
};

QSGMaterialShader *ShapeMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new ShapeShader;
}

struct ShapeVertex {
    float x, y;
    /// where in the shape's own unit square, already turned
    float u, v;
    /// the shape's side in pixels, the morph, and the two cells
    float side, morph, cellA, cellB;
    /// premultiplied
    float r, g, b, a;
    /// where the vertex is in the parent's space, and the parent's width,
    /// height and corner radius; a radius below 0 leaves it uncut
    float clipX, clipY;
    float clipWidth, clipHeight, clipRadius, clipping;
};

const QSGGeometry::AttributeSet &shapeAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
        QSGGeometry::Attribute::createWithAttributeType(3, 4, QSGGeometry::FloatType, QSGGeometry::ColorAttribute),
        QSGGeometry::Attribute::createWithAttributeType(4, 2, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
        QSGGeometry::Attribute::createWithAttributeType(5, 4, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
    };
    static const QSGGeometry::AttributeSet set = {6, sizeof(ShapeVertex), attributes};
    return set;
}

} // namespace

MaterialShapeItem::MaterialShapeItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    if (Settings *settings = Settings::instance()) {
        connect(settings, &Settings::noShapesChanged, this, [this] {
            followSpin();
            update();
        });
    }
}

bool MaterialShapeItem::isPlain() const
{
    const Settings *settings = Settings::instance();
    return !m_keepsShape && settings && settings->noShapes();
}

void MaterialShapeItem::setKeepsShape(bool keeps)
{
    if (keeps == m_keepsShape) {
        return;
    }
    m_keepsShape = keeps;
    emit keepsShapeChanged();
    followSpin();
    update();
}

int MaterialShapeItem::cellOf(const QString &name)
{
    static const QHash<QString, int> cells = [] {
        QHash<QString, int> byName;
        const auto &entries = shapeatlas::entries();
        for (int index = 0; index < int(entries.size()); ++index) {
            byName.insert(QString::fromLatin1(entries[index].name), index);
        }
        return byName;
    }();
    return cells.value(name, 0);
}

void MaterialShapeItem::setShape(const QString &shape)
{
    if (shape == m_shape) {
        return;
    }
    const QString before = morphTo();
    m_shape = shape;
    m_cellA = cellOf(shape);
    if (m_morphTo.isNull()) {
        m_cellB = m_cellA;
    }
    emit shapeChanged();
    if (morphTo() != before) {
        emit morphToChanged();
    }
    update();
}

void MaterialShapeItem::setMorphTo(const QString &shape)
{
    // an empty name is one like any other unknown one, a circle; only a reset
    // has it follow the shape again
    const QString named = shape.isNull() ? QStringLiteral("") : shape;
    if (!m_morphTo.isNull() && named == m_morphTo) {
        return;
    }
    const QString before = morphTo();
    m_morphTo = named;
    m_cellB = cellOf(named);
    if (morphTo() != before) {
        emit morphToChanged();
    }
    update();
}

void MaterialShapeItem::resetMorphTo()
{
    if (m_morphTo.isNull()) {
        return;
    }
    const QString before = morphTo();
    m_morphTo = QString();
    m_cellB = m_cellA;
    if (morphTo() != before) {
        emit morphToChanged();
    }
    update();
}

void MaterialShapeItem::setMorph(qreal morph)
{
    if (qFuzzyCompare(morph + 1, m_morph + 1)) {
        return;
    }
    m_morph = morph;
    emit morphChanged();
    update();
}

void MaterialShapeItem::setColor(const QColor &color)
{
    if (color == m_color) {
        return;
    }
    m_color = color;
    emit colorChanged();
    update();
}

void MaterialShapeItem::setSpinAngle(qreal degrees)
{
    if (degrees == m_spinAngle) {
        return;
    }
    m_spinAngle = degrees;
    emit spinAngleChanged();
    update();
}

void MaterialShapeItem::setSpinRate(qreal rate)
{
    if (rate == m_spinRate) {
        return;
    }
    m_spinRate = rate;
    emit spinRateChanged();
    followSpin();
    update();
}

void MaterialShapeItem::followSpin()
{
    // a circle looks the same however it is turned
    AmbientClock *clock = m_spinRate != 0 && isVisible() && window() && !isPlain() ? AmbientClock::spin() : nullptr;
    if (clock == m_followed) {
        return;
    }
    if (m_followed) {
        disconnect(m_followed, nullptr, this, nullptr);
    }
    m_followed = clock;
    if (clock) {
        connect(clock, &AmbientClock::valueChanged, this, &QQuickItem::update);
        update();
    }
}

void MaterialShapeItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged || change == ItemSceneChange) {
        followSpin();
    }
    if (change == ItemParentHasChanged) {
        watchParent();
        update();
    }
}

void MaterialShapeItem::setParentClipRadius(qreal radius)
{
    if (radius == m_parentClipRadius) {
        return;
    }
    m_parentClipRadius = radius;
    emit parentClipRadiusChanged();
    watchParent();
    update();
}

void MaterialShapeItem::watchParent()
{
    disconnect(m_parentWidth);
    disconnect(m_parentHeight);
    QQuickItem *parent = parentItem();
    if (m_parentClipRadius < 0 || !parent) {
        return;
    }
    m_parentWidth = connect(parent, &QQuickItem::widthChanged, this, &QQuickItem::update);
    m_parentHeight = connect(parent, &QQuickItem::heightChanged, this, &QQuickItem::update);
}

void MaterialShapeItem::setExtent(qreal extent)
{
    if (extent == m_extent) {
        return;
    }
    m_extent = extent;
    emit extentChanged();
    update();
}

void MaterialShapeItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    // cut to its parent, where it sits in it matters too
    if (newGeometry.size() != oldGeometry.size() || (m_parentClipRadius >= 0 && newGeometry != oldGeometry)) {
        update();
    }
}

QSGNode *MaterialShapeItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    const float width = float(this->width());
    const float height = float(this->height());
    const float shortest = std::min(width, height);
    // the side of the shape's own square, in pixels. the item may be bigger
    // than the shape, to leave room for a spring to carry it past its size
    const float side = float(m_extent) * (shortest - 1.5f);
    if (side <= 0 || m_color.alpha() == 0) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(shapeAttributes(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new ShapeMaterial(atlasFor(window())));
        node->setFlag(QSGNode::OwnsMaterial);
    }

    // turned, a square's corners reach out to half its diagonal, past the
    // item's edge. so a shape that is turned draws on a quad grown to hold
    // its corners at any angle, with a little over for a spring's overshoot
    const bool plain = isPlain();
    const qreal turn = plain ? 0 : std::fmod(m_spinAngle + (m_followed ? m_followed->value() * m_spinRate : 0), 360.0);
    const bool turned = turn != 0;
    const float pad = turned ? std::max(0.0f, std::ceil(side * 0.7071f * 1.08f + 1 - shortest / 2)) : 0;
    const float spin = float(turn * M_PI / 180.0);
    const float cosine = std::cos(spin);
    const float sine = std::sin(spin);
    const float alpha = m_color.alphaF();
    const float red = m_color.redF() * alpha;
    const float green = m_color.greenF() * alpha;
    const float blue = m_color.blueF() * alpha;
    static const int circle = cellOf(QStringLiteral("circle"));
    const float cellA = float(plain ? circle : m_cellA);
    const float cellB = float(plain ? circle : m_cellB);
    const float morph = plain ? 0.0f : float(m_morph);

    QQuickItem *parent = parentItem();
    const bool cut = m_parentClipRadius >= 0 && parent;
    const float clipWidth = cut ? float(parent->width()) : 0;
    const float clipHeight = cut ? float(parent->height()) : 0;
    const float clipRadius = cut ? float(std::min<qreal>(m_parentClipRadius, std::min(clipWidth, clipHeight) / 2)) : 0;

    auto *vertices = static_cast<ShapeVertex *>(node->geometry()->vertexData());
    const float corners[4][2] = {{-pad, -pad}, {width + pad, -pad}, {-pad, height + pad}, {width + pad, height + pad}};
    for (int index = 0; index < 4; ++index) {
        const float x = corners[index][0];
        const float y = corners[index][1];
        const float across = x - width / 2;
        const float down = y - height / 2;
        // turned back by the spin, so the shape turns forward by it
        const float u = (cosine * across + sine * down) / side + 0.5f;
        const float v = (-sine * across + cosine * down) / side + 0.5f;
        const QPointF inParent = cut ? mapToItem(parent, QPointF(x, y)) : QPointF();
        vertices[index] = {x, y, u, v, side, morph, cellA, cellB, red, green, blue, alpha,
                           float(inParent.x()), float(inParent.y()), clipWidth, clipHeight, clipRadius, cut ? 1.0f : 0.0f};
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

} // namespace swiftify
