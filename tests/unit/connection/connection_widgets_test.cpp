// tests/unit/connection/connection_widgets_test.cpp
//
// S6 — ConnectionListWidget + ConnectionStatusWidget tests.
// Forces QT_QPA_PLATFORM=offscreen so the widgets construct
// without a display.

#include "connection/connection_list_widget.hpp"
#include "connection/connection_manager.hpp"
#include "connection/connection_status_widget.hpp"
#include "decode/decoder_registrar.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace conn = signalforge::connection;
namespace dr = signalforge::drivers;

namespace {

class GuiAppHolder {
public:
    GuiAppHolder() {
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "connection_widgets_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QApplication> app_;
};
GuiAppHolder g_app;

class TestFixture {
public:
    TestFixture()
        : pipelineManager_(std::make_unique<signalforge::pipeline::PipelineManager>()),
          decoderRegistrar_(std::make_unique<signalforge::decoder::DecoderRegistrar>(
              pipelineManager_.get(), std::unordered_map<QString, QString>{}, nullptr)),
          manager_(std::make_unique<conn::ConnectionManager>(*decoderRegistrar_)) {}

    conn::ConnectionManager& manager() {
        return *manager_;
    }

private:
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipelineManager_;
    std::unique_ptr<signalforge::decoder::DecoderRegistrar> decoderRegistrar_;
    std::unique_ptr<conn::ConnectionManager> manager_;
};

bool waitForState(const conn::Connection& c, conn::Connection::State desired, int ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (c.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return c.state() == desired;
}

conn::ConnectionConfig makeReplayConfig(const QString& id, const QString& path) {
    conn::ConnectionConfig cfg;
    cfg.id = id;
    cfg.displayName = QStringLiteral("Test ") + id;
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig rc;
    rc.sessionFilePath = path;
    cfg.driverConfig = rc;
    return cfg;
}

}  // namespace

TEST_CASE("S6: ListWidget initial population from existing manager state", "[connection][s6][list]") {
    TestFixture fx;
    const QString id1 = fx.manager().addConnection(makeReplayConfig(
        QStringLiteral("a"), QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay")));
    const QString id2 = fx.manager().addConnection(makeReplayConfig(
        QStringLiteral("b"), QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay")));

    conn::ConnectionListWidget widget(&fx.manager());
    REQUIRE(widget.listWidget()->count() == 2);
}

TEST_CASE("S6: ListWidget grows on connectionAdded, shrinks on connectionRemoved", "[connection][s6][list]") {
    TestFixture fx;
    conn::ConnectionListWidget widget(&fx.manager());
    REQUIRE(widget.listWidget()->count() == 0);

    const QString id = fx.manager().addConnection(makeReplayConfig(
        QStringLiteral("x"), QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay")));
    REQUIRE(widget.listWidget()->count() == 1);

    REQUIRE(fx.manager().removeConnection(id));
    REQUIRE(widget.listWidget()->count() == 0);
}

TEST_CASE("S6: ListWidget row label updates when state changes", "[connection][s6][list]") {
    TestFixture fx;
    const QString id = fx.manager().addConnection(makeReplayConfig(
        QStringLiteral("dyn"), QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay")));
    conn::ConnectionListWidget widget(&fx.manager());
    REQUIRE(widget.listWidget()->item(0)->text().contains(QStringLiteral("Idle")));

    REQUIRE(fx.manager().connectConnection(id));
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Connected));
    REQUIRE(widget.listWidget()->item(0)->text().contains(QStringLiteral("Connected")));

    REQUIRE(fx.manager().disconnectConnection(id));
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Idle));
    REQUIRE(widget.listWidget()->item(0)->text().contains(QStringLiteral("Idle")));
}

TEST_CASE("S6: ListWidget Add button emits addRequested", "[connection][s6][list]") {
    TestFixture fx;
    conn::ConnectionListWidget widget(&fx.manager());
    QSignalSpy spy(&widget, &conn::ConnectionListWidget::addRequested);

    auto* addBtn = widget.findChild<QPushButton*>();  // first push button is "Add"
    REQUIRE(addBtn != nullptr);
    addBtn->click();
    REQUIRE(spy.count() == 1);
}

TEST_CASE("S6: StatusWidget reflects connect/disconnect counts", "[connection][s6][status]") {
    TestFixture fx;
    conn::ConnectionStatusWidget status(&fx.manager());
    REQUIRE(status.label()->text() == QStringLiteral("0/0 connected"));

    const QString id = fx.manager().addConnection(makeReplayConfig(
        QStringLiteral("s1"), QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay")));
    REQUIRE(status.label()->text() == QStringLiteral("0/1 connected"));

    REQUIRE(fx.manager().connectConnection(id));
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Connected));
    REQUIRE(status.label()->text() == QStringLiteral("1/1 connected"));

    REQUIRE(fx.manager().disconnectConnection(id));
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Idle));
    REQUIRE(status.label()->text() == QStringLiteral("0/1 connected"));
}

TEST_CASE("S6: StatusWidget shows error count when any connection is errored", "[connection][s6][status]") {
    TestFixture fx;
    conn::ConnectionStatusWidget status(&fx.manager());

    const QString id = fx.manager().addConnection(
        makeReplayConfig(QStringLiteral("bad"), QStringLiteral("/tmp/missing-file.sfreplay")));
    REQUIRE_FALSE(id.isEmpty());

    (void)fx.manager().connectConnection(id);
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Error));
    REQUIRE(status.label()->text().contains(QStringLiteral("errors: 1")));

    REQUIRE(fx.manager().disconnectConnection(id));
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Idle));
    REQUIRE_FALSE(status.label()->text().contains(QStringLiteral("errors")));
}
