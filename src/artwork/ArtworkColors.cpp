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

#include "artwork/ArtworkColors.h"

#include "artwork/ArtworkCache.h"
#include "artwork/ArtworkPalette.h"

#include <QCache>
#include <QPointer>
#include <QtConcurrent>

namespace swiftify {

namespace {

QCache<QUrl, QList<QColor>> &sharedPalettes()
{
    static QCache<QUrl, QList<QColor>> cache(256);
    return cache;
}

} // namespace

void ArtworkColors::setSource(const QUrl &source)
{
    if (m_source == source) {
        return;
    }
    m_source = source;
    emit sourceChanged();

    if (source.isEmpty() || !ArtworkCache::instance()) {
        return;
    }
    if (const QList<QColor> *cached = sharedPalettes().object(source)) {
        m_palette = *cached;
        emit colorsChanged();
        return;
    }
    // the old colours stay until the new ones are ready
    QPointer<ArtworkColors> self(this);
    ArtworkCache::instance()->bytes(source, [self, source](const QByteArray &data) {
        if (!self || data.isEmpty() || self->m_source != source) {
            return;
        }
        QtConcurrent::run([data] { return palette::extract(data); })
            .then(self.data(), [self, source](const std::optional<WeightedPalette> &palette) {
                if (!self || !palette) {
                    return;
                }
                sharedPalettes().insert(source, new QList<QColor>(palette->colors));
                if (self->m_source == source) {
                    self->m_palette = palette->colors;
                    emit self->colorsChanged();
                }
            });
    });
}

} // namespace swiftify
