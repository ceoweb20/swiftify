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

#include "render/SoftShadowItem.h"

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>

#include <algorithm>

namespace swiftify {

namespace {

class ShadowMaterial : public QSGMaterial {
public:
    ShadowMaterial() { setFlag(Blending); }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType kind;
        return &kind;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    // every shadow is the same material: what differs is in the vertices
    int compare(const QSGMaterial *) const override { return 0; }
};

class ShadowShader : public QSGMaterialShader {
public:
    ShadowShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/soft_shadow.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/soft_shadow.frag.qsb"));
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
};

QSGMaterialShader *ShadowMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new ShadowShader;
}

struct ShadowVertex {
    float x, y;
    /// from the rectangle's centre
    float u, v;
    /// the solid core's half width and height, its corner radius, and how
    /// far past it the shadow fades
    float halfWidth, halfHeight, radius, fade;
    /// premultiplied
    float r, g, b, a;
};

const QSGGeometry::AttributeSet &shadowAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
        QSGGeometry::Attribute::createWithAttributeType(3, 4, QSGGeometry::FloatType, QSGGeometry::ColorAttribute),
    };
    static const QSGGeometry::AttributeSet set = {4, sizeof(ShadowVertex), attributes};
    return set;
}

} // namespace

SoftShadowItem::SoftShadowItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
}

void SoftShadowItem::setRadius(qreal radius)
{
    if (radius == m_radius) {
        return;
    }
    m_radius = radius;
    emit radiusChanged();
    update();
}

void SoftShadowItem::setBlur(qreal blur)
{
    if (blur == m_blur) {
        return;
    }
    m_blur = blur;
    emit blurChanged();
    update();
}

void SoftShadowItem::setOffset(const QVector2D &offset)
{
    if (offset == m_offset) {
        return;
    }
    m_offset = offset;
    emit offsetChanged();
    update();
}

void SoftShadowItem::setColor(const QColor &color)
{
    if (color == m_color) {
        return;
    }
    m_color = color;
    emit colorChanged();
    update();
}

void SoftShadowItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

QSGNode *SoftShadowItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    const float width = float(this->width());
    const float height = float(this->height());
    if (width <= 0 || height <= 0 || m_color.alpha() == 0) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(shadowAttributes(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new ShadowMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    // RectangularShadow's own proportions (qquickrectangularshadow.cpp): its
    // quad reaches `blur` past the rectangle, moved by the offset, and its
    // solid core is drawn in from the edge by 0.8 of the blur and fades over
    // 2.1 of it, its corners rounded 0.75 of the blur less
    const float blur = float(std::max<qreal>(0, m_blur));
    const float fade = 2.1f * blur + 1;
    const float inset = 0.8f * blur + 1;
    const float radius = std::max(0.0f, std::clamp(float(m_radius), 0.0f, std::min(width, height) / 2) - 0.75f * blur);
    const float left = float(m_offset.x()) - blur;
    const float top = float(m_offset.y()) - blur;
    const float right = float(m_offset.x()) + width + blur;
    const float bottom = float(m_offset.y()) + height + blur;
    const float centreX = float(m_offset.x()) + width / 2;
    const float centreY = float(m_offset.y()) + height / 2;
    const float halfWidth = std::max(0.0f, width / 2 - inset);
    const float halfHeight = std::max(0.0f, height / 2 - inset);
    const float alpha = m_color.alphaF();
    const float red = m_color.redF() * alpha;
    const float green = m_color.greenF() * alpha;
    const float blue = m_color.blueF() * alpha;

    auto *vertices = static_cast<ShadowVertex *>(node->geometry()->vertexData());
    const float corners[4][2] = {{left, top}, {right, top}, {left, bottom}, {right, bottom}};
    for (int index = 0; index < 4; ++index) {
        const float x = corners[index][0];
        const float y = corners[index][1];
        vertices[index] = {x, y, x - centreX, y - centreY, halfWidth, halfHeight, radius, fade, red, green, blue, alpha};
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

} // namespace swiftify
