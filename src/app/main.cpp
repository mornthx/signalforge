// src/app/main.cpp
#include "app_style.hpp"
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
    const QString autoDashSignalsArg = flagValue(argc, argv, "--auto-dashboard-signals");
    const bool autoAddTableFlag = hasFlag(argc, argv, "--auto-add-table");
    const bool autoAddBarFlag = hasFlag(argc, argv, "--auto-add-bar");
    const bool autoAddGaugeFlag = hasFlag(argc, argv, "--auto-add-gauge");
    const QString dumpPngArg = flagValue(argc, argv, "--dump-chart-png-after-ms");
    const QString dumpPngPathArg = flagValue(argc, argv, "--dump-chart-png-path");
    const bool exitAfterDump = hasFlag(argc, argv, "--exit-after-dump");

    // M14 S6 mechanical 18-test automation flags. Drive recording start +
    // stop programmatically so harnessed tests (T7 round-trip, T8
    // across-restart, T10 mid-stream, T11 backpressure) can be exercised
    // without a human clicking the Record menu.
    const QString autoRecordPath = flagValue(argc, argv, "--auto-record-to");
    const QString autoStopRecordingArg = flagValue(argc, argv, "--auto-stop-recording-after-ms");
    const QString autoCloseWindowArg = flagValue(argc, argv, "--auto-close-window-after-ms");
    const QString exitAfterMsArg = flagValue(argc, argv, "--exit-after-ms");

    // M15 S1 full-window screenshot capture (mechanism C per
    // M15-concerns C1). Distinct from --dump-chart-png-* which
    // captures the QQuickWidget framebuffer only. Used by
    // tests/visual/ harness for state-machine-complete baseline
    // coverage (M15-concerns C3).
    const QString captureMsArg = flagValue(argc, argv, "--capture-screenshot-after-ms");
    const QString capturePathArg = flagValue(argc, argv, "--capture-screenshot-path");

    // M15 S3 Round 2 baseline-capture flags. `--auto-no-connect` is a
    // sibling to `--auto-load-test-fixture`; both load a YAML, but the
    // no-connect variant skips connectAll() so Idle states (C3 §02)
    // are observable. `--auto-add-charts <n>` invokes the same
    // `+ Chart` action `n` extra times for multi-chart baselines
    // (C3 §36, §37). Both are non-frozen MainWindow surfaces.
    const QString fixtureNoConnectPath = flagValue(argc, argv, "--auto-no-connect");
    const QString autoAddChartsArg = flagValue(argc, argv, "--auto-add-charts");

    // M15 S3 Round 3 replay-state baseline-capture flags. Mirror the
    // GUI Open-Session / Play / Pause / Scrub flow without the modal
    // QFileDialog or the connection-pause confirmation, so replay
    // visual baselines (C3 §17–§20) can be captured deterministically
    // under xvfb. `--auto-replay-play-after-ms` schedules a play()
    // call once the load has settled; `--auto-replay-seek-percent`
    // jumps to a specific position before capture.
    const QString autoReplayLoadPath = flagValue(argc, argv, "--auto-load-replay");
    const QString autoReplayPlayAfterArg = flagValue(argc, argv, "--auto-replay-play-after-ms");
    const QString autoReplayPauseAfterArg = flagValue(argc, argv, "--auto-replay-pause-after-ms");
    const QString autoReplayStepToEndAfterArg = flagValue(argc, argv, "--auto-replay-step-to-end-after-ms");
    const QString autoReplaySeekPercentArg = flagValue(argc, argv, "--auto-replay-seek-percent");
    const QString autoBufferStatusArg = flagValue(argc, argv, "--auto-buffer-status");
    const QString autoChartStatusArg = flagValue(argc, argv, "--auto-chart-status");
    const QString autoConfigSaveStatusArg = flagValue(argc, argv, "--auto-config-save-status");
    const QString autoConnectionStateArg = flagValue(argc, argv, "--auto-connection-state");
    const QString autoM19ModalArg = flagValue(argc, argv, "--auto-m19-modal");
    const QString autoFocusWidgetArg = flagValue(argc, argv, "--auto-focus-widget");
    const QString themeArg = flagValue(argc, argv, "--theme");

    // M15 S3 Round 4 menu/dialog baseline-capture flags. Full-screen
    // capture (`--capture-fullscreen-*`) captures top-level windows
    // outside MainWindow (open menu popups, modal dialogs); pairs
    // with `--auto-open-menu`, `--auto-open-dialog` to put the GUI
    // into the target state before capture. Used for C3 §24–§26
    // (connection dialogs) and §30–§32 (menu open).
    const QString captureFsMsArg = flagValue(argc, argv, "--capture-fullscreen-after-ms");
    const QString captureFsPathArg = flagValue(argc, argv, "--capture-fullscreen-path");
    const QString autoOpenMenuArg = flagValue(argc, argv, "--auto-open-menu");
    const QString autoOpenDialogArg = flagValue(argc, argv, "--auto-open-dialog");
    const QString autoOpenDialogDriverArg = flagValue(argc, argv, "--auto-open-dialog-driver");
    const bool autoOpenDialogAdvanced = hasFlag(argc, argv, "--auto-open-dialog-advanced");
    const QString autoReplaySpeedPopupIndexArg = flagValue(argc, argv, "--auto-replay-speed-popup-index");

    // M16 S5 — `--dump-render-env <path>`: emit env-sidecar JSON
    // (Tier 1/2/3/4 per `docs/v0.3/rendering-environment-lock.md`)
    // then exit without constructing MainWindow. Used by
    // tests/visual/scripts/dump_render_env.py for standalone env
    // capture (CI smoke / operator forensic).
    const QString dumpRenderEnvPath = flagValue(argc, argv, "--dump-render-env");

    // M16 S4 — load qrc resources containing bundled fonts +
    // tokens.qss BEFORE constructing QApplication so the resources
    // are registered when SignalForgeStyle queries them.
    Q_INIT_RESOURCE(fonts);
    Q_INIT_RESOURCE(styles);

    QApplication app(argc, argv);

    // M16 S4 — Establish the M16 visual-identity contract on
    // `app` BEFORE constructing MainWindow. Force Fusion +
    // bundled fonts + 18-role palette + tokens.qss. Replaces
    // the M16 S0.5 R13 `--m16-spike-stack` ephemeral path.
    // Spike artifact (.m16-spike/fonts/Inter-Regular.otf,
    // sha256 be6d709d...) is now resources/fonts/Inter-Regular.otf
    // (byte-identical) so the empirical S0.5 cross-env determinism
    // (0.12 % / 0.30 %) carries through. See
    // docs/v0.3/spike-result.md §6 for the design path.
    signalforge::app::SignalForgeStyle::applyAtStartup(&app);
    if (!themeArg.isEmpty()) {
        bool themeOk = false;
        const auto theme = signalforge::app::SignalForgeStyle::themeFromName(themeArg, &themeOk);
        if (themeOk) {
            signalforge::app::SignalForgeStyle::setActiveTheme(theme);
        } else {
            SF_LOG_ERROR("SignalForge: --theme '{}' unknown (expected light|dark|high_contrast)",
                         themeArg.toStdString());
        }
    }

    // M16 S5 — standalone env-dump mode: emit sidecar then exit.
    // No MainWindow construction (the dump only needs QApplication
    // + SignalForgeStyle state).
    if (!dumpRenderEnvPath.isEmpty()) {
        const bool ok = signalforge::app::SignalForgeStyle::dumpEnvironmentJson(dumpRenderEnvPath);
        return ok ? 0 : 1;
    }

    signalforge::app::MainWindow window;
    window.show();

    if (!fixturePath.isEmpty()) {
        if (!window.autoLoadTestFixture(fixturePath)) {
            SF_LOG_ERROR("SignalForge: --auto-load-test-fixture failed for '{}'", fixturePath.toStdString());
        }
    }
    if (!fixtureNoConnectPath.isEmpty()) {
        if (!window.autoLoadFixtureNoConnect(fixtureNoConnectPath)) {
            SF_LOG_ERROR("SignalForge: --auto-no-connect failed for '{}'", fixtureNoConnectPath.toStdString());
        }
    }
    if (!autoAddChartsArg.isEmpty()) {
        bool ok = false;
        const int extra = autoAddChartsArg.toInt(&ok);
        if (!ok || extra < 0) {
            SF_LOG_ERROR("SignalForge: --auto-add-charts requires non-negative integer; got '{}'",
                         autoAddChartsArg.toStdString());
        } else if (extra > 0) {
            // Synchronous invocation BEFORE the event loop spins:
            // the initial chart's QQuickWidget hasn't begun its
            // setSource path yet, so rebuildChartWidgets() can
            // tear down + rebuild without racing against QML
            // scene-graph init. Subsequent `--capture-screenshot`
            // QTimer fires after the rebuilt widgets settle.
            (void)window.autoAddCharts(extra);
        }
    }
    if (!autoReplayLoadPath.isEmpty()) {
        // Defer the replay-load past show() + initial chart QML
        // init so the Replay toolbar overlay + signal-selector
        // tree are populated when the screenshot QTimer fires.
        QTimer::singleShot(200, &app, [&window, autoReplayLoadPath]() {
            if (!window.autoLoadReplaySession(autoReplayLoadPath)) {
                SF_LOG_ERROR("SignalForge: --auto-load-replay failed for '{}'", autoReplayLoadPath.toStdString());
            }
        });
    }
    if (!autoReplayPlayAfterArg.isEmpty()) {
        bool ok = false;
        const int playMs = autoReplayPlayAfterArg.toInt(&ok);
        if (!ok || playMs < 0) {
            SF_LOG_ERROR("SignalForge: --auto-replay-play-after-ms requires non-negative integer; got '{}'",
                         autoReplayPlayAfterArg.toStdString());
        } else {
            QTimer::singleShot(playMs, &app, [&window]() {
                if (!window.autoReplayPlay()) {
                    SF_LOG_ERROR("SignalForge: autoReplayPlay() returned false");
                }
            });
        }
    }
    if (!autoReplayPauseAfterArg.isEmpty()) {
        bool ok = false;
        const int pauseMs = autoReplayPauseAfterArg.toInt(&ok);
        if (!ok || pauseMs < 0) {
            SF_LOG_ERROR("SignalForge: --auto-replay-pause-after-ms requires non-negative integer; got '{}'",
                         autoReplayPauseAfterArg.toStdString());
        } else {
            QTimer::singleShot(pauseMs, &app, [&window]() {
                if (!window.autoReplayPause()) {
                    SF_LOG_ERROR("SignalForge: autoReplayPause() returned false");
                }
            });
        }
    }
    if (!autoReplayStepToEndAfterArg.isEmpty()) {
        bool ok = false;
        const int endMs = autoReplayStepToEndAfterArg.toInt(&ok);
        if (!ok || endMs < 0) {
            SF_LOG_ERROR("SignalForge: --auto-replay-step-to-end-after-ms requires non-negative integer; got '{}'",
                         autoReplayStepToEndAfterArg.toStdString());
        } else {
            QTimer::singleShot(endMs, &app, [&window]() {
                if (!window.autoReplayStepToEnd()) {
                    SF_LOG_ERROR("SignalForge: autoReplayStepToEnd() returned false");
                }
            });
        }
    }
    if (!autoReplaySeekPercentArg.isEmpty()) {
        bool ok = false;
        const int percent = autoReplaySeekPercentArg.toInt(&ok);
        if (!ok || percent < 0 || percent > 100) {
            SF_LOG_ERROR("SignalForge: --auto-replay-seek-percent requires 0-100; got '{}'",
                         autoReplaySeekPercentArg.toStdString());
        } else {
            // Seek runs after load + any play. 700ms covers the
            // 200ms load defer + a 500ms post-load settle window.
            QTimer::singleShot(700, &app, [&window, percent]() {
                if (!window.autoReplaySeekPercent(percent)) {
                    SF_LOG_ERROR("SignalForge: autoReplaySeekPercent({}) returned false", percent);
                }
            });
        }
    }
    if (!autoBufferStatusArg.isEmpty()) {
        QTimer::singleShot(400, &app, [&window, autoBufferStatusArg]() {
            if (!window.autoSetBufferStatusForVisualTest(autoBufferStatusArg)) {
                SF_LOG_ERROR("SignalForge: --auto-buffer-status '{}' failed", autoBufferStatusArg.toStdString());
            }
        });
    }
    if (!autoChartStatusArg.isEmpty()) {
        QTimer::singleShot(400, &app, [&window, autoChartStatusArg]() {
            if (!window.autoSetChartStatusForVisualTest(autoChartStatusArg)) {
                SF_LOG_ERROR("SignalForge: --auto-chart-status '{}' failed", autoChartStatusArg.toStdString());
            }
        });
    }
    if (!autoConfigSaveStatusArg.isEmpty()) {
        QTimer::singleShot(400, &app, [&window, autoConfigSaveStatusArg]() {
            if (!window.autoSetConfigSaveStatusForVisualTest(autoConfigSaveStatusArg)) {
                SF_LOG_ERROR("SignalForge: --auto-config-save-status '{}' failed",
                             autoConfigSaveStatusArg.toStdString());
            }
        });
    }
    if (!autoConnectionStateArg.isEmpty()) {
        QTimer::singleShot(500, &app, [&window, autoConnectionStateArg]() {
            if (!window.autoSetConnectionStateForVisualTest(autoConnectionStateArg)) {
                SF_LOG_ERROR("SignalForge: --auto-connection-state '{}' failed", autoConnectionStateArg.toStdString());
            }
        });
    }
    if (!autoM19ModalArg.isEmpty()) {
        QTimer::singleShot(2000, &app, [&window, autoM19ModalArg]() {
            if (!window.autoShowM19ModalForVisualTest(autoM19ModalArg)) {
                SF_LOG_ERROR("SignalForge: --auto-m19-modal '{}' failed", autoM19ModalArg.toStdString());
            }
        });
    }
    if (!autoFocusWidgetArg.isEmpty()) {
        QTimer::singleShot(900, &app, [&window, autoFocusWidgetArg]() {
            if (!window.autoFocusWidgetForVisualTest(autoFocusWidgetArg)) {
                SF_LOG_ERROR("SignalForge: --auto-focus-widget '{}' failed", autoFocusWidgetArg.toStdString());
            }
        });
    }
    if (!autoOpenMenuArg.isEmpty()) {
        // Menu pops up at 2000 ms; captures schedule for 2500 ms
        // so the menu is fully drawn at capture time.
        QTimer::singleShot(2000, &app, [&window, autoOpenMenuArg]() {
            if (!window.autoOpenMenu(autoOpenMenuArg)) {
                SF_LOG_ERROR("SignalForge: --auto-open-menu '{}' failed", autoOpenMenuArg.toStdString());
            }
        });
    }
    if (!autoOpenDialogArg.isEmpty()) {
        // Dialog shows at 2000 ms (non-modal show()); captures
        // schedule for 2500 ms.
        QTimer::singleShot(2000, &app, [&window, autoOpenDialogArg, autoOpenDialogDriverArg, autoOpenDialogAdvanced]() {
            const QString d = autoOpenDialogArg.toLower();
            bool ok = false;
            if (d == "add" || d == "add-conn") {
                ok = window.autoShowAddConnectionDialog(autoOpenDialogDriverArg, autoOpenDialogAdvanced);
            } else if (d == "edit" || d == "edit-conn") {
                ok = window.autoShowEditConnectionDialog();
            } else {
                SF_LOG_ERROR("SignalForge: --auto-open-dialog '{}' unknown (expected add|edit)",
                             autoOpenDialogArg.toStdString());
            }
            if (!ok) {
                SF_LOG_ERROR("SignalForge: --auto-open-dialog '{}' failed", autoOpenDialogArg.toStdString());
            }
        });
    }
    if (!autoReplaySpeedPopupIndexArg.isEmpty()) {
        bool ok = false;
        const int idx = autoReplaySpeedPopupIndexArg.toInt(&ok);
        if (!ok || idx < 0) {
            SF_LOG_ERROR("SignalForge: --auto-replay-speed-popup-index requires non-negative integer; got '{}'",
                         autoReplaySpeedPopupIndexArg.toStdString());
        } else {
            // Speed-combo popup runs after replay-load (200 ms)
            // + a settle window. 1500 ms gives the toolbar time
            // to lay out before we pop the combo.
            QTimer::singleShot(1500, &app, [&window, idx]() {
                if (!window.autoReplaySpeedComboPopup(idx)) {
                    SF_LOG_ERROR("SignalForge: autoReplaySpeedComboPopup({}) returned false", idx);
                }
            });
        }
    }
    if (!captureFsMsArg.isEmpty()) {
        bool ok = false;
        const int captureMs = captureFsMsArg.toInt(&ok);
        if (!ok || captureMs < 0) {
            SF_LOG_ERROR("SignalForge: --capture-fullscreen-after-ms requires non-negative integer; got '{}'",
                         captureFsMsArg.toStdString());
        } else if (captureFsPathArg.isEmpty()) {
            SF_LOG_ERROR("SignalForge: --capture-fullscreen-after-ms requires --capture-fullscreen-path");
        } else {
            QTimer::singleShot(captureMs, &app, [&window, captureFsPathArg]() {
                if (!window.captureFullScreen(captureFsPathArg)) {
                    SF_LOG_ERROR("SignalForge: --capture-fullscreen-path '{}' failed", captureFsPathArg.toStdString());
                }
                // M16 S5: auto-emit env sidecar alongside the PNG.
                // R14 / H10 enforcement: every visual baseline ships
                // with its `.env.json` contract sidecar so
                // `compare_with_contract` Step-1 pre-check can
                // distinguish env drift (INVALID) from regression.
                const QString sidecar =
                    captureFsPathArg.endsWith(QStringLiteral(".png"))
                        ? captureFsPathArg.left(captureFsPathArg.size() - 4) + QStringLiteral(".env.json")
                        : captureFsPathArg + QStringLiteral(".env.json");
                (void)signalforge::app::SignalForgeStyle::dumpEnvironmentJson(sidecar);
            });
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
    if (!autoDashSignalsArg.isEmpty()) {
        // Defer so connect/registration has populated the registry, then add
        // each signal to the dashboard via the auto-suggest path.
        QTimer::singleShot(600, &app, [&window, autoDashSignalsArg]() {
            for (const QString& id : autoDashSignalsArg.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                (void)window.autoAddDashboardSignal(id.trimmed());
            }
        });
    }
    if (autoAddTableFlag) {
        // Defer so the connect path has registered signals into the registry.
        QTimer::singleShot(650, &app, [&window]() { (void)window.autoAddTablePanel(); });
    }
    if (autoAddBarFlag) {
        QTimer::singleShot(650, &app, [&window]() { (void)window.autoAddBarPanel(); });
    }
    if (autoAddGaugeFlag) {
        QTimer::singleShot(650, &app, [&window]() { (void)window.autoAddGaugePanel(); });
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
    if (!autoCloseWindowArg.isEmpty()) {
        bool ok = false;
        const int closeMs = autoCloseWindowArg.toInt(&ok);
        if (!ok || closeMs < 0) {
            SF_LOG_ERROR("SignalForge: --auto-close-window-after-ms requires non-negative integer; got '{}'",
                         autoCloseWindowArg.toStdString());
        } else {
            QTimer::singleShot(closeMs, &app, [&window]() {
                SF_LOG_INFO("SignalForge: --auto-close-window-after-ms reached; closing window");
                window.close();
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
    if (!captureMsArg.isEmpty()) {
        bool ok = false;
        const int captureMs = captureMsArg.toInt(&ok);
        if (!ok || captureMs < 0) {
            SF_LOG_ERROR("SignalForge: --capture-screenshot-after-ms requires non-negative integer; got '{}'",
                         captureMsArg.toStdString());
        } else if (capturePathArg.isEmpty()) {
            SF_LOG_ERROR("SignalForge: --capture-screenshot-after-ms requires --capture-screenshot-path");
        } else {
            QTimer::singleShot(captureMs, &app, [&window, capturePathArg]() {
                if (!window.captureScreenshot(capturePathArg)) {
                    SF_LOG_ERROR("SignalForge: --capture-screenshot-path '{}' failed", capturePathArg.toStdString());
                }
                // M16 S5: auto-emit env sidecar alongside the PNG
                // (parallel with the --capture-fullscreen-path path
                // above). See that block for R14 / H10 rationale.
                const QString sidecar =
                    capturePathArg.endsWith(QStringLiteral(".png"))
                        ? capturePathArg.left(capturePathArg.size() - 4) + QStringLiteral(".env.json")
                        : capturePathArg + QStringLiteral(".env.json");
                (void)signalforge::app::SignalForgeStyle::dumpEnvironmentJson(sidecar);
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
