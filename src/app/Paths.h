#pragma once

#include <QString>

namespace swiftify::paths {

/// $XDG_DATA_HOME/Swiftify, created on first use: what the app keeps of the
/// person's.
QString dataDirectory();
/// $XDG_CACHE_HOME/Swiftify, for artwork and other things safe to delete.
QString cacheDirectory();

} // namespace swiftify::paths
