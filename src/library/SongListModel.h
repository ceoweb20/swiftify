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

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

#include <functional>

namespace swiftify {

/// songs for a list view. holds plain c++ songs, so a playlist of thousands
/// costs a vector and the view only builds the rows it shows.
class SongListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Song lists come from the player and the library.")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)
    Q_PROPERTY(qint64 totalDurationMs READ totalDurationMs NOTIFY countChanged FINAL)

public:
    enum Role {
        SongRole = Qt::UserRole + 1,
        InstanceIdRole,
        NameRole,
        ArtistsRole,
        AlbumNameRole,
        ArtworkUrlRole,
        UriRole,
        DurationMsRole,
        ExplicitRole,
        AlbumIdRole,
    };
    Q_ENUM(Role)

    explicit SongListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_songs.size()); }
    qint64 totalDurationMs() const;
    const QList<Song> &songs() const { return m_songs; }
    /// a list that keeps some of these rows, in the same order, arrives as
    /// removals and insertions, so a view keeps its rows and can animate the
    /// ones that moved (the queue stepping on). anything else is a reset.
    void setSongs(const QList<Song> &songs);
    void clear() { setSongs({}); }
    /// moves rows in place, so a drag in the view animates rather than resets
    void moveRows(int from, int count, int to);
    void removeAll(const std::function<bool(const Song &)> &predicate);
    void append(const Song &song);

    Q_INVOKABLE swiftify::Song at(int row) const;
    Q_INVOKABLE int indexOfInstance(quint64 instanceId) const;

signals:
    void countChanged();

private:
    bool applyAsEdits(const QList<Song> &songs);

    QList<Song> m_songs;
};

} // namespace swiftify
