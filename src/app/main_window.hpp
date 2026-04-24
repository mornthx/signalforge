#pragma once

#include <QMainWindow>
#include <memory>

namespace signalforge::app {

class ConnectionManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void openConnectionManager();

private:
    std::unique_ptr<ConnectionManager> connectionManager_;
};

}  // namespace signalforge::app
