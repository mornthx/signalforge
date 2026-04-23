// src/platform/crash_reporting.hpp
#pragma once

#include <QString>

namespace signalforge::platform {

/// Configuration for Crashpad initialization. Fields without defaults must
/// be populated by the caller before passing to initCrashReporting().
struct CrashReporterConfig {
    QString applicationName = "SignalForge";  ///< Product name embedded in crash metadata
    QString applicationVersion;               ///< From CMake-generated SIGNALFORGE_VERSION
    QString crashDumpDirectory;               ///< Must exist and be writable
    QString handlerExecutable;                ///< Path to crashpad_handler binary
};

/// Initialize Crashpad. Idempotent — subsequent calls log a warning and
/// return the current active state without reinitializing.
///
/// On failure (handler missing, dump directory inaccessible, Crashpad
/// internal error), logs an error and returns false; the application
/// continues without crash reporting.
///
/// Thread safety: call from the main thread before any worker threads
/// start. The Crashpad client is process-global.
[[nodiscard]] bool initCrashReporting(const CrashReporterConfig& config);

/// Explicit shutdown. The Crashpad handler process continues running until
/// the parent exits naturally; this releases only the in-process client
/// registration so subsequent initCrashReporting() is permitted.
///
/// Thread safety: must be called from the same thread as
/// initCrashReporting(). Idempotent.
void shutdownCrashReporting();

/// Whether crash reporting is currently active in this process.
///
/// Thread safety: safe from any thread. Noexcept.
[[nodiscard]] bool crashReportingActive() noexcept;

}  // namespace signalforge::platform
