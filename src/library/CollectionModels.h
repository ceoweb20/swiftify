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

#include <algorithm>

namespace swiftify {

/// the list behind a playlist, album or artist model, so each model below can
/// derive from QAbstractListModel directly, where qml's tooling can see it.
template <typename Item>
class ItemList {
public:
    int size() const { return int(m_items.size()); }
    const QList<Item> &items() const { return m_items; }
    const Item *at(const QModelIndex &index) const
    {
        return index.isValid() && index.row() < m_items.size() ? &m_items.at(index.row()) : nullptr;
    }
    Item value(int row) const { return row >= 0 && row < m_items.size() ? m_items.at(row) : Item {}; }

    template <typename Model>
    void replace(Model *model, const QList<Item> &items)
    {
        // the same things again, as going back to a page reloads them: the
        // rows are only refreshed, so its cards stay built where they were
        const bool same = items.size() == m_items.size()
            && std::equal(items.cbegin(), items.cend(), m_items.cbegin(),
                          [](const Item &next, const Item &current) { return next.id == current.id; });
        if (same) {
            m_items = items;
            if (!m_items.isEmpty()) {
                emit model->dataChanged(model->index(0), model->index(int(m_items.size()) - 1));
            }
            return;
        }
        model->beginResetModel();
        m_items = items;
        model->endResetModel();
        emit model->countChanged();
    }

private:
    QList<Item> m_items;
};

#define SWIFTIFY_ITEM_LIST_MODEL(Model, Item)                                                           \
public:                                                                                               \
    using QAbstractListModel::QAbstractListModel;                                                     \
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_list.size(); } \
    int count() const { return m_list.size(); }                                                       \
    const QList<Item> &items() const { return m_list.items(); }                                       \
    void setItems(const QList<Item> &items) { m_list.replace(this, items); }                          \
                                                                                                      \
private:                                                                                              \
    friend class ItemList<Item>;                                                                      \
    ItemList<Item> m_list;

class PlaylistListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Playlist lists come from the library.")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Role { PlaylistRole = Qt::UserRole + 1, IdRole, NameRole, DescriptionRole, SongCountRole, ArtworkUrlRole, OwnerNameRole };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE swiftify::Playlist at(int row) const { return m_list.value(row); }

signals:
    void countChanged();

    SWIFTIFY_ITEM_LIST_MODEL(PlaylistListModel, Playlist)
};

class AlbumListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Album lists come from the library.")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Role { AlbumRole = Qt::UserRole + 1, IdRole, NameRole, ArtistsRole, ArtworkUrlRole, ReleaseDateRole };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE swiftify::Album at(int row) const { return m_list.value(row); }

signals:
    void countChanged();

    SWIFTIFY_ITEM_LIST_MODEL(AlbumListModel, Album)
};

class ArtistListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Artist lists come from the library.")
    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Role { ArtistRole = Qt::UserRole + 1, IdRole, NameRole, ArtworkUrlRole };

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE swiftify::Artist at(int row) const { return m_list.value(row); }

signals:
    void countChanged();

    SWIFTIFY_ITEM_LIST_MODEL(ArtistListModel, Artist)
};

} // namespace swiftify
