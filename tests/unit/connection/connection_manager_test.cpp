// tests/unit/connection/connection_manager_test.cpp
//
// S3 — ConnectionManager CRUD + signal forwarding tests.

#include "connection/connection_manager.hpp"
#include "decode/decoder_registrar.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QThread>
#include <QtTest/QtTest>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace conn = signalforge::connection;
namespace dr = signalforge::drivers;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "connection_manager_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

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
        QThread::yieldCurrentThread();
    }
    return c.state() == desired;
}

conn::ConnectionConfig makeReplayConfig(const QString& id = {}, const QString& path = {}) {
    conn::ConnectionConfig cfg;
    cfg.id = id;
    cfg.displayName = QStringLiteral("Replay test");
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig rc;
    rc.sessionFilePath =
        path.isEmpty() ? QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay") : path;
    cfg.driverConfig = rc;
    return cfg;
}

}  // namespace

TEST_CASE("S3: empty manager reports zero counts", "[connection-manager][s3]") {
    TestFixture fx;
    auto& m = fx.manager();
    REQUIRE(m.connectionCount() == 0);
    REQUIRE(m.connectedCount() == 0);
    REQUIRE(m.erroredCount() == 0);
    REQUIRE(m.connectionIds().isEmpty());
    REQUIRE(m.connection(QStringLiteral("anything")) == nullptr);
}

TEST_CASE("S3: addConnection generates id when empty and emits signal", "[connection-manager][s3][crud]") {
    TestFixture fx;
    auto& m = fx.manager();
    QSignalSpy spy(&m, &conn::ConnectionManager::connectionAdded);

    const QString id = m.addConnection(makeReplayConfig());
    REQUIRE_FALSE(id.isEmpty());
    REQUIRE(m.connectionCount() == 1);
    REQUIRE(m.connection(id) != nullptr);
    REQUIRE(m.connection(id)->config().id == id);
    REQUIRE(spy.count() == 1);
    REQUIRE(spy.takeFirst().at(0).toString() == id);
}

TEST_CASE("S3: addConnection honors caller-supplied id when free", "[connection-manager][s3][crud]") {
    TestFixture fx;
    auto& m = fx.manager();
    const QString id = m.addConnection(makeReplayConfig(QStringLiteral("my-favorite")));
    REQUIRE(id == QStringLiteral("my-favorite"));
}

TEST_CASE("S3: addConnection rejects duplicate id", "[connection-manager][s3][crud]") {
    TestFixture fx;
    auto& m = fx.manager();
    const QString id1 = m.addConnection(makeReplayConfig(QStringLiteral("dup")));
    REQUIRE(id1 == QStringLiteral("dup"));
    const QString id2 = m.addConnection(makeReplayConfig(QStringLiteral("dup")));
    REQUIRE(id2.isEmpty());
    REQUIRE(m.connectionCount() == 1);
}

TEST_CASE("S3: removeConnection rejects non-Idle and removes Idle", "[connection-manager][s3][crud]") {
    TestFixture fx;
    auto& m = fx.manager();
    const QString id = m.addConnection(makeReplayConfig(QStringLiteral("r")));
    REQUIRE_FALSE(id.isEmpty());

    QSignalSpy removedSpy(&m, &conn::ConnectionManager::connectionRemoved);

    // Connect → cannot remove.
    REQUIRE(m.connectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Connected));
    REQUIRE_FALSE(m.removeConnection(id));
    REQUIRE(m.connectionCount() == 1);

    // Disconnect → can remove.
    REQUIRE(m.disconnectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Idle));
    REQUIRE(m.removeConnection(id));
    REQUIRE(m.connectionCount() == 0);
    REQUIRE(removedSpy.count() == 1);
    REQUIRE(removedSpy.takeFirst().at(0).toString() == id);
}

