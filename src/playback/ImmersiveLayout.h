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

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <algorithm>

namespace swiftify {

/// sizes the immersive player from its window (ImmersivePlayerLayout.swift).
/// the spectrum's room is set aside before the artwork is sized, so a short
/// window never pushes the bars off the bottom.
class ImmersiveLayout : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(qreal windowWidth MEMBER m_windowWidth NOTIFY changed FINAL)
    Q_PROPERTY(qreal windowHeight MEMBER m_windowHeight NOTIFY changed FINAL)
    Q_PROPERTY(bool showsSpectrum MEMBER m_showsSpectrum NOTIFY changed FINAL)
    Q_PROPERTY(bool showsPanel MEMBER m_showsPanel NOTIFY changed FINAL)
    Q_PROPERTY(bool circular MEMBER m_circular NOTIFY changed FINAL)

    Q_PROPERTY(qreal topBarHeight READ topBarHeight CONSTANT FINAL)
    Q_PROPERTY(qreal bottomPadding READ bottomPadding CONSTANT FINAL)
    Q_PROPERTY(qreal spectrumHeight READ spectrumHeight NOTIFY changed FINAL)
    Q_PROPERTY(qreal spectrumSpacing READ spectrumSpacing NOTIFY changed FINAL)
    Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY changed FINAL)
    Q_PROPERTY(qreal artworkSize READ artworkSize NOTIFY changed FINAL)
    Q_PROPERTY(bool compactSpacing READ usesCompactSpacing NOTIFY changed FINAL)
    Q_PROPERTY(qreal contentSpacing READ contentSpacing NOTIFY changed FINAL)
    Q_PROPERTY(qreal titleHeight READ titleHeight NOTIFY changed FINAL)
    Q_PROPERTY(qreal titleFontSize READ titleFontSize NOTIFY changed FINAL)

public:
    explicit ImmersiveLayout(QObject *parent = nullptr);

    static constexpr qreal kTopBarHeight = 70;
    static constexpr qreal kBottomPadding = 18;

    qreal topBarHeight() const { return kTopBarHeight; }
    qreal bottomPadding() const { return kBottomPadding; }
    qreal spectrumHeight() const { return m_showsSpectrum ? std::min(150.0, std::max(72.0, m_windowHeight * 0.17)) : 0; }
    qreal spectrumSpacing() const { return m_showsSpectrum ? 16 : 0; }
    qreal contentHeight() const
    {
        return std::max(0.0, m_windowHeight - kTopBarHeight - kBottomPadding - spectrumHeight() - spectrumSpacing());
    }
    bool usesCompactSpacing() const { return contentHeight() < 460; }
    qreal contentSpacing() const { return usesCompactSpacing() ? 12 : 20; }
    qreal titleHeight() const { return usesCompactSpacing() ? 64 : 72; }
    qreal titleFontSize() const { return usesCompactSpacing() ? 26 : 30; }
    /// what goes under the artwork: the song, its controls and their gaps
    qreal chromeHeight() const;
    qreal artworkSize() const;

    void set(qreal width, qreal height, bool showsSpectrum, bool showsPanel, bool circular);

signals:
    void changed();

private:
    qreal m_windowWidth = 0;
    qreal m_windowHeight = 0;
    bool m_showsSpectrum = true;
    bool m_showsPanel = false;
    bool m_circular = false;
};

} // namespace swiftify
