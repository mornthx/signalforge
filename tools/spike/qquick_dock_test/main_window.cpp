#include "main_window.hpp"

#include <QAction>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QProcess>
#include <QQmlContext>
#include <QQuickItem>
#include <QRegularExpression>
#include <QTest>
#include <QTextStream>
#include <QUrl>
#include <QWindow>
#include <algorithm>
#include <cctype>
#include <numeric>
#include <optional>
#include <unistd.h>
#include <vector>

namespace signalforge::spike {

namespace {

constexpr std::array<const char*, 3> kDockLabels = {"Dock 1", "Dock 2", "Dock 3"};

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SignalForge M1 Spike — QQuickWidget in QDockWidget"));
    resize(1280, 800);

    build_docks();
    build_menu_bar();

    qDebug() << "[spike] main window constructed, three docks ready";
}

void MainWindow::build_docks() {
    constexpr std::array<Qt::DockWidgetArea, 3> areas = {
        Qt::LeftDockWidgetArea,
        Qt::RightDockWidgetArea,
        Qt::BottomDockWidgetArea,
    };
    for (std::size_t i = 0; i < docks_.size(); ++i) {
        docks_[i] = make_dock(i, areas[i]);
        addDockWidget(areas[i], docks_[i]);
    }
}

QDockWidget* MainWindow::make_dock(std::size_t index, Qt::DockWidgetArea /*area*/) {
    const QString label = QString::fromLatin1(kDockLabels[index]);
    auto* dock = new QDockWidget(label, this);
    dock->setObjectName(QStringLiteral("dock_%1").arg(index + 1));
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);

    auto* qw = new QQuickWidget(dock);
    qw->setResizeMode(QQuickWidget::SizeRootObjectToView);
    qw->setMinimumSize(320, 240);
    qw->rootContext()->setContextProperty(QStringLiteral("_initialDockId"), label);
    qw->setSource(QUrl(QStringLiteral("qrc:/qml/DockContent.qml")));

    if (qw->status() == QQuickWidget::Error) {
        for (const auto& err : qw->errors()) {
            qWarning() << "[spike] QML load error:" << err.toString();
        }
    } else {
        qDebug() << "[spike] QML loaded for" << label;
    }

    if (auto* root = qw->rootObject()) {
        root->setProperty("dockId", label);
        root->setProperty("dockState", QStringLiteral("docked"));
    }

    dock->setWidget(qw);
    quick_widgets_[index] = qw;

    QObject::connect(dock, &QDockWidget::topLevelChanged, this, [index, label](bool floating) {
        qDebug().nospace() << "[spike] " << label << " topLevelChanged floating=" << floating;
    });

    if (auto* root = qw->rootObject()) {
        const bool ok = QObject::connect(root, SIGNAL(contextMenuRequested(QPointF)), this,
                                         SLOT(on_context_menu_requested(QPointF)));
        if (!ok) {
            qWarning() << "[spike] failed to connect contextMenuRequested for" << label;
        }
    }

    return dock;
}

void MainWindow::build_menu_bar() {
    auto* file_menu = menuBar()->addMenu(QStringLiteral("&File"));

    auto* toggle_action = file_menu->addAction(QStringLiteral("Toggle Dock 1 visibility"));
    toggle_action->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
    QObject::connect(toggle_action, &QAction::triggered, this, &MainWindow::toggle_dock1_visibility);

    file_menu->addSeparator();

    auto* quit_action = file_menu->addAction(QStringLiteral("&Quit"));
    quit_action->setShortcut(QKeySequence::Quit);
    QObject::connect(quit_action, &QAction::triggered, this, &QMainWindow::close);
}

void MainWindow::on_context_menu_requested(const QPointF& pos) {
    // Identify the source QQuickWidget via the sender root item.
    auto* src_root = qobject_cast<QQuickItem*>(sender());
    QQuickWidget* source_qw = nullptr;
    for (auto* qw : quick_widgets_) {
        if (qw != nullptr && qw->rootObject() == src_root) {
            source_qw = qw;
            break;
        }
    }
    if (source_qw == nullptr) {
        qWarning() << "[spike] context menu sender not matched to a QQuickWidget";
        return;
    }

    const QPoint widget_local = pos.toPoint();
    const QPoint global = source_qw->mapToGlobal(widget_local);

    auto* menu = new QMenu(this);
    auto* action_a = menu->addAction(QStringLiteral("Action A"));
    auto* action_b = menu->addAction(QStringLiteral("Action B"));
    QObject::connect(action_a, &QAction::triggered, this, [this]() {
        last_chosen_action_ = QStringLiteral("Action A");
        qDebug() << "[spike] context menu triggered: Action A";
    });
    QObject::connect(action_b, &QAction::triggered, this, [this]() {
        last_chosen_action_ = QStringLiteral("Action B");
        qDebug() << "[spike] context menu triggered: Action B";
    });
    QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);

    qDebug().nospace() << "[spike] showing context menu at global=" << global.x() << "," << global.y();
    menu->popup(global);  // non-modal so --auto-check can drive it
}