TEST_CASE("S3: editConnection rejects non-Idle, accepts Idle, preserves id", "[connection-manager][s3][crud]") {
    TestFixture fx;
    auto& m = fx.manager();
    const QString id = m.addConnection(makeReplayConfig(QStringLiteral("e")));
    REQUIRE(m.connection(id)->config().displayName == QStringLiteral("Replay test"));

    // Idle → success; id preserved even if newConfig.id differs.
    auto newCfg = makeReplayConfig(QStringLiteral("ignored"));
    newCfg.displayName = QStringLiteral("Renamed");
    REQUIRE(m.editConnection(id, newCfg));
    REQUIRE(m.connection(id)->config().id == id);  // unchanged
    REQUIRE(m.connection(id)->config().displayName == QStringLiteral("Renamed"));

    // Connect → edit must fail.
    REQUIRE(m.connectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Connected));
    auto cfg2 = newCfg;
    cfg2.displayName = QStringLiteral("Should-not-apply");
    REQUIRE_FALSE(m.editConnection(id, cfg2));
    REQUIRE(m.connection(id)->config().displayName == QStringLiteral("Renamed"));

    REQUIRE(m.disconnectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Idle));
}

TEST_CASE("S3: connectionStateChanged forwards Connection state with id", "[connection-manager][s3][signals]") {
    TestFixture fx;
    auto& m = fx.manager();
    const QString id = m.addConnection(makeReplayConfig(QStringLiteral("forwarded")));

    QSignalSpy stateSpy(&m, &conn::ConnectionManager::connectionStateChanged);

    REQUIRE(m.connectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Connected));
    REQUIRE(stateSpy.count() >= 2);
    // First entry should carry the id.
    REQUIRE(stateSpy.first().at(0).toString() == id);

    REQUIRE(m.disconnectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Idle));
}

TEST_CASE("S3: connectAll / disconnectAll fan out across connections", "[connection-manager][s3][lifecycle]") {
    TestFixture fx;
    auto& m = fx.manager();
    const QString a = m.addConnection(makeReplayConfig(QStringLiteral("a")));
    const QString b = m.addConnection(makeReplayConfig(QStringLiteral("b")));

    m.connectAll();
    REQUIRE(waitForState(*m.connection(a), conn::Connection::State::Connected));
    REQUIRE(waitForState(*m.connection(b), conn::Connection::State::Connected));
    REQUIRE(m.connectedCount() == 2);

    m.disconnectAll();
    REQUIRE(waitForState(*m.connection(a), conn::Connection::State::Idle));
    REQUIRE(waitForState(*m.connection(b), conn::Connection::State::Idle));
    REQUIRE(m.connectedCount() == 0);
}

TEST_CASE("S3: connectionIds returns insertion order", "[connection-manager][s3][lookup]") {
    TestFixture fx;
    auto& m = fx.manager();
    const QString a = m.addConnection(makeReplayConfig(QStringLiteral("alpha")));
    const QString b = m.addConnection(makeReplayConfig(QStringLiteral("beta")));
    const QString c = m.addConnection(makeReplayConfig(QStringLiteral("gamma")));
    QStringList ids = m.connectionIds();
    REQUIRE(ids.size() == 3);
    REQUIRE(ids.at(0) == a);
    REQUIRE(ids.at(1) == b);
    REQUIRE(ids.at(2) == c);
}

TEST_CASE("S3: erroredCount tracks Error-state connections", "[connection-manager][s3][lookup]") {
    TestFixture fx;
    auto& m = fx.manager();
    auto cfg = makeReplayConfig(QStringLiteral("bad"), QStringLiteral("/tmp/no-such-file.sfreplay"));
    const QString id = m.addConnection(cfg);

    // connectDriver returns false synchronously when open() rejects (missing
    // file). Either way the connection lands in Error.
    (void)m.connectConnection(id);
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Error));
    REQUIRE(m.erroredCount() == 1);

    // Recovery to Idle clears the error count.
    REQUIRE(m.disconnectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Idle));
    REQUIRE(m.erroredCount() == 0);
}

TEST_CASE("S3: removeConnection on unknown id returns false", "[connection-manager][s3][crud]") {
    TestFixture fx;
    auto& m = fx.manager();
    REQUIRE_FALSE(m.removeConnection(QStringLiteral("nope")));
}

TEST_CASE("S3: connectConnection / disconnectConnection on unknown id return false",
          "[connection-manager][s3][lifecycle]") {
    TestFixture fx;
    auto& m = fx.manager();
    REQUIRE_FALSE(m.connectConnection(QStringLiteral("nope")));
    REQUIRE_FALSE(m.disconnectConnection(QStringLiteral("nope")));
}
