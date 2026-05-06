#include "main_window.hpp"

#include "app/connection_manager.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_registrar.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QAction>
#include <QMenuBar>
#include <unordered_map>

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
    if (!pipelineManager_) {
        pipelineManager_ = std::make_unique<signalforge::pipeline::PipelineManager>(this);
    }
    if (!signalBufferRegistry_) {
        // M6 production sink. Replaces M5's LoggingSignalValueSink.
        signalBufferRegistry_ = std::make_unique<signalforge::buffer::SignalBufferRegistry>();
    }
    if (!decoderRegistrar_) {
        // Non-owning aliased shared_ptr to the registry — the registry
        // outlives the registrar, so the no-op deleter is safe.
        std::shared_ptr<signalforge::decoder::SignalValueSink> sink(signalBufferRegistry_.get(),
                                                                    [](signalforge::decoder::SignalValueSink*) {});
        // Driver-type → schema-path map will be populated by the M9
        // Connection Manager UI; for now, an empty map means no decoder
        // is auto-attached on pipeline creation.
        decoderRegistrar_ = std::make_unique<signalforge::decoder::DecoderRegistrar>(
            pipelineManager_.get(), std::unordered_map<QString, QString>{}, std::move(sink), this);
    }
    if (!connectionManager_) {
        connectionManager_ = std::make_unique<ConnectionManager>(pipelineManager_.get(), this);
        connectionManager_->setModal(false);
    }
    connectionManager_->show();
    connectionManager_->raise();
    connectionManager_->activateWindow();
}

}  // namespace signalforge::app
