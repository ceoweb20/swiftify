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

#include "app/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace swiftify::paths {

namespace {

QString ensured(const QString &path)
{
    QDir().mkpath(path);
    return path;
}

} // namespace

QString dataDirectory()
{
    return ensured(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                   + QStringLiteral("/Swiftify"));
}

QString cacheDirectory()
{
    return ensured(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                   + QStringLiteral("/Swiftify"));
}

QString logDirectory()
{
    return ensured(dataDirectory() + QStringLiteral("/logs"));
}

QString logFilePath()
{
    return logDirectory() + QStringLiteral("/swiftify.log");
}

} // namespace swiftify::paths
