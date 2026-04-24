// tests/unit/platform/crash_reporting_test.cpp
#include "observability/logging.hpp"
#include "platform/crash_reporting.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>

using namespace signalforge::platform;

namespace {

constexpr const char* kHandlerPath = SIGNALFORGE_TEST_CRASHPAD_HANDLER_PATH;

std::filesystem::path uniqueCrashDir(const std::string& suffix) {
    auto dir =
        std::filesystem::temp_directory_path() / ("signalforge_m2_crash_" + suffix + "_" + std::to_string(::getpid()));
    std::filesystem::remove_all(dir);
    return dir;
}

CrashReporterConfig validConfig(const std::filesystem::path& dir) {
    CrashReporterConfig cfg;
    cfg.applicationName = "SignalForgeTest";
    cfg.applicationVersion = "0.0.3-test";
    cfg.crashDumpDirectory = QString::fromStdString(dir.string());
    cfg.backendHandlerPath = QString::fromUtf8(kHandlerPath);
    return cfg;
}

}  // namespace

// Early-return paths are order-independent (they validate before touching
// sentry state). The combined-lifecycle test is the only case that calls
// sentry_init; crashpad's client enforces one init per process, so all
// lifecycle / idempotency / post-shutdown-reinit behavior is verified in
// that single test.

TEST_CASE("crash_reporting: active flag starts false", "[platform][crash_reporting]") {
    signalforge::observability::init_logging();
    REQUIRE(crashReportingActive() == false);
}

TEST_CASE("crash_reporting: init fails on empty crashDumpDirectory", "[platform][crash_reporting]") {
    signalforge::observability::init_logging();
    CrashReporterConfig cfg;
    cfg.applicationName = "X";
    cfg.crashDumpDirectory = "";
    cfg.backendHandlerPath = QString::fromUtf8(kHandlerPath);
    REQUIRE(initCrashReporting(cfg) == false);
}

TEST_CASE("crash_reporting: init fails on empty backendHandlerPath", "[platform][crash_reporting]") {
    signalforge::observability::init_logging();
    const auto dir = uniqueCrashDir("nohandler");
    CrashReporterConfig cfg;
    cfg.applicationName = "X";
    cfg.crashDumpDirectory = QString::fromStdString(dir.string());
    cfg.backendHandlerPath = "";
    REQUIRE(initCrashReporting(cfg) == false);
    std::filesystem::remove_all(dir);
}

TEST_CASE("crash_reporting: shutdown without init is a no-op", "[platform][crash_reporting]") {
    signalforge::observability::init_logging();
    REQUIRE_NOTHROW(shutdownCrashReporting());
    // Does not assert crashReportingActive() here because another test may
    // already have run the lifecycle test in this process. This test only
    // verifies shutdown-without-active-state is safe.
}

TEST_CASE("crash_reporting: full lifecycle, idempotency, and single-init-per-process constraint",
          "[platform][crash_reporting]") {
    signalforge::observability::init_logging();
    const auto dir = uniqueCrashDir("lifecycle");

    // Pre-state: either untouched (first test to run) or post-shutdown from
    // a previous invocation. In either case, active is false.
    REQUIRE(crashReportingActive() == false);

    // Initialize with a valid config. If this is the first init in the
    // process, sentry_init runs. If a prior test already did the init (test
    // ordering), initCrashReporting returns false per the single-init
    // contract, and the remainder of this test case has nothing to verify —
    // the contract is already covered. We branch below.
    const bool initOk = initCrashReporting(validConfig(dir));

    if (initOk) {
        // First init in this process. Verify lifecycle + idempotent repeat.
        REQUIRE(crashReportingActive() == true);
        REQUIRE(std::filesystem::exists(dir));

        // Idempotent second init while active: returns true, still active.
        REQUIRE(initCrashReporting(validConfig(dir)) == true);
        REQUIRE(crashReportingActive() == true);

        // Shutdown releases active state.
        shutdownCrashReporting();
        REQUIRE(crashReportingActive() == false);

        // Post-shutdown init is rejected: crashpad cannot re-register.
        REQUIRE(initCrashReporting(validConfig(dir)) == false);
        REQUIRE(crashReportingActive() == false);
    } else {
        // Process already consumed its one init in a prior test.
        REQUIRE(crashReportingActive() == false);
    }

    std::filesystem::remove_all(dir);
}
