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

#include <QAbstractListModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace swiftify {

struct LyricLine {
    quint32 startTimeMs = 0;
    quint32 endTimeMs = 0;
    QString words;
};

/// spotify's color-lyrics, as the core returns them (SpotifyLyrics.swift)
struct Lyrics {
    QString language;
    QString provider;
    bool synchronized = false;
    QList<LyricLine> lines;

    static std::optional<Lyrics> fromJson(const QByteArray &json);
    /// the line playing at `positionMs`, by binary search. -1 before the first
    /// line, and always for unsynchronised lyrics.
    int lineIndex(quint32 positionMs) const;
};

class LyricsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Lyrics come from the player.")

    Q_PROPERTY(int count READ count NOTIFY countChanged FINAL)

public:
    enum Role {
        WordsRole = Qt::UserRole + 1,
        StartTimeMsRole,
        EndTimeMsRole,
    };

    explicit LyricsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_lines.size()); }
    void setLines(const QList<LyricLine> &lines);
    quint32 startTimeMs(int row) const { return row >= 0 && row < m_lines.size() ? m_lines.at(row).startTimeMs : 0; }

signals:
    void countChanged();

private:
    QList<LyricLine> m_lines;
};

} // namespace swiftify
