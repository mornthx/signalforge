#include "main_window.hpp"

namespace signalforge::app {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("SignalForge"));
    resize(1280, 800);
}

}  // namespace signalforge::app
