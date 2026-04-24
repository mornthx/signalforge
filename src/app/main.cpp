// src/app/main.cpp
#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"
#include "main_window.hpp"
#include "observability/logging.hpp"
#include "platform/app_paths.hpp"
#include "platform/crash_reporting.hpp"

#include <QApplication>
#include <QString>
#include <QStringList>
#include <string_view>

#ifndef SIGNALFORGE_VERSION
#define SIGNALFORGE_VERSION "0.0.2-alpha"
#endif

namespace {

bool hasFlag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string_view{argv[i]} == flag) {
            return true;
        }
    }
    return false;
}

void initializeCrashReporting() {
    signalforge::platform::CrashReporterConfig cfg;
    cfg.applicationName = QStringLiteral("SignalForge");
    cfg.applicationVersion = QStringLiteral(SIGNALFORGE_VERSION);
    cfg.crashDumpDirectory = signalforge::platform::crashDumpDirectory();
    // backendHandlerPath left empty by design: zero-config. sentry-native
    // auto-discovers the handler binary adjacent to the main executable.
    cfg.backendHandlerPath = QString{};

    if (!signalforge::platform::initCrashReporting(cfg)) {
        SF_LOG_WARN("crash reporting unavailable; continuing without minidump support");
    }
}

}  // namespace

int main(int argc, char** argv) {
    signalforge::observability::init_logging();
    SF_LOG_INFO("SignalForge starting (version={})", SIGNALFORGE_VERSION);

    signalforge::frame::registerMetatypes();
    signalforge::drivers::registerMetatypes();

    initializeCrashReporting();

    const bool smokeTest = hasFlag(argc, argv, "--headless-smoke-test");

    QApplication app(argc, argv);
    signalforge::app::MainWindow window;
    window.show();

    int rc = 0;
    if (smokeTest) {
        // Exit after init completes; intended for ctest's app-launch smoke test.
        SF_LOG_INFO("SignalForge --headless-smoke-test: init complete, exiting");
        app.processEvents();
        rc = 0;
    } else {
        rc = app.exec();
    }

    signalforge::platform::shutdownCrashReporting();
    SF_LOG_INFO("SignalForge exiting, rc={}", rc);
    return rc;
}
