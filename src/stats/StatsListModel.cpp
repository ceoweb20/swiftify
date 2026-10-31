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

#include "stats/StatsListModel.h"

#include <algorithm>

namespace swiftify {

int StatsListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant StatsListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return {};
    }
    const StatEntry &entry = m_entries.at(index.row());
    switch (role) {
    case RankRole: return index.row() + 1;
    case KeyRole: return entry.key;
    case NameRole: return entry.name;
    case SubtitleRole: return entry.subtitle;
    case ArtworkUrlRole: return entry.artworkUrl;
    case PlaysRole: return entry.plays;
    case ListenedMsRole: return double(entry.listenedMs);
    case PlayedAtRole: return entry.playedAt;
    case ShareRole: {
        const double value = m_byTime ? double(entry.listenedMs) : double(entry.plays);
        return m_top > 0 ? std::clamp(value / m_top, 0.0, 1.0) : 0.0;
    }
    case SongRole: return QVariant::fromValue(entry.song);
    case ArtistRole: return QVariant::fromValue(entry.artist);
    case AlbumRole: return QVariant::fromValue(entry.album);
    default: return {};
    }
}

QHash<int, QByteArray> StatsListModel::roleNames() const
{
    return {{RankRole, "rank"},         {KeyRole, "key"},         {NameRole, "name"},
            {SubtitleRole, "subtitle"}, {ArtworkUrlRole, "artworkUrl"}, {PlaysRole, "plays"},
            {ListenedMsRole, "listenedMs"}, {PlayedAtRole, "playedAt"}, {ShareRole, "share"},
            {SongRole, "song"},         {ArtistRole, "artist"},   {AlbumRole, "album"}};
}

void StatsListModel::setEntries(const QList<StatEntry> &entries, bool byTime)
{
    QList<StatEntry> sorted = entries;
    // ranked by what is asked, whatever order the source gave: stats.fm
    // orders its lists by time heard. a place without a count keeps its
    // order, after those with one
    if (byTime) {
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const StatEntry &a, const StatEntry &b) { return a.listenedMs > b.listenedMs; });
    } else {
        std::stable_sort(sorted.begin(), sorted.end(), [](const StatEntry &a, const StatEntry &b) {
            return a.plays >= 0 && b.plays >= 0 ? a.plays > b.plays : a.plays >= 0 && b.plays < 0;
        });
    }
    beginResetModel();
    m_entries = sorted;
    m_byTime = byTime;
    m_top = 0;
    for (const StatEntry &entry : std::as_const(m_entries)) {
        m_top = std::max(m_top, byTime ? double(entry.listenedMs) : double(entry.plays));
    }
    endResetModel();
    emit countChanged();
}

Song StatsListModel::songAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).song : Song {};
}

Artist StatsListModel::artistAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).artist : Artist {};
}

Album StatsListModel::albumAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).album : Album {};
}

QString StatsListModel::nameAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).name : QString();
}

} // namespace swiftify
