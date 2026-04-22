// src/app/main.cpp
#include "main_window.hpp"
#include "observability/logging.hpp"

#include <QApplication>

int main(int argc, char** argv) {
    signalforge::observability::init_logging();
    SF_LOG_INFO("SignalForge starting");

    QApplication app(argc, argv);
    signalforge::app::MainWindow window;
    window.show();
    const int rc = app.exec();

    SF_LOG_INFO("SignalForge exiting, rc={}", rc);
    return rc;
}
