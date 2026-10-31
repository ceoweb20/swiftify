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

#include "playback/ImmersiveLayout.h"

#include <algorithm>

namespace swiftify {

ImmersiveLayout::ImmersiveLayout(QObject *parent)
    : QObject(parent)
{
}

void ImmersiveLayout::set(qreal width, qreal height, bool showsSpectrum, bool showsPanel, bool circular)
{
    m_windowWidth = width;
    m_windowHeight = height;
    m_showsSpectrum = showsSpectrum;
    m_showsPanel = showsPanel;
    m_circular = circular;
    emit changed();
}

qreal ImmersiveLayout::artworkSize() const
{
    const qreal metadataAndControls = chromeHeight();
    qreal widthShare;
    qreal heightShare;
    if (m_showsPanel && m_circular) {
        widthShare = 0.34;
        heightShare = 0.42;
    } else if (m_showsPanel) {
        widthShare = 0.27;
        heightShare = 0.32;
    } else if (m_circular) {
        widthShare = 0.48;
        heightShare = 0.5;
    } else {
        widthShare = 0.38;
        heightShare = 0.38;
    }
    return std::max(0.0, std::min({m_circular ? 500.0 : 380.0, m_windowWidth * widthShare, m_windowHeight * heightShare,
                                   contentHeight() - metadataAndControls}));
}

qreal ImmersiveLayout::chromeHeight() const
{
    // two title lines, artist and album, progress, transport, the volume,
    // and their gaps
    return usesCompactSpacing() ? 274 : 318;
}

} // namespace swiftify
