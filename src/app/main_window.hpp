#pragma once

#include <QMainWindow>
#include <memory>

namespace signalforge::pipeline {
class PipelineManager;
}

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
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipelineManager_;
    std::unique_ptr<ConnectionManager> connectionManager_;
};

}  // namespace signalforge::app