void MainWindow::toggle_dock1_visibility() {
    if (docks_[0] == nullptr) {
        return;
    }
    const bool was_visible = docks_[0]->isVisible();
    docks_[0]->setVisible(!was_visible);
    qDebug().nospace() << "[spike] Dock 1 visibility " << was_visible << " -> " << !was_visible;
}

int MainWindow::run_auto_check(int check_id, bool short_variant) {
    qDebug() << "[spike] run_auto_check" << check_id << "short=" << short_variant;
    switch (check_id) {
    case 1:
        return run_check_1_floating();
    case 2:
        return run_check_2_hidpi();
    case 3:
        return run_check_3_context_menu();
    case 4:
        return run_check_4_lifecycle(short_variant);
    case 5:
        return run_check_5_multi_instance_gpu();
    default:
        qWarning() << "[spike] unknown check id:" << check_id;
        return 2;
    }
}

int MainWindow::run_check_1_floating() {
    constexpr int kCycles = 5;
    QDockWidget* dock = docks_[0];
    if (dock == nullptr) {
        qWarning() << "[check1] Dock 1 missing";
        return 2;
    }
    const QString dock_label = dock->windowTitle();

    qDebug().nospace() << "[check1] starting " << kCycles << " float/move/re-dock cycles on " << dock_label;

    for (int i = 1; i <= kCycles; ++i) {
        qDebug().nospace() << "[check1] cycle " << i << "/" << kCycles << " -> float";
        dock->setFloating(true);
        QTest::qWait(800);

        // Programmatic move to simulate cross-monitor drag (approximation — a real
        // drag cannot be synthesized under xvfb, so we exercise the re-render path).
        const QPoint target(80 + i * 90, 60 + i * 55);
        if (auto* win = dock->windowHandle()) {
            win->setPosition(target);
            qDebug().nospace() << "[check1] cycle " << i << " moved floating window to " << target.x() << ","
                               << target.y();
        } else {
            dock->move(target);
            qDebug().nospace() << "[check1] cycle " << i << " moved dock widget to " << target.x() << "," << target.y()
                               << " (no windowHandle)";
        }
        QTest::qWait(400);

        qDebug().nospace() << "[check1] cycle " << i << "/" << kCycles << " -> re-dock";
        dock->setFloating(false);
        QTest::qWait(800);
    }

    // Settle before screenshot so the re-docked state is fully painted.
    QTest::qWait(500);

    const bool saved = save_screenshot(QStringLiteral("check1-end-state.png"));
    qDebug() << "[check1] end-state screenshot saved:" << saved;
    return saved ? 0 : 1;
}

int MainWindow::run_check_2_hidpi() {
    // Check 2's capture is done externally by scrot (see run-check2.sh). The spike
    // only has to show the window, grab focus so scrot -u picks the right target,
    // log observed geometry for later cross-checking against the requested scale,
    // and stay alive long enough for scrot to snap.
    show();
    raise();
    activateWindow();

    const QSize logical = size();
    qDebug().nospace() << "[check2] logical size=" << logical.width() << "x" << logical.height();
    if (auto* win = windowHandle()) {
        const qreal dpr = win->devicePixelRatio();
        const QSize physical(static_cast<int>(logical.width() * dpr), static_cast<int>(logical.height() * dpr));
        qDebug().nospace() << "[check2] devicePixelRatio=" << dpr << " physical size~=" << physical.width() << "x"
                           << physical.height();
    }

    // 3 s is enough for the 30 Hz canvas to paint several frames and for an
    // external scrot invocation (~1.8 s in) to capture the focused window.
    QTest::qWait(3000);

    return 0;
}

