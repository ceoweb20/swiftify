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

#include "spotify/SpotifyTypes.h"

#include <QList>
#include <QString>

#include <optional>

namespace swiftify {

/// the songs of a playlist (or the saved songs) as they were last loaded, kept
/// on disk so a list opened after a restart shows at once rather than after
/// the dozens of pages spotify sends it in. what is shown from here is
/// refreshed from spotify underneath, so a stale list only lasts a moment.
/// it is a cache: anything unreadable is treated as missing.
class SongListCache {
public:
    /// keeps its files under `directory`, made when first written to
    explicit SongListCache(QString directory);

    std::optional<QList<Song>> load(const QString &key) const;
    void save(const QString &key, const QList<Song> &songs) const;
    void remove(const QString &key) const;
    /// everything, as when the person signs out
    void clear() const;

private:
    QString pathFor(const QString &key) const;

    QString m_directory;
};

} // namespace swiftify
