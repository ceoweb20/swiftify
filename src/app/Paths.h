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

#include <QString>

namespace swiftify::paths {

/// $XDG_DATA_HOME/Swiftify, created on first use: what the app keeps of the
/// person's.
QString dataDirectory();
/// $XDG_CACHE_HOME/Swiftify, for artwork and other things safe to delete.
QString cacheDirectory();
/// $XDG_DATA_HOME/Swiftify/logs, for diagnostic output.
QString logDirectory();
/// $XDG_DATA_HOME/Swiftify/logs/swiftify.log
QString logFilePath();

} // namespace swiftify::paths