int MainWindow::run_check_3_context_menu() {
    // Wait for the MainWindow's layout to settle so QQuickWidgets have non-zero
    // usable geometry. Without this, an auto-check at startup sees dock widgets
    // still being laid out and hits y=0.
    for (int wait_budget = 0; wait_budget < 2000; wait_budget += 50) {
        bool all_sized = true;
        for (auto* qw : quick_widgets_) {
            if (qw == nullptr || qw->width() < 100 || qw->height() < 100) {
                all_sized = false;
                break;
            }
        }
        if (all_sized) {
            break;
        }
        QTest::qWait(50);
    }

    for (std::size_t i = 0; i < quick_widgets_.size(); ++i) {
        QQuickWidget* qw = quick_widgets_[i];
        if (qw == nullptr) {
            qWarning() << "[check3] dock" << (i + 1) << "missing";
            return 2;
        }
        qDebug().nospace() << "[check3] dock " << (i + 1) << " size=" << qw->width() << "x" << qw->height();
        last_chosen_action_.clear();

        const QPoint center = qw->rect().center();
        qDebug().nospace() << "[check3] dock " << (i + 1) << " right-click at local " << center.x() << ","
                           << center.y();
        QTest::mouseClick(qw, Qt::RightButton, Qt::NoModifier, center);

        // Poll for the menu to show (non-modal popup).
        const int kPollBudgetMs = 500;
        int waited_ms = 0;
        while (waited_ms < kPollBudgetMs && QApplication::activePopupWidget() == nullptr) {
            QTest::qWait(20);
            waited_ms += 20;
        }
        auto* popup = QApplication::activePopupWidget();
        if (popup == nullptr) {
            qWarning() << "[check3] dock" << (i + 1) << "popup did not appear within" << kPollBudgetMs << "ms";
            return 1;
        }

        // Snap a screenshot of the menu open on the middle dock (i == 1).
        if (i == 1) {
            save_screenshot(QStringLiteral("check3-menu-screenshot.png"));
        }

        auto* menu = qobject_cast<QMenu*>(popup);
        if (menu == nullptr) {
            qWarning() << "[check3] dock" << (i + 1) << "popup is not a QMenu:" << popup;
            return 1;
        }
        const auto actions = menu->actions();
        if (actions.size() != 2 || actions[0]->text() != QStringLiteral("Action A")) {
            qWarning() << "[check3] dock" << (i + 1) << "unexpected actions count=" << actions.size();
            return 1;
        }

        actions[0]->trigger();
        menu->hide();
        QTest::qWait(200);

        if (last_chosen_action_ != QStringLiteral("Action A")) {
            qWarning() << "[check3] dock" << (i + 1) << "last_chosen_action_ mismatch, got:" << last_chosen_action_;
            return 1;
        }

        if (QApplication::activePopupWidget() != nullptr) {
            qWarning() << "[check3] dock" << (i + 1) << "popup still active after trigger";
            return 1;
        }
        qDebug() << "[check3] dock" << (i + 1) << "pass";
    }
    return 0;
}

namespace {

std::optional<qint64> read_vm_rss_kb() {
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    // procfs file sizes are reported as 0; readLine's heuristics do not cope.
    // readAll() forces a full read into memory.
    const QByteArray content = f.readAll();
    const auto lines = content.split('\n');
    for (const QByteArray& line : lines) {
        if (!line.startsWith("VmRSS:")) {
            continue;
        }
        QByteArray digits;
        for (char c : line) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                digits.append(c);
            } else if (!digits.isEmpty()) {
                break;
            }
        }
        if (digits.isEmpty()) {
            return std::nullopt;
        }
        bool ok = false;
        const qint64 v = digits.toLongLong(&ok);
        return ok ? std::optional<qint64>{v} : std::nullopt;
    }
    return std::nullopt;
}

std::optional<int> count_open_fds() {
    QDir d(QStringLiteral("/proc/self/fd"));
    if (!d.exists()) {
        return std::nullopt;
    }
    return static_cast<int>(d.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).size());
}

}  // namespace

