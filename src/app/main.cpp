// src/app/main.cpp
#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"
#include "main_window.hpp"
#include "observability/logging.hpp"
#include "platform/app_paths.hpp"
#include "platform/crash_reporting.hpp"

#include <QApplication>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <cstdint>
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

/// Return the value of `--name <value>` if present, else `defaultValue`.
QString flagValue(int argc, char** argv, const char* flag, const QString& defaultValue = {}) {
    for (int i = 1; i < argc - 1; ++i) {
        if (argv[i] != nullptr && std::string_view{argv[i]} == flag) {
            return QString::fromUtf8(argv[i + 1]);
        }
    }
    return defaultValue;
}

/// Count non-clear-color (non-white) pixels in `img`. Used by the M14 S1
/// CI smoke harness's Tier A pixel-diff assertion. Logged via SF_LOG so
/// the harness can grep the log file for `M14_SMOKE_TIER_A: …`.
std::uint64_t countNonClearPixels(const QImage& img) {
    if (img.isNull()) {
        return 0;
    }
    std::uint64_t nonWhite = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = img.pixel(x, y);
            if (qRed(px) != 255 || qGreen(px) != 255 || qBlue(px) != 255) {
                ++nonWhite;
            }
        }
    }
    return nonWhite;
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
    // ADR-010 S8.2: force the linker to retain qrc_qml.cpp.o from
    // libsignalforge_app_ui.a. Static archives only pull in objects
    // whose symbols are referenced; main.cpp otherwise references
    // nothing in qrc_qml.cpp, so its file-scope static initializer
    // (which registers ChartHost.qml at qrc:/qml/ChartHost.qml) was
    // dropped — runtime setSource silently failed and the chart
    // panel rendered blank. Q_INIT_RESOURCE forces retention by
    // taking the address of the generated qInitResources_qml symbol.
    Q_INIT_RESOURCE(qml);

    signalforge::observability::init_logging();
    SF_LOG_INFO("SignalForge starting (version={})", SIGNALFORGE_VERSION);

    signalforge::frame::registerMetatypes();
    signalforge::drivers::registerMetatypes();

    initializeCrashReporting();

    const bool smokeTest = hasFlag(argc, argv, "--headless-smoke-test");

    // M14 S1 CI smoke-test flags. Drive the production binary headlessly
    // through the live-mode chain so the CI release-binary smoke test can
    // assert that decoded signals reach the chart QQuickWidget framebuffer.
    const QString fixturePath = flagValue(argc, argv, "--auto-load-test-fixture");
    const QString autoSignalId = flagValue(argc, argv, "--auto-select-signal");
    const QString dumpPngArg = flagValue(argc, argv, "--dump-chart-png-after-ms");
    const QString dumpPngPathArg = flagValue(argc, argv, "--dump-chart-png-path");
    const bool exitAfterDump = hasFlag(argc, argv, "--exit-after-dump");

    // M14 S6 mechanical 18-test automation flags. Drive recording start +
    // stop programmatically so harnessed tests (T7 round-trip, T8
    // across-restart, T10 mid-stream, T11 backpressure) can be exercised
    // without a human clicking the Record menu.
    const QString autoRecordPath = flagValue(argc, argv, "--auto-record-to");
    const QString autoStopRecordingArg = flagValue(argc, argv, "--auto-stop-recording-after-ms");
    const QString exitAfterMsArg = flagValue(argc, argv, "--exit-after-ms");

    QApplication app(argc, argv);
    signalforge::app::MainWindow window;
    window.show();

    if (!fixturePath.isEmpty()) {
        if (!window.autoLoadTestFixture(fixturePath)) {
            SF_LOG_ERROR("SignalForge: --auto-load-test-fixture failed for '{}'", fixturePath.toStdString());
        }
    }
    if (!autoSignalId.isEmpty()) {
        // Defer the signal-attach until after the connect path has had a
        // chance to register signals into the SignalBufferRegistry; the
        // chart subsystem only renders signals it can actually find.
        QTimer::singleShot(500, &app, [&window, autoSignalId]() {
            if (!window.autoSelectSignal(autoSignalId)) {
                SF_LOG_ERROR("SignalForge: --auto-select-signal '{}' failed", autoSignalId.toStdString());
            }
        });
    }
    if (!autoRecordPath.isEmpty()) {
        // Defer recording start so connections have a chance to reach
        // Connected and SchemaDecoder fires onSignalsRegistered (so the
        // recording's catalog seeds correctly per F6 fix).
        QTimer::singleShot(700, &app, [&window, autoRecordPath]() {
            if (!window.autoStartRecording(autoRecordPath)) {
                SF_LOG_ERROR("SignalForge: --auto-record-to '{}' failed", autoRecordPath.toStdString());
            }
        });
    }
    if (!autoStopRecordingArg.isEmpty()) {
        bool ok = false;
        const int stopMs = autoStopRecordingArg.toInt(&ok);
        if (!ok || stopMs < 0) {
            SF_LOG_ERROR("SignalForge: --auto-stop-recording-after-ms requires non-negative integer; got '{}'",
                         autoStopRecordingArg.toStdString());
        } else {
            QTimer::singleShot(stopMs, &app, [&window]() {
                const std::size_t bytes = window.autoStopRecording();
                SF_LOG_INFO("SignalForge: auto-stop wrote {} bytes", bytes);
            });
        }
    }
    if (!exitAfterMsArg.isEmpty()) {
        bool ok = false;
        const int exitMs = exitAfterMsArg.toInt(&ok);
        if (!ok || exitMs < 0) {
            SF_LOG_ERROR("SignalForge: --exit-after-ms requires non-negative integer; got '{}'",
                         exitAfterMsArg.toStdString());
        } else {
            QTimer::singleShot(exitMs, &app, []() {
                SF_LOG_INFO("SignalForge: --exit-after-ms reached; quitting");
                QApplication::quit();
            });
        }
    }
    if (!dumpPngArg.isEmpty()) {
        bool ok = false;
        const int dumpMs = dumpPngArg.toInt(&ok);
        if (!ok || dumpMs < 0) {
            SF_LOG_ERROR("SignalForge: --dump-chart-png-after-ms requires non-negative integer; got '{}'",
                         dumpPngArg.toStdString());
        } else {
            QTimer::singleShot(dumpMs, &app, [&app, &window, dumpPngPathArg, exitAfterDump]() {
                const QImage img = window.grabChartImage();
                const std::uint64_t nonWhite = countNonClearPixels(img);
                const std::uint64_t total =
                    static_cast<std::uint64_t>(img.width()) * static_cast<std::uint64_t>(img.height());
                SF_LOG_INFO("M14_SMOKE_TIER_A: non_white_pixels={} total_pixels={} width={} height={}", nonWhite, total,
                            img.width(), img.height());
                if (!dumpPngPathArg.isEmpty()) {
                    if (!img.save(dumpPngPathArg)) {
                        SF_LOG_ERROR("SignalForge: chart PNG save failed for '{}'", dumpPngPathArg.toStdString());
                    } else {
                        SF_LOG_INFO("SignalForge: chart PNG saved to '{}'", dumpPngPathArg.toStdString());
                    }
                }
                if (exitAfterDump) {
                    SF_LOG_INFO("SignalForge: --exit-after-dump: requesting clean shutdown");
                    QTimer::singleShot(50, &app, []() { QApplication::quit(); });
                }
            });
        }
    }

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
