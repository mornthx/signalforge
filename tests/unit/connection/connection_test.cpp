// tests/unit/connection/connection_test.cpp
//
// S2 — Connection lifecycle state machine + driver dispatch
// tests. Targets ≥ 80% coverage on connection.cpp; the class is
// pure logic plus driver translation.
//
// Uses M3's ReplayDriver against the existing minimal_session
// fixtures (already shipped with the M3 preview ConnectionManager
// integration test). Replay is the only driver type that lets the
// state machine close the full loop without real hardware.

#include "connection/connection.hpp"

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
            static char argv0[] = "connection_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

bool waitForState(const conn::Connection& c, conn::Connection::State desired, int ms = 5000) {
    // ReplayDriver close() is async (queues work to its IO thread,
    // then emits stateChanged(Idle) back to us via QueuedConnection).
    // Under CPU contention the round trip can take longer than the
    // 3 sec the M3 preview test uses; M9 budgets 5 sec to absorb
    // CI variance without masking real regressions.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (c.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::yieldCurrentThread();
    }
    return c.state() == desired;
}

conn::ConnectionConfig makeReplayConfig(const QString& path) {
    conn::ConnectionConfig cfg;
    cfg.id = QStringLiteral("test-replay");
    cfg.displayName = QStringLiteral("Test Replay");
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig rc;
    rc.sessionFilePath = path;
    cfg.driverConfig = rc;
    return cfg;
}

}  // namespace

TEST_CASE("S2: Connection construction with each DriverType succeeds", "[connection][s2][construct]") {
    using DT = conn::DriverType;
    {
        conn::ConnectionConfig cfg;
        cfg.driverType = DT::Serial;
        cfg.driverConfig = dr::SerialConfig{};
        conn::Connection c{cfg};
        REQUIRE(c.driver() != nullptr);
        REQUIRE(c.state() == conn::Connection::State::Idle);
    }
    {
        conn::ConnectionConfig cfg;
        cfg.driverType = DT::Tcp;
        cfg.driverConfig = dr::TcpConfig{.host = QStringLiteral("127.0.0.1"), .port = 12345};
        conn::Connection c{cfg};
        REQUIRE(c.driver() != nullptr);
    }
    {
        conn::ConnectionConfig cfg;
        cfg.driverType = DT::Udp;
        cfg.driverConfig = dr::UdpConfig{.localBindPort = 12345};
        conn::Connection c{cfg};
        REQUIRE(c.driver() != nullptr);
    }
    {
        conn::ConnectionConfig cfg;
        cfg.driverType = DT::Replay;
        dr::ReplayConfig rc;
        rc.sessionFilePath = QStringLiteral("/tmp/no.sfreplay");
        cfg.driverConfig = rc;
        conn::Connection c{cfg};
        REQUIRE(c.driver() != nullptr);
    }
}

TEST_CASE("S2: connectDriver from Idle transitions through Connecting → Connected", "[connection][s2][lifecycle]") {
    auto cfg = makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay"));
    conn::Connection c{cfg};
    REQUIRE(c.state() == conn::Connection::State::Idle);

    QSignalSpy stateSpy(&c, &conn::Connection::stateChanged);

    REQUIRE(c.connectDriver());

    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    // We saw Connecting at minimum, then Connected. Some drivers
    // may emit further intermediates we do not test here.
    REQUIRE(stateSpy.count() >= 2);
}

TEST_CASE("S2: connectDriver rejects when not in Idle/Error", "[connection][s2][lifecycle]") {
    auto cfg = makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay"));
    conn::Connection c{cfg};
    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));

    // Already connected — second connect should be rejected.
    REQUIRE_FALSE(c.connectDriver());
    REQUIRE(c.state() == conn::Connection::State::Connected);

    // Tear down for clean destruction.
    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S2: disconnectDriver returns Connection to Idle", "[connection][s2][lifecycle]") {
    auto cfg = makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay"));
    conn::Connection c{cfg};
    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S2: disconnectDriver from Idle returns false (no-op)", "[connection][s2][lifecycle]") {
    auto cfg = makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay"));
    conn::Connection c{cfg};
    REQUIRE_FALSE(c.disconnectDriver());
}

TEST_CASE("S2: invalid Replay path drives state to Error", "[connection][s2][error]") {
    auto cfg = makeReplayConfig(QStringLiteral("/tmp/this-file-does-not-exist.sfreplay"));
    conn::Connection c{cfg};

    QSignalSpy errSpy(&c, &conn::Connection::errorOccurred);

    // open() may fail synchronously (returns non-Success) OR be
    // accepted but transition to Error asynchronously. Either way,
    // we expect to land in Error.
    (void)c.connectDriver();  // intentional: we check resulting state
    REQUIRE(waitForState(c, conn::Connection::State::Error));
    REQUIRE_FALSE(c.lastError().isEmpty());
    REQUIRE(errSpy.count() >= 1);
}

TEST_CASE("S2: setConfig only succeeds when Idle", "[connection][s2][config]") {
    auto cfg = makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay"));
    conn::Connection c{cfg};

    // Idle → success, rebuilds driver.
    auto cfg2 =
        makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session_alt.sfreplay"));
    cfg2.id = QStringLiteral("renamed");
    REQUIRE(c.setConfig(cfg2));
    REQUIRE(c.config().id == QStringLiteral("renamed"));

    // After connecting, setConfig must fail.
    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    auto cfg3 = cfg2;
    cfg3.id = QStringLiteral("again");
    REQUIRE_FALSE(c.setConfig(cfg3));
    REQUIRE(c.config().id == QStringLiteral("renamed"));

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S2: error state can be reset to Idle via disconnectDriver", "[connection][s2][error]") {
    auto cfg = makeReplayConfig(QStringLiteral("/tmp/missing-replay.sfreplay"));
    conn::Connection c{cfg};
    (void)c.connectDriver();  // intentional: we check resulting state
    REQUIRE(waitForState(c, conn::Connection::State::Error));

    // disconnect() from Error should clear back to Idle.
    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S2: lastError is empty until an error occurs", "[connection][s2][error]") {
    auto cfg = makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay"));
    conn::Connection c{cfg};
    REQUIRE(c.lastError().isEmpty());
}

TEST_CASE("S2: autoConnectCompleted fires on Connected when no commands configured", "[connection][s2][autoconnect]") {
    auto cfg = makeReplayConfig(QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay"));
    conn::Connection c{cfg};

    QSignalSpy spy(&c, &conn::Connection::autoConnectCompleted);

    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));

    // S2 stub: with empty command list, completed(true) is emitted
    // when state hits Connected. S7 will replace this with real
    // sequencing.
    REQUIRE(spy.count() == 1);
    REQUIRE(spy.takeFirst().at(0).toBool() == true);

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}
