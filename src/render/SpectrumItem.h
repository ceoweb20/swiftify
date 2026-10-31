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

#include "render/SpectrumShaping.h"
#include "render/SpectrumStyle.h"

#include <QElapsedTimer>

#include <array>
#include <QQuickItem>
#include <QVariantAnimation>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// every visualizer in the app (SpectrumLayerRenderer + SpectrumBarsLayer):
/// bars with their glow in one draw, and the halo or wave behind them as one
/// vertex-coloured mesh. it eases between the analyser's 60 hz targets at the
/// display's own rate, every frame while music plays or the loading pulse
/// runs, and after that only until its bars have settled.
class SpectrumItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(SpectrumView)

    Q_PROPERTY(Kind kind READ kind WRITE setKind NOTIFY styleChanged FINAL)
    Q_PROPERTY(bool wave READ isWave WRITE setWave NOTIFY styleChanged FINAL)
    /// an immersive band in one row instead of mirrored
    Q_PROPERTY(bool continuous READ isContinuous WRITE setContinuous NOTIFY styleChanged FINAL)
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(qreal sensitivity READ sensitivity WRITE setSensitivity NOTIFY styleChanged FINAL)
    Q_PROPERTY(bool suspended READ isSuspended WRITE setSuspended NOTIFY suspendedChanged FINAL)
    /// the inner radius of a ring as a fraction of the item, for sizing what it wraps
    Q_PROPERTY(qreal innerRadiusRatio READ innerRadiusRatio CONSTANT FINAL)
    /// a ring round a squiggly cover is cut along the cover's squiggle line,
    /// r = cutRadius + cutAmplitude * sin(cutWaves * angle - cutPhase) from
    /// the item's centre, clockwise from the top (ArtworkImage's sine). no
    /// waves, no cut.
    Q_PROPERTY(qreal cutRadius READ cutRadius WRITE setCutRadius NOTIFY cutChanged FINAL)
    Q_PROPERTY(qreal cutAmplitude READ cutAmplitude WRITE setCutAmplitude NOTIFY cutChanged FINAL)
    Q_PROPERTY(qreal cutWaves READ cutWaves WRITE setCutWaves NOTIFY cutChanged FINAL)
    Q_PROPERTY(qreal cutPhase READ cutPhase WRITE setCutPhase NOTIFY cutChanged FINAL)

public:
    enum class Kind { Compact, Immersive, Ring, Equalizer };
    Q_ENUM(Kind)

    explicit SpectrumItem(QQuickItem *parent = nullptr);
    ~SpectrumItem() override;

    Kind kind() const { return m_kind; }
    void setKind(Kind kind);
    bool isWave() const { return m_wave; }
    void setWave(bool wave);
    bool isContinuous() const { return m_continuous; }
    void setContinuous(bool continuous);
    bool isActive() const { return m_active; }
    void setActive(bool active);
    qreal sensitivity() const { return m_sensitivity; }
    void setSensitivity(qreal sensitivity);
    bool isSuspended() const { return m_suspended; }
    void setSuspended(bool suspended);
    qreal innerRadiusRatio() const { return 0.66; }
    qreal cutRadius() const { return m_cutRadius; }
    void setCutRadius(qreal radius) { setCut(m_cutRadius, radius); }
    qreal cutAmplitude() const { return m_cutAmplitude; }
    void setCutAmplitude(qreal amplitude) { setCut(m_cutAmplitude, amplitude); }
    qreal cutWaves() const { return m_cutWaves; }
    void setCutWaves(qreal waves) { setCut(m_cutWaves, waves); }
    qreal cutPhase() const { return m_cutPhase; }
    void setCutPhase(qreal phase) { setCut(m_cutPhase, phase); }

    /// the styling as configured, for tests
    const SpectrumStyle &style() const { return m_style; }
    const QList<float> &levels() const { return m_levels; }
    const QList<float> &targetLevels() const { return m_targets; }
    bool isMoving() const;
    bool wantsFrames() const;
    /// one frame's worth of easing, as the display link would run it
    void advance(double seconds);

signals:
    void styleChanged();
    void activeChanged();
    void suspendedChanged();
    void cutChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    struct BarVertex {
        float x, y;
        float localX, localY;
        float r, g, b, a;
        float width, length, tipRadius, footRadius;
    };
    struct HaloVertex {
        float x, y;
        quint8 r, g, b, a;
    };

    void setCut(qreal &field, qreal value);
    void rebuildStyle();
    void applyPalette();
    void retarget();
    void updateOpacity(bool animated);
    void ensureFrames();
    void stopFrames();
    void onFrame();
    void buildGeometry();
    void buildLinear();
    void buildRadial();
    void appendBar(float footX, float footY, float directionX, float directionY, float width, float length,
                   float tipRadius, float footRadius, const spectrum::Rgba &color, float margin);
    void appendRowHalo(float barWidth, float spacing);
    void appendRingHalo(const QList<float> &radii, float centerX, float centerY, float edge);
    void prepareRing(qsizetype count);
    void appendFeatheredStrip(const QList<QPointF> &curve, const QList<QPointF> &inward, const QList<spectrum::Rgba> &colors,
                              float sigma, float opacity);
    float rangeWeight(int index, int count) const;
    float linearBarHeight(int index, float barWidth) const;
    bool wantsHalo() const;

    Kind m_kind = Kind::Compact;
    bool m_wave = false;
    bool m_continuous = false;
    bool m_active = false;
    qreal m_sensitivity = 1;
    bool m_suspended = false;
    SpectrumStyle m_style;
    qreal m_cutRadius = 0;
    qreal m_cutAmplitude = 0;
    qreal m_cutWaves = 0;
    qreal m_cutPhase = 0;

    QList<float> m_levels;
    QList<float> m_targets;
    QList<float> m_fallVelocities;
    QList<spectrum::Rgba> m_barColors;
    QList<spectrum::Rgba> m_haloColors;
    bool m_loading = false;

    QList<BarVertex> m_barVertices;
    QList<quint16> m_barIndices;
    QList<HaloVertex> m_haloVertices;
    QList<quint16> m_haloIndices;
    /// what the indices were built for (shape, halo, bars, how many levels),
    /// and whether this frame keeps them rather than building them again
    quint64 m_indexLayout = 0;
    bool m_keepIndices = false;
    bool m_geometryDirty = true;

    /// the ring halo's fixed part, worked out again only when the number of
    /// bars or the colours change: each point round the ring's direction and
    /// its colours, solid and at each step of the feathered edge. a frame
    /// then only moves the points in and out. worked out every frame it was
    /// a fifth of the render thread's time in the immersive player.
    struct RingPoint {
        float cos;
        float sin;
        std::array<std::array<quint8, 4>, 6> colors;
    };
    QList<RingPoint> m_ringPoints;
    qsizetype m_ringBars = -1;
    bool m_ringColorsDirty = true;
    QList<float> m_ringRadii;
    QList<float> m_ringScratch;
    QList<float> m_ringCurve;
    QList<float> m_barRadii;

    QElapsedTimer m_frameClock;
    bool m_frameClockRunning = false;
    QMetaObject::Connection m_frameConnection;
    qreal m_contentOpacity = 1;
    QVariantAnimation m_opacityAnimation;
    bool m_configured = false;
};

} // namespace swiftify
