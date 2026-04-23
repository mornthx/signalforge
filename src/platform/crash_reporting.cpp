// src/platform/crash_reporting.cpp
#include "platform/crash_reporting.hpp"

#include "observability/logging.hpp"

#include <atomic>
#include <filesystem>
#include <sentry.h>
#include <string>

namespace signalforge::platform {

namespace {

// The crashpad backend's client library enforces exactly one sentry_init per
// process lifetime; a second init after sentry_close triggers an internal
// Check failure. We therefore track two flags: `g_active` reflects the
// observable active state, while `g_ever_initialized` records whether we
// have already called sentry_init and so must not call it again.
std::atomic<bool> g_active{false};
std::atomic<bool> g_ever_initialized{false};

std::string toUtf8(const QString& s) {
    const QByteArray bytes = s.toUtf8();
    return std::string{bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

}  // namespace

bool initCrashReporting(const CrashReporterConfig& config) {
    // Validation first. Fail fast on malformed config regardless of prior
    // process state so these checks remain order-independent across callers
    // (and across Catch2 test cases).
    if (config.crashDumpDirectory.isEmpty()) {
        SF_LOG_ERROR("crash_reporting: crashDumpDirectory is empty; crash reporting disabled");
        return false;
    }
    if (config.backendHandlerPath.isEmpty()) {
        SF_LOG_ERROR("crash_reporting: backendHandlerPath is empty; crash reporting disabled");
        return false;
    }

    const std::filesystem::path dir{toUtf8(config.crashDumpDirectory)};
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        SF_LOG_ERROR("crash_reporting: failed to create crashDumpDirectory '{}': {}", dir.string(), ec.message());
        return false;
    }

    if (g_active.load(std::memory_order_acquire)) {
        SF_LOG_WARN("crash_reporting: already active; ignoring redundant init and returning current state");
        return true;
    }

    if (g_ever_initialized.load(std::memory_order_acquire)) {
        // Crashpad does not permit re-registration of its signal handler.
        // Once shutdownCrashReporting has been called, the backend cannot be
        // brought back up in this process; callers must restart the process.
        SF_LOG_WARN("crash_reporting: already initialized and shut down in this process; "
                    "crashpad backend does not support re-initialization, returning false");
        return false;
    }

    sentry_options_t* options = sentry_options_new();
    if (options == nullptr) {
        SF_LOG_ERROR("crash_reporting: sentry_options_new returned null");
        return false;
    }

    sentry_options_set_database_path(options, toUtf8(config.crashDumpDirectory).c_str());
    sentry_options_set_handler_path(options, toUtf8(config.backendHandlerPath).c_str());

    if (!config.applicationVersion.isEmpty()) {
        const std::string release = toUtf8(config.applicationName) + "@" + toUtf8(config.applicationVersion);
        sentry_options_set_release(options, release.c_str());
    }

    // DSN intentionally unset — architecture §14.3 forbids an upload backend in
    // V1. Without a DSN, sentry-native runs in local-only mode and never emits
    // network traffic.

    const int rc = sentry_init(options);
    if (rc != 0) {
        SF_LOG_ERROR("crash_reporting: sentry_init failed with rc={}", rc);
        return false;
    }

    g_ever_initialized.store(true, std::memory_order_release);
    g_active.store(true, std::memory_order_release);
    SF_LOG_INFO("crash_reporting: initialized (database='{}')", dir.string());
    return true;
}

void shutdownCrashReporting() {
    if (!g_active.load(std::memory_order_acquire)) {
        return;
    }
    sentry_close();
    g_active.store(false, std::memory_order_release);
    SF_LOG_INFO("crash_reporting: shutdown complete");
}

bool crashReportingActive() noexcept {
    return g_active.load(std::memory_order_acquire);
}

}  // namespace signalforge::platform
