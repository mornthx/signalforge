#include "main_window.hpp"

#include <QAction>
#include <QDebug>
#include <QDir>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QQmlContext>
#include <QQuickItem>
#include <QTest>
#include <QUrl>
#include <QWindow>

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
    case 4:
    case 5:
        qWarning() << "[spike] Check" << check_id << "not yet implemented in this subtask";
        return 2;
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
