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

#include "playback/Lyrics.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace swiftify {

std::optional<Lyrics> Lyrics::fromJson(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonObject object = QJsonDocument::fromJson(json, &error).object();
    if (error.error != QJsonParseError::NoError || !object.contains(QStringLiteral("lines"))) {
        return std::nullopt;
    }
    Lyrics lyrics;
    lyrics.language = object.value(QStringLiteral("language")).toString();
    lyrics.provider = object.value(QStringLiteral("provider")).toString();
    lyrics.synchronized = object.value(QStringLiteral("syncType")).toString() == QLatin1StringView("lineSynced");
    for (const QJsonValue &value : object.value(QStringLiteral("lines")).toArray()) {
        const QJsonObject line = value.toObject();
        lyrics.lines.append({
            quint32(line.value(QStringLiteral("startTimeMs")).toDouble()),
            quint32(line.value(QStringLiteral("endTimeMs")).toDouble()),
            line.value(QStringLiteral("words")).toString(),
        });
    }
    // spotify gives synced lines no end, so each lasts until the next one
    // starts and the last for a few seconds
    for (qsizetype index = 0; index < lyrics.lines.size(); ++index) {
        LyricLine &line = lyrics.lines[index];
        if (line.endTimeMs <= line.startTimeMs) {
            line.endTimeMs = index + 1 < lyrics.lines.size() ? lyrics.lines.at(index + 1).startTimeMs
                                                             : line.startTimeMs + 5000;
        }
    }
    return lyrics;
}

int Lyrics::lineIndex(quint32 positionMs) const
{
    if (!synchronized || lines.isEmpty()) {
        return -1;
    }
    qsizetype lower = 0;
    qsizetype upper = lines.size();
    while (lower < upper) {
        const qsizetype middle = lower + (upper - lower) / 2;
        if (lines.at(middle).startTimeMs <= positionMs) {
            lower = middle + 1;
        } else {
            upper = middle;
        }
    }
    return int(lower) - 1;
}

LyricsModel::LyricsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LyricsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_lines.size());
}

QVariant LyricsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size()) {
        return {};
    }
    const LyricLine &line = m_lines.at(index.row());
    switch (role) {
    case WordsRole: return line.words;
    case StartTimeMsRole: return line.startTimeMs;
    case EndTimeMsRole: return line.endTimeMs;
    default: return {};
    }
}

QHash<int, QByteArray> LyricsModel::roleNames() const
{
    return {{WordsRole, "words"}, {StartTimeMsRole, "startTimeMs"}, {EndTimeMsRole, "endTimeMs"}};
}

void LyricsModel::setLines(const QList<LyricLine> &lines)
{
    beginResetModel();
    m_lines = lines;
    endResetModel();
    emit countChanged();
}

} // namespace swiftify
