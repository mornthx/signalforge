// src/platform/crash_reporting.hpp
#pragma once

#include <QString>

namespace signalforge::platform {

/// Configuration for crash reporting initialization. Fields without defaults
/// must be populated by the caller before passing to initCrashReporting().
struct CrashReporterConfig {
    QString applicationName = "SignalForge";  ///< Product name embedded in crash metadata
    QString applicationVersion;               ///< From CMake-generated SIGNALFORGE_VERSION
    QString crashDumpDirectory;               ///< Must exist and be writable
    QString backendHandlerPath;  ///< Path to the crash handler binary; implementation-defined (e.g., sentry-native's
                                 ///< crashpad_handler or equivalent).
};

/// Initialize the crash reporting backend. Idempotent while active — repeat
/// calls log a warning and return the current active state without
/// reinitializing.
///
/// Single-init-per-process constraint: the selected backend (sentry-native
/// with the crashpad client) registers a process-global signal handler that
/// cannot be safely re-registered. Once shutdownCrashReporting() has been
/// called, a subsequent initCrashReporting() returns false and leaves the
/// process without crash reporting for the rest of its lifetime. Callers
/// must restart the process to re-enable crash reporting.
///
/// On validation or backend failure (dump directory empty or uncreatable,
/// backend handler path empty, sentry_init error), logs an error and returns
/// false; the application continues without crash reporting.
///
/// Thread safety: call from the main thread before any worker threads start.
/// The backend client is process-global.
[[nodiscard]] bool initCrashReporting(const CrashReporterConfig& config);

/// Explicit shutdown. Releases the in-process client registration and marks
/// crashReportingActive() false. See initCrashReporting() for the
/// single-init-per-process constraint that prevents subsequent reactivation.
///
/// Thread safety: must be called from the same thread as initCrashReporting().
/// Idempotent: safe to call when already inactive.
void shutdownCrashReporting();

/// Whether crash reporting is currently active in this process.
///
/// Thread safety: safe from any thread. Noexcept.
[[nodiscard]] bool crashReportingActive() noexcept;

}  // namespace signalforge::platform
