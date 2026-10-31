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

#include "library/SongListCache.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace swiftify {

namespace {

/// raised whenever what a song keeps changes, so an older file is let go of
/// rather than read as something it is not
constexpr int kFormat = 1;

} // namespace

SongListCache::SongListCache(QString directory)
    : m_directory(std::move(directory))
{
}

QString SongListCache::pathFor(const QString &key) const
{
    // keys are spotify ids, but nothing outside the folder should be reachable
    QString safe = key;
    for (QChar &character : safe) {
        if (!character.isLetterOrNumber() && character != u'-' && character != u'_') {
            character = u'_';
        }
    }
    return m_directory + u'/' + safe + QStringLiteral(".json");
}

std::optional<QList<Song>> SongListCache::load(const QString &key) const
{
    QFile file(pathFor(key));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QStringLiteral("format")).toInt() != kFormat) {
        return std::nullopt;
    }
    QList<Song> songs;
    const QJsonArray array = root.value(QStringLiteral("songs")).toArray();
    songs.reserve(array.size());
    for (const QJsonValue &value : array) {
        const Song song = Song::fromJson(value.toObject());
        if (!song.uri.isEmpty()) {
            songs.append(song);
        }
    }
    return songs;
}

void SongListCache::save(const QString &key, const QList<Song> &songs) const
{
    QDir().mkpath(m_directory);
    QJsonArray array;
    for (const Song &song : songs) {
        array.append(song.toJson());
    }
    QSaveFile file(pathFor(key));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(QJsonObject {{QStringLiteral("format"), kFormat}, {QStringLiteral("songs"), array}})
                   .toJson(QJsonDocument::Compact));
    file.commit();
}

void SongListCache::remove(const QString &key) const
{
    QFile::remove(pathFor(key));
}

void SongListCache::clear() const
{
    QDir(m_directory).removeRecursively();
}

} // namespace swiftify