int MainWindow::run_check_4_lifecycle(bool short_variant) {
    const int cycles = short_variant ? 5 : 20;
    QDockWidget* dock = docks_[0];
    if (dock == nullptr) {
        qWarning() << "[check4] Dock 1 missing";
        return 2;
    }

    const QString dir = QStringLiteral("docs/spikes/M1-artifacts");
    if (!QDir().mkpath(dir)) {
        qWarning() << "[check4] could not create artifact dir";
        return 2;
    }
    const QString csv_path = dir + QLatin1Char('/') + QStringLiteral("check4-memory-trace.csv");
    QFile csv(csv_path);
    if (!csv.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "[check4] could not open" << csv_path;
        return 2;
    }
    QTextStream out(&csv);
    out << "cycle,state,rss_kb,fd_count\n";

    auto sample = [&out](int cycle, const char* state) {
        const auto rss = read_vm_rss_kb();
        const auto fds = count_open_fds();
        out << cycle << "," << state << "," << (rss ? QString::number(*rss) : QStringLiteral("NA")) << ","
            << (fds ? QString::number(*fds) : QStringLiteral("NA")) << "\n";
        out.flush();
        qDebug().nospace() << "[check4] cycle=" << cycle << " state=" << state
                           << " rss_kb=" << (rss ? QString::number(*rss) : QStringLiteral("NA"))
                           << " fd_count=" << (fds ? QString::number(*fds) : QStringLiteral("NA"));
    };

    sample(0, "baseline");

    for (int i = 1; i <= cycles; ++i) {
        dock->hide();
        QTest::qWait(500);
        sample(i, "after_hide");

        dock->show();
        QTest::qWait(500);
        sample(i, "after_show");
    }

    csv.close();

    // Success criterion: RSS growth across cycles under 10 MB; open-FD count
    // returns to baseline ± 2. The CSV contains the raw data; report analysis
    // is done at S9 so both the short and full runs contribute evidence.
    qDebug() << "[check4] CSV written:" << csv_path;
    return 0;
}

namespace {

std::optional<qint64> read_vm_size_kb() {
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray content = f.readAll();
    const auto lines = content.split('\n');
    for (const QByteArray& line : lines) {
        if (!line.startsWith("VmSize:")) {
            continue;
        }
        QByteArray digits;
        for (char c : line) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                digits.append(c);
            } else if (!digits.isEmpty()) {
                break;
            }
        }
        bool ok = false;
        const qint64 v = digits.toLongLong(&ok);
        return ok ? std::optional<qint64>{v} : std::nullopt;
    }
    return std::nullopt;
}

struct CpuTicks {
    qint64 utime{0};
    qint64 stime{0};
    qint64 wall_ms{0};
};

