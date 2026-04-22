#include "main_window.hpp"

#include <QAction>
#include <QDebug>
#include <QMenu>
#include <QMenuBar>
#include <QQmlContext>
#include <QQuickItem>
#include <QUrl>

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
    case 2:
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

}  // namespace signalforge::spike
