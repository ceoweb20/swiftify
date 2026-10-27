#pragma once

#include <QJsonObject>
#include <QStringList>
#include <optional>

namespace swiftify::playlistlinks {
bool validId(const QString &id);
std::optional<QString> id(const QString &reference);
QStringList recentIds(const QJsonObject &history);
} // namespace swiftify::playlistlinks
