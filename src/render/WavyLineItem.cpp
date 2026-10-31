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

#include "render/WavyLineItem.h"

#include "app/Settings.h"
#include "motion/AmbientClock.h"

#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>

#include <cmath>

namespace swiftify {

namespace {

class WaveMaterial : public QSGMaterial {
public:
    WaveMaterial() { setFlag(Blending); }

    QSGMaterialType *type() const override
    {
        static QSGMaterialType kind;
        return &kind;
    }

    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;

    // every divider is the same material: what differs is in the vertices
    int compare(const QSGMaterial *) const override { return 0; }
};

class WaveShader : public QSGMaterialShader {
public:
    WaveShader()
    {
        setShaderFileName(VertexStage, QStringLiteral(":/shaders/wavy_line.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/shaders/wavy_line.frag.qsb"));
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

QSGMaterialShader *WaveMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new WaveShader;
}

struct WaveVertex {
    float x, y;
    /// from the line's left end, across its middle
    float u, v;
    /// its length, the wave's amplitude, wavelength and phase
    float length, amplitude, wavelength, phase;
    /// premultiplied
    float r, g, b, a;
    float thickness;
};

const QSGGeometry::AttributeSet &waveAttributes()
{
    static const QSGGeometry::Attribute attributes[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 4, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
        QSGGeometry::Attribute::createWithAttributeType(3, 4, QSGGeometry::FloatType, QSGGeometry::ColorAttribute),
        QSGGeometry::Attribute::createWithAttributeType(4, 1, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute),
    };
    static const QSGGeometry::AttributeSet set = {5, sizeof(WaveVertex), attributes};
    return set;
}

} // namespace

WavyLineItem::WavyLineItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    updateImplicitHeight();
    if (Settings *settings = Settings::instance()) {
        connect(settings, &Settings::noShapesChanged, this, [this] {
            followSpin();
            update();
        });
    }
}

namespace {

bool plainLines()
{
    const Settings *settings = Settings::instance();
    return settings && settings->noShapes();
}

} // namespace

void WavyLineItem::updateImplicitHeight()
{
    setImplicitHeight(std::ceil(2 * m_amplitude + m_thickness + 2));
}

void WavyLineItem::setColor(const QColor &color)
{
    if (color == m_color) {
        return;
    }
    m_color = color;
    emit colorChanged();
    update();
}

void WavyLineItem::setThickness(qreal thickness)
{
    if (thickness == m_thickness) {
        return;
    }
    m_thickness = thickness;
    emit thicknessChanged();
    updateImplicitHeight();
    update();
}

void WavyLineItem::setAmplitude(qreal amplitude)
{
    if (amplitude == m_amplitude) {
        return;
    }
    m_amplitude = amplitude;
    emit amplitudeChanged();
    updateImplicitHeight();
    update();
}

void WavyLineItem::setWavelength(qreal wavelength)
{
    if (wavelength == m_wavelength) {
        return;
    }
    m_wavelength = wavelength;
    emit wavelengthChanged();
    update();
}

void WavyLineItem::setPhase(qreal phase)
{
    if (phase == m_phase) {
        return;
    }
    m_phase = phase;
    emit phaseChanged();
    update();
}

void WavyLineItem::setPhaseRate(qreal rate)
{
    if (rate == m_phaseRate) {
        return;
    }
    m_phaseRate = rate;
    emit phaseRateChanged();
    followSpin();
    update();
}

void WavyLineItem::followSpin()
{
    // a plain line looks the same wherever its wave would be
    AmbientClock *clock = m_phaseRate != 0 && isVisible() && window() && !plainLines() ? AmbientClock::spin() : nullptr;
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

void WavyLineItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemVisibleHasChanged || change == ItemSceneChange) {
        followSpin();
    }
}

void WavyLineItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

QSGNode *WavyLineItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    const float width = float(this->width());
    const float height = float(this->height());
    if (width <= 0 || height <= 0 || m_color.alpha() == 0 || m_wavelength <= 0) {
        delete oldNode;
        return nullptr;
    }

    auto *node = static_cast<QSGGeometryNode *>(oldNode);
    if (!node) {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(waveAttributes(), 4);
        geometry->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new WaveMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    const qreal moved = m_followed ? m_followed->value() * m_phaseRate : 0;
    const float amplitude = plainLines() ? 0.0f : float(m_amplitude);
    const float phase = float(std::fmod(m_phase + moved, 2 * M_PI));
    const float alpha = m_color.alphaF();
    const float red = m_color.redF() * alpha;
    const float green = m_color.greenF() * alpha;
    const float blue = m_color.blueF() * alpha;
    auto *vertices = static_cast<WaveVertex *>(node->geometry()->vertexData());
    const float corners[4][2] = {{0, 0}, {width, 0}, {0, height}, {width, height}};
    for (int index = 0; index < 4; ++index) {
        const float x = corners[index][0];
        const float y = corners[index][1];
        vertices[index] = {x,     y,     x,     y - height / 2, width, amplitude, float(m_wavelength),
                           phase, red,   green, blue,           alpha, float(m_thickness)};
    }
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

} // namespace swiftify
