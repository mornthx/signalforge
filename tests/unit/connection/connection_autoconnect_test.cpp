// tests/unit/connection/connection_autoconnect_test.cpp
//
// S7 — AutoConnectCommandSequence behavior tests.
//
// Uses ReplayDriver with the existing minimal_session fixture so
// the connection can reach Connected without real hardware. The
// driver's write() returns NotConfigured (read-only), so any
// command that issues a write will surface the "abort sequence"
// path — exactly what spec §3.5 requires for the
// "ERROR-and-abort on driver write failure" branch.
//
// Integration tests with real (loopback) writes land in S10.

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
            static char argv0[] = "connection_autoconnect_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

bool waitForState(const conn::Connection& c, conn::Connection::State desired, int ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (c.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::yieldCurrentThread();
    }
    return c.state() == desired;
}

bool waitForSignal(QSignalSpy& spy, int targetCount, int ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (spy.count() < targetCount && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::yieldCurrentThread();
    }
    return spy.count() >= targetCount;
}

conn::ConnectionConfig makeReplayConfig(std::vector<conn::AutoConnectCommand> cmds) {
    conn::ConnectionConfig cfg;
    cfg.id = QStringLiteral("auto-test");
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig rc;
    rc.sessionFilePath = QStringLiteral(SIGNALFORGE_FIXTURES_DIR) + QStringLiteral("/minimal_session.sfreplay");
    cfg.driverConfig = rc;
    cfg.autoConnectCommands = std::move(cmds);
    return cfg;
}

}  // namespace

TEST_CASE("S7: empty command list emits autoConnectCompleted(true) immediately", "[connection][s7][autoconnect]") {
    auto cfg = makeReplayConfig({});
    conn::Connection c{cfg};
    QSignalSpy spy(&c, &conn::Connection::autoConnectCompleted);

    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    REQUIRE(waitForSignal(spy, 1));
    REQUIRE(spy.takeFirst().at(0).toBool() == true);

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S7: ReplayDriver write() failure aborts sequence with completed(false)",
          "[connection][s7][autoconnect][error]") {
    // ReplayDriver returns NotConfigured for write(). The
    // sequence must abort and emit autoConnectCompleted(false).
    conn::AutoConnectCommand cmd;
    cmd.name = QStringLiteral("won't-go");
    cmd.payload = QByteArray("hi");
    cmd.timeout = std::chrono::milliseconds{100};
    auto cfg = makeReplayConfig({cmd});
    conn::Connection c{cfg};

    QSignalSpy sentSpy(&c, &conn::Connection::autoConnectCommandSent);
    QSignalSpy completedSpy(&c, &conn::Connection::autoConnectCompleted);

    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    REQUIRE(waitForSignal(completedSpy, 1));
    // Connection stays Connected per spec §3.5.
    REQUIRE(c.state() == conn::Connection::State::Connected);
    REQUIRE(sentSpy.count() == 1);
    REQUIRE(completedSpy.takeFirst().at(0).toBool() == false);

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S7: cancelled mid-sequence when connection moves to Disconnecting",
          "[connection][s7][autoconnect][cancel]") {
    // Two commands with a delayBefore on the second. Disconnect
    // before the second fires; we should land in Idle without
    // crash.
    conn::AutoConnectCommand c1;
    c1.payload = QByteArray("ping");

    conn::AutoConnectCommand c2;
    c2.payload = QByteArray("pong");
    c2.delayBefore = std::chrono::milliseconds{1500};

    auto cfg = makeReplayConfig({c1, c2});
    conn::Connection c{cfg};

    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));

    // Disconnect almost immediately; the second command's
    // delayBefore won't elapse.
    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S7: autoConnectCommandSent fires per command before write()", "[connection][s7][autoconnect][signals]") {
    conn::AutoConnectCommand cmd;
    cmd.name = QStringLiteral("only-cmd");
    cmd.payload = QByteArray("X");
    auto cfg = makeReplayConfig({cmd});
    conn::Connection c{cfg};

    QSignalSpy sentSpy(&c, &conn::Connection::autoConnectCommandSent);

    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    REQUIRE(waitForSignal(sentSpy, 1));
    REQUIRE(sentSpy.takeFirst().at(0).toString() == QStringLiteral("only-cmd"));

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}
