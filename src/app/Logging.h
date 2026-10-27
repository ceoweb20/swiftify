#pragma once

#include <QString>

namespace swiftify::logging {

/// sets the required logging environment variables for the respective OS:
/// - QT_FORCE_STDERR_LOGGING = 1 (forces Qt logs to stderr on all platforms)
/// - QT_LOGGING_RULES = swiftify.*=true (enables all swiftify categories)
/// - RUST_LOG = info,spotify_core=debug,librespot=info (rust core and librespot)
/// - QT_MESSAGE_PATTERN (timestamp, level, category, and message)
/// - QT_ASSUME_STDERR_HAS_CONSOLE = 1 (on Windows)
void applyEnvironmentVariables();

/// checks if logging is requested (via settings or SWIFTIFY_LOG), sets the OS
/// variables, rotates old logs, redirects stderr/stdout to swiftify.log in the
/// log directory, and installs the formatted message handler.
void setupLogging();

/// whether logging is active for the current process session
bool isActive();

} // namespace swiftify::logging
