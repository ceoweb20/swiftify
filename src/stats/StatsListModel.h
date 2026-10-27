#pragma once

#include "stats/StatsTypes.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace swiftify {

/// a top list or the recent plays for the stats page: each row what it is,
/// where it placed, and how much of it was heard
class StatsListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Stats lists come from the stats.")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Role {
        RankRole = Qt::UserRole + 1,
        KeyRole,
        NameRole,
        SubtitleRole,
        ArtworkUrlRole,
        PlaysRole,
        ListenedMsRole,
        PlayedAtRole,
        /// its plays (or time) over the first place's, for a bar beside it
        ShareRole,
        SongRole,
        ArtistRole,
        AlbumRole,
    };
    Q_ENUM(Role)

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_entries.size()); }
    const QList<StatEntry> &entries() const { return m_entries; }
    /// `byTime` ranks and shares by time heard rather than by plays
    void setEntries(const QList<StatEntry> &entries, bool byTime);

    Q_INVOKABLE swiftify::Song songAt(int row) const;
    Q_INVOKABLE swiftify::Artist artistAt(int row) const;
    Q_INVOKABLE swiftify::Album albumAt(int row) const;
    Q_INVOKABLE QString nameAt(int row) const;

signals:
    void countChanged();

private:
    QList<StatEntry> m_entries;
    double m_top = 0;
    bool m_byTime = false;
};

} // namespace swiftify