std::optional<CpuTicks> read_cpu_ticks() {
    QFile f(QStringLiteral("/proc/self/stat"));
    if (!f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray content = f.readAll();
    // Fields (1-indexed): 14=utime, 15=stime. The comm field can contain
    // spaces/parens; split after the last ')' to be safe.
    const int end_of_comm = content.lastIndexOf(')');
    if (end_of_comm < 0) {
        return std::nullopt;
    }
    const QByteArray after = content.mid(end_of_comm + 1).trimmed();
    const auto fields = after.split(' ');
    // After ')', field indices shift: index 0 = state (field 3 overall),
    // so utime = index 11, stime = index 12.
    if (fields.size() < 13) {
        return std::nullopt;
    }
    bool ok_u = false, ok_s = false;
    const qint64 ut = fields[11].toLongLong(&ok_u);
    const qint64 st = fields[12].toLongLong(&ok_s);
    if (!ok_u || !ok_s) {
        return std::nullopt;
    }
    return CpuTicks{ut, st, QDateTime::currentMSecsSinceEpoch()};
}

std::optional<qint64> read_vram_used_bytes() {
    static const QString kPath = QStringLiteral("/sys/class/drm/card1/device/mem_info_vram_used");
    QFile f(kPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QByteArray content = f.readAll().trimmed();
    bool ok = false;
    const qint64 v = content.toLongLong(&ok);
    return ok ? std::optional<qint64>{v} : std::nullopt;
}

}  // namespace

int MainWindow::run_check_5_multi_instance_gpu() {
    // All three QQuickWidgets already have an animated Canvas running at 30 Hz.
    // Sample process and GPU metrics for 30 s at 500 ms cadence (60 samples).

    const QString dir = QStringLiteral("docs/spikes/M1-artifacts");
    if (!QDir().mkpath(dir)) {
        qWarning() << "[check5] could not create artifact dir";
        return 2;
    }
    QFile csv(dir + QLatin1Char('/') + QStringLiteral("check5-gpu-trace.csv"));
    if (!csv.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "[check5] could not open trace CSV";
        return 2;
    }
    QTextStream out(&csv);
    out << "elapsed_s,cpu_pct,rss_kb,vsize_kb,gpu_pct,vram_bytes_sysfs,vram_mb_radeontop\n";

    QProcess radeontop;
    radeontop.setProcessChannelMode(QProcess::MergedChannels);
    radeontop.setProgram(QStringLiteral("radeontop"));
    radeontop.setArguments({QStringLiteral("-d"), QStringLiteral("-"), QStringLiteral("-i"), QStringLiteral("1"),
                            QStringLiteral("-l"), QStringLiteral("0")});
    qDebug() << "[check5] starting radeontop";
    radeontop.start();
    if (!radeontop.waitForStarted(2000)) {
        qWarning() << "[check5] radeontop failed to start";
        return 2;
    }

    // Calibration wait — radeontop emits the first useful line after ~1–2 s.
    QTest::qWait(2200);

    QString radeontop_buffer;
    double latest_gpu_pct = -1.0;
    double latest_vram_mb = -1.0;
    static const QRegularExpression kGpuRx(QStringLiteral(R"(gpu\s+([0-9.]+)%)"));
    static const QRegularExpression kVramRx(QStringLiteral(R"(vram\s+[0-9.]+%\s+([0-9.]+)mb)"));

    auto pump_radeontop = [&]() {
        radeontop_buffer += QString::fromUtf8(radeontop.readAllStandardOutput());
        int nl = radeontop_buffer.lastIndexOf(QLatin1Char('\n'));
        if (nl < 0) {
            return;
        }
        const QString complete = radeontop_buffer.left(nl);
        radeontop_buffer = radeontop_buffer.mid(nl + 1);
        // Parse only the latest complete line for current readings.
        const int last_line_start = complete.lastIndexOf(QLatin1Char('\n'));
        const QString last_line = (last_line_start < 0) ? complete : complete.mid(last_line_start + 1);
        const auto gpu_m = kGpuRx.match(last_line);
        if (gpu_m.hasMatch()) {
            latest_gpu_pct = gpu_m.captured(1).toDouble();
        }
        const auto vram_m = kVramRx.match(last_line);
        if (vram_m.hasMatch()) {
            latest_vram_mb = vram_m.captured(1).toDouble();
        }
    };

    const auto baseline_cpu = read_cpu_ticks();
    const qint64 wall_start = QDateTime::currentMSecsSinceEpoch();

    std::vector<double> cpu_samples, gpu_samples, vram_samples;
    std::vector<qint64> rss_samples, vsize_samples, vram_bytes_samples;

    auto prev_cpu = baseline_cpu;

    constexpr int kTotalSamples = 60;
    constexpr int kCadenceMs = 500;

    for (int s = 0; s < kTotalSamples; ++s) {
        QTest::qWait(kCadenceMs);
        pump_radeontop();

        const qint64 wall_now = QDateTime::currentMSecsSinceEpoch();
        const double elapsed = (wall_now - wall_start) / 1000.0;
        const auto cpu_now = read_cpu_ticks();
        const auto rss = read_vm_rss_kb();
        const auto vsize = read_vm_size_kb();
        const auto vram_bytes = read_vram_used_bytes();

        double cpu_pct = -1.0;
        if (cpu_now && prev_cpu) {
            const qint64 ticks_delta = (cpu_now->utime + cpu_now->stime) - (prev_cpu->utime + prev_cpu->stime);
            const double wall_delta_s = (cpu_now->wall_ms - prev_cpu->wall_ms) / 1000.0;
            const long hz = ::sysconf(_SC_CLK_TCK);
            if (hz > 0 && wall_delta_s > 0.0) {
                cpu_pct = (static_cast<double>(ticks_delta) / static_cast<double>(hz)) / wall_delta_s * 100.0;
            }
        }
        prev_cpu = cpu_now;

        out << elapsed << "," << (cpu_pct >= 0 ? QString::number(cpu_pct, 'f', 2) : QStringLiteral("NA")) << ","
            << (rss ? QString::number(*rss) : QStringLiteral("NA")) << ","
            << (vsize ? QString::number(*vsize) : QStringLiteral("NA")) << ","
            << (latest_gpu_pct >= 0 ? QString::number(latest_gpu_pct, 'f', 2) : QStringLiteral("NA")) << ","
            << (vram_bytes ? QString::number(*vram_bytes) : QStringLiteral("NA")) << ","
            << (latest_vram_mb >= 0 ? QString::number(latest_vram_mb, 'f', 2) : QStringLiteral("NA")) << "\n";

        if (cpu_pct >= 0)
            cpu_samples.push_back(cpu_pct);
        if (rss)
            rss_samples.push_back(*rss);
        if (vsize)
            vsize_samples.push_back(*vsize);
        if (latest_gpu_pct >= 0)
            gpu_samples.push_back(latest_gpu_pct);
        if (latest_vram_mb >= 0)
            vram_samples.push_back(latest_vram_mb);
        if (vram_bytes)
            vram_bytes_samples.push_back(*vram_bytes);
    }

    radeontop.terminate();
    radeontop.waitForFinished(2000);
    csv.close();

    auto stats = [](const std::vector<double>& v) -> QString {
        if (v.empty())
            return QStringLiteral("(no samples)");
        const double mn = *std::min_element(v.begin(), v.end());
        const double mx = *std::max_element(v.begin(), v.end());
        const double mean = std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
        return QStringLiteral("min=%1 max=%2 mean=%3 n=%4")
            .arg(mn, 0, 'f', 2)
            .arg(mx, 0, 'f', 2)
            .arg(mean, 0, 'f', 2)
            .arg(v.size());
    };
    auto stats_int = [](const std::vector<qint64>& v) -> QString {
        if (v.empty())
            return QStringLiteral("(no samples)");
        const qint64 mn = *std::min_element(v.begin(), v.end());
        const qint64 mx = *std::max_element(v.begin(), v.end());
        const double mean = std::accumulate(v.begin(), v.end(), qint64{0}) / static_cast<double>(v.size());
        return QStringLiteral("min=%1 max=%2 mean=%3 n=%4").arg(mn).arg(mx).arg(mean, 0, 'f', 0).arg(v.size());
    };

    QFile summary(dir + QLatin1Char('/') + QStringLiteral("check5-summary.md"));
    if (summary.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream s(&summary);
        s << "# Check 5 — multi-instance GPU summary\n\n";
        s << "30 s @ 500 ms cadence; three QQuickWidgets each running a 30 Hz canvas.\n\n";
        s << "| Metric | Stats |\n|---|---|\n";
        s << "| cpu_pct (%) | " << stats(cpu_samples) << " |\n";
        s << "| rss_kb | " << stats_int(rss_samples) << " |\n";
        s << "| vsize_kb | " << stats_int(vsize_samples) << " |\n";
        s << "| gpu_pct (%) | " << stats(gpu_samples) << " |\n";
        s << "| vram_bytes_sysfs | " << stats_int(vram_bytes_samples) << " |\n";
        s << "| vram_mb_radeontop | " << stats(vram_samples) << " |\n\n";
        s << "## Spec thresholds (§S7)\n\n";
        s << "- gpu_pct sustained < 60% → see max above\n";
        s << "- cpu_pct < 30% single-core → see max above\n";
        s << "- spike-process GPU memory < 200 MB total — the vram_bytes_sysfs column reports system-wide VRAM usage "
             "(shared iGPU, cannot be attributed per-process); vram_mb_radeontop is also system-wide. Per-process VRAM "
             "attribution is not available from free-tier AMD telemetry. The report treats this as partial evidence "
             "and flags the attribution gap.\n";
        s.flush();
    }

    qDebug() << "[check5] complete:";
    qDebug().noquote() << "  cpu_pct" << stats(cpu_samples);
    qDebug().noquote() << "  gpu_pct" << stats(gpu_samples);
    qDebug().noquote() << "  rss_kb" << stats_int(rss_samples);
    qDebug().noquote() << "  vram_mb_radeontop" << stats(vram_samples);

    return 0;
}

bool MainWindow::save_screenshot(const QString& filename) {
    const QString dir = QStringLiteral("docs/spikes/M1-artifacts");
    if (!QDir().mkpath(dir)) {
        qWarning() << "[spike] failed to create artifact dir" << dir;
        return false;
    }
    const QString path = dir + QLatin1Char('/') + filename;
    const QPixmap pm = this->grab();
    const bool ok = pm.save(path, "PNG");
    qDebug() << "[spike] screenshot" << path << "->" << ok << "size=" << pm.size();
    return ok;
}

}  // namespace signalforge::spike
