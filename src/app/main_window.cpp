#include "main_window.hpp"

#include "app/connection_manager.hpp"

#include <QAction>
#include <QMenuBar>

namespace signalforge::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SignalForge"));
    resize(1280, 800);

    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    auto* openConn = fileMenu->addAction(QStringLiteral("&Connection Manager..."));
    openConn->setShortcut(QKeySequence(QStringLiteral("Ctrl+M")));
    connect(openConn, &QAction::triggered, this, &MainWindow::openConnectionManager);
}

MainWindow::~MainWindow() = default;

void MainWindow::openConnectionManager() {
    if (!connectionManager_) {
        connectionManager_ = std::make_unique<ConnectionManager>(this);
        connectionManager_->setModal(false);
    }
    connectionManager_->show();
    connectionManager_->raise();
    connectionManager_->activateWindow();
}

}  // namespace signalforge::app
