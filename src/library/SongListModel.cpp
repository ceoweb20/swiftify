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

#include "library/SongListModel.h"

#include <QHash>

#include <functional>

namespace swiftify {

SongListModel::SongListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SongListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_songs.size());
}

QVariant SongListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_songs.size()) {
        return {};
    }
    const Song &song = m_songs.at(index.row());
    switch (role) {
    case SongRole: return QVariant::fromValue(song);
    case InstanceIdRole: return song.instanceId;
    case NameRole: return song.name;
    case ArtistsRole: return song.artists;
    case AlbumNameRole: return song.albumName;
    case ArtworkUrlRole: return song.artworkUrl;
    case UriRole: return song.uri;
    case DurationMsRole: return song.durationMs;
    case ExplicitRole: return song.isExplicit;
    case AlbumIdRole: return song.albumId;
    default: return {};
    }
}

QHash<int, QByteArray> SongListModel::roleNames() const
{
    return {
        {SongRole, "song"},
        {InstanceIdRole, "instanceId"},
        {NameRole, "name"},
        {ArtistsRole, "artists"},
        {AlbumNameRole, "albumName"},
        {ArtworkUrlRole, "artworkUrl"},
        {UriRole, "uri"},
        {DurationMsRole, "durationMs"},
        {ExplicitRole, "isExplicit"},
        {AlbumIdRole, "albumId"},
    };
}

qint64 SongListModel::totalDurationMs() const
{
    qint64 total = 0;
    for (const Song &song : m_songs) {
        total += song.durationMs;
    }
    return total;
}

void SongListModel::setSongs(const QList<Song> &songs)
{
    if (!applyAsEdits(songs)) {
        beginResetModel();
        m_songs = songs;
        endResetModel();
    }
    // the total duration changes with any new list, even one of the same length.
    emit countChanged();
}

bool SongListModel::applyAsEdits(const QList<Song> &songs)
{
    // past this many single-row changes a reset is cheaper for the view
    constexpr qsizetype kMaximumEdits = 64;

    QHash<quint64, qsizetype> next;
    next.reserve(songs.size());
    for (qsizetype row = 0; row < songs.size(); ++row) {
        const quint64 id = songs.at(row).instanceId;
        if (id == 0 || next.contains(id)) {
            return false;
        }
        next.insert(id, row);
    }

    // the rows both lists share have to keep their order
    qsizetype last = -1;
    qsizetype shared = 0;
    for (const Song &song : std::as_const(m_songs)) {
        const auto found = next.constFind(song.instanceId);
        if (found == next.cend()) {
            continue;
        }
        if (*found <= last) {
            return false;
        }
        last = *found;
        ++shared;
    }
    const qsizetype edits = (m_songs.size() - shared) + (songs.size() - shared);
    if (shared == 0 || edits > kMaximumEdits) {
        return false;
    }

    // removals from the bottom up, so the rows above keep their places
    for (qsizetype row = m_songs.size() - 1; row >= 0; --row) {
        if (!next.contains(m_songs.at(row).instanceId)) {
            beginRemoveRows({}, int(row), int(row));
            m_songs.removeAt(row);
            endRemoveRows();
        }
    }
    // what is left runs in the new list's order, so the gaps fill top down
    for (qsizetype row = 0; row < songs.size(); ++row) {
        if (row < m_songs.size() && m_songs.at(row).instanceId == songs.at(row).instanceId) {
            m_songs[row] = songs.at(row);
            continue;
        }
        beginInsertRows({}, int(row), int(row));
        m_songs.insert(row, songs.at(row));
        endInsertRows();
    }
    if (!m_songs.isEmpty()) {
        // a kept row may carry newer details, such as a fresh cover
        emit dataChanged(index(0), index(int(m_songs.size()) - 1));
    }
    return true;
}

void SongListModel::moveRows(int from, int count, int to)
{
    if (count <= 0 || from < 0 || from + count > m_songs.size() || to < 0 || to > m_songs.size()
        || (to >= from && to <= from + count)) {
        return;
    }
    beginMoveRows({}, from, from + count - 1, {}, to);
    QList<Song> moving = m_songs.mid(from, count);
    m_songs.remove(from, count);
    const int insertAt = to > from ? to - count : to;
    for (int offset = 0; offset < count; ++offset) {
        m_songs.insert(insertAt + offset, moving.at(offset));
    }
    endMoveRows();
}

void SongListModel::removeAll(const std::function<bool(const Song &)> &predicate)
{
    for (int row = int(m_songs.size()) - 1; row >= 0; --row) {
        if (predicate(m_songs.at(row))) {
            beginRemoveRows({}, row, row);
            m_songs.removeAt(row);
            endRemoveRows();
        }
    }
    emit countChanged();
}

void SongListModel::append(const Song &song)
{
    beginInsertRows({}, int(m_songs.size()), int(m_songs.size()));
    m_songs.append(song);
    endInsertRows();
    emit countChanged();
}

Song SongListModel::at(int row) const
{
    return row >= 0 && row < m_songs.size() ? m_songs.at(row) : Song {};
}

int SongListModel::indexOfInstance(quint64 instanceId) const
{
    for (qsizetype row = 0; row < m_songs.size(); ++row) {
        if (m_songs.at(row).instanceId == instanceId) {
            return int(row);
        }
    }
    return -1;
}

} // namespace swiftify
