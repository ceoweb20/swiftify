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

/// a squiggly divider: the seek line's wave, drawn the whole way across its
/// width, through its middle.
///
/// as a ShaderEffect each divider was a draw of its own, its length a
/// uniform no other divider shared. here the wave's measures and colour ride
/// in its vertices, so the dividers in a window that do not overlap
/// something else between them draw as one. with no shapes
/// (Settings.noShapes) it is a plain line.
class WavyLineItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(WavyLine)

    /// transparent, and so not drawn, until one is given
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(qreal thickness READ thickness WRITE setThickness NOTIFY thicknessChanged FINAL)
    Q_PROPERTY(qreal amplitude READ amplitude WRITE setAmplitude NOTIFY amplitudeChanged FINAL)
    Q_PROPERTY(qreal wavelength READ wavelength WRITE setWavelength NOTIFY wavelengthChanged FINAL)
    /// radians the wave is moved along by
    Q_PROPERTY(qreal phase READ phase WRITE setPhase NOTIFY phaseChanged FINAL)
    /// radians it moves on from phase for each unit of the music's spin
    /// (UiState.spin), following the clock itself while it shows
    Q_PROPERTY(qreal phaseRate READ phaseRate WRITE setPhaseRate NOTIFY phaseRateChanged FINAL)

public:
    explicit WavyLineItem(QQuickItem *parent = nullptr);

    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    qreal thickness() const { return m_thickness; }
    void setThickness(qreal thickness);
    qreal amplitude() const { return m_amplitude; }
    void setAmplitude(qreal amplitude);
    qreal wavelength() const { return m_wavelength; }
    void setWavelength(qreal wavelength);
    qreal phase() const { return m_phase; }
    void setPhase(qreal phase);
    qreal phaseRate() const { return m_phaseRate; }
    void setPhaseRate(qreal rate);

signals:
    void colorChanged();
    void thicknessChanged();
    void amplitudeChanged();
    void wavelengthChanged();
    void phaseChanged();
    void phaseRateChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;

private:
    /// as tall as the wave and its thickness, with a pixel either side
    void updateImplicitHeight();
    /// follows the spin clock while the wave moves and shows, and only then
    void followSpin();

    QColor m_color = Qt::transparent;
    qreal m_thickness = 1.5;
    qreal m_amplitude = 1.6;
    qreal m_wavelength = 14;
    qreal m_phase = 0;
    qreal m_phaseRate = 0;
    QPointer<AmbientClock> m_followed;
};

} // namespace swiftify
