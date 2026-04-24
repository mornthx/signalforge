// src/platform/app_paths.hpp
#pragma once

#include <QString>

namespace signalforge::platform {

/// Log directory. Resolution order:
///   1. `$SIGNALFORGE_LOG_DIR` if set and non-empty
///   2. `$XDG_STATE_HOME/signalforge/logs` if `XDG_STATE_HOME` is set
///   3. `~/.local/state/signalforge/logs`
///
/// The directory is created if absent (including any missing parent dirs).
/// If creation fails, an empty `QString` is returned and a warning is logged.
///
/// Thread safety: safe from any thread, but returns the same path regardless
/// of which thread calls it.
[[nodiscard]] QString logDirectory();

/// Crashdump directory. Resolution:
///   1. `$XDG_STATE_HOME/signalforge/crashdumps` if `XDG_STATE_HOME` is set
///   2. `~/.local/state/signalforge/crashdumps`
///
/// The directory is created if absent. Returns empty `QString` on creation
/// failure.
///
/// Thread safety: safe from any thread.
[[nodiscard]] QString crashDumpDirectory();

/// Config directory. Resolution:
///   1. `$XDG_CONFIG_HOME/signalforge` if `XDG_CONFIG_HOME` is set
///   2. `~/.config/signalforge`
///
/// The directory is created if absent. Returns empty `QString` on creation
/// failure.
///
/// Thread safety: safe from any thread.
[[nodiscard]] QString configDirectory();

}  // namespace signalforge::platform
