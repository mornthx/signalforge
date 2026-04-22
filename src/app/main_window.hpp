#pragma once

#include <QMainWindow>

namespace signalforge::app {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace signalforge::app
