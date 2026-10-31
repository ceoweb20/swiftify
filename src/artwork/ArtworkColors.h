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
#include <QList>
#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// the palette of any cover, for the glows behind header artwork. palettes are
/// shared through a cache of 256, as ArtworkAmbience.swift keeps them.
class ArtworkColors : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    Q_PROPERTY(bool ready READ isReady NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor primary READ primary NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor secondary READ secondary NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QList<QColor> palette READ palette NOTIFY colorsChanged FINAL)

public:
    using QObject::QObject;

    QUrl source() const { return m_source; }
    void setSource(const QUrl &source);
    bool isReady() const { return !m_palette.isEmpty(); }
    QColor primary() const { return m_palette.value(0, QColor(0xff, 0x3d, 0x8a)); }
    QColor secondary() const { return m_palette.value(1, QColor(0xd6, 0x24, 0x6c)); }
    QList<QColor> palette() const { return m_palette; }

signals:
    void sourceChanged();
    void colorsChanged();

private:
    QUrl m_source;
    QList<QColor> m_palette;
};

} // namespace swiftify
