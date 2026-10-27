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
