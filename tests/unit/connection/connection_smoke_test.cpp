// tests/unit/connection/connection_smoke_test.cpp
//
// S1 smoke test: confirm the M9 freeze-surface headers compile,
// the static lib links, and basic value types behave. Full
// state-machine + persistence + dialog tests land in S2 / S4 /
// S5.

#include "connection/connection.hpp"
#include "connection/connection_manager.hpp"

#include <catch2/catch_test_macros.hpp>

namespace conn = signalforge::connection;
namespace dr = signalforge::drivers;

TEST_CASE("S1: ConnectionConfig defaults are sane", "[connection][s1][smoke]") {
    conn::ConnectionConfig cfg;
    REQUIRE(cfg.id.isEmpty());
    REQUIRE(cfg.displayName.isEmpty());
    REQUIRE(cfg.driverType == conn::DriverType::Serial);
    REQUIRE_FALSE(cfg.autoConnectOnStartup);
    REQUIRE(cfg.autoConnectCommands.empty());
}

TEST_CASE("S1: DriverType enum values are distinct", "[connection][s1][smoke]") {
    REQUIRE(static_cast<int>(conn::DriverType::Serial) != static_cast<int>(conn::DriverType::Tcp));
    REQUIRE(static_cast<int>(conn::DriverType::Tcp) != static_cast<int>(conn::DriverType::Udp));
    REQUIRE(static_cast<int>(conn::DriverType::Udp) != static_cast<int>(conn::DriverType::Replay));
}

TEST_CASE("S1: Connection::State enum values are distinct", "[connection][s1][smoke]") {
    using S = conn::Connection::State;
    REQUIRE(static_cast<int>(S::Idle) != static_cast<int>(S::Connecting));
    REQUIRE(static_cast<int>(S::Connecting) != static_cast<int>(S::Connected));
    REQUIRE(static_cast<int>(S::Connected) != static_cast<int>(S::Disconnecting));
    REQUIRE(static_cast<int>(S::Disconnecting) != static_cast<int>(S::Error));
}

TEST_CASE("S1: AutoConnectCommand defaults round-trip a payload", "[connection][s1][smoke]") {
    conn::AutoConnectCommand cmd;
    cmd.name = QStringLiteral("hello");
    cmd.payload = QByteArray::fromRawData("AB\0CD\r\n", 7);
    cmd.timeout = std::chrono::milliseconds{500};
    cmd.delayBefore = std::chrono::milliseconds{100};
    REQUIRE(cmd.payload.size() == 7);
    REQUIRE(cmd.payload[2] == '\0');
    REQUIRE(cmd.payload[5] == '\r');
    REQUIRE(cmd.timeout.count() == 500);
}

TEST_CASE("S1: DriverConfig variant defaults to SerialConfig", "[connection][s1][smoke]") {
    conn::DriverConfig dc;  // default-constructed variant — first alternative
    REQUIRE(std::holds_alternative<dr::SerialConfig>(dc));
}

TEST_CASE("S1: Connection construction with Replay driver succeeds", "[connection][s1][smoke]") {
    conn::ConnectionConfig cfg;
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig rc;
    rc.sessionFilePath = QStringLiteral("/tmp/does-not-exist.sfreplay");
    rc.playbackSpeed = 2.0;
    rc.loop = true;
    cfg.driverConfig = rc;
    conn::Connection c{cfg};
    REQUIRE(c.state() == conn::Connection::State::Idle);
    REQUIRE(c.driver() != nullptr);
    REQUIRE(c.config().driverType == conn::DriverType::Replay);
    const auto& back = std::get<dr::ReplayConfig>(c.config().driverConfig);
    REQUIRE(back.playbackSpeed == 2.0);
    REQUIRE(back.loop);
}

TEST_CASE("S1: ConnectionManager::defaultConfigPath returns a non-empty path", "[connection][s1][smoke]") {
    const QString p = conn::ConnectionManager::defaultConfigPath();
    REQUIRE_FALSE(p.isEmpty());
    REQUIRE(p.endsWith(QStringLiteral("connections.yaml")));
}
