// tests/unit/drivers/driver_configs_test.cpp
#include "drivers/driver_configs.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace signalforge::drivers;

TEST_CASE("SerialConfig: defaults", "[drivers][config]") {
    SerialConfig c;
    REQUIRE(c.device.isEmpty());
    REQUIRE(c.baudRate == 115200);
    REQUIRE(c.dataBits == 8);
    REQUIRE(c.parity == QStringLiteral("none"));
    REQUIRE(c.stopBits == 1);
    REQUIRE(c.flowControl == QStringLiteral("none"));
}

TEST_CASE("SerialConfig: fields populate", "[drivers][config]") {
    SerialConfig c;
    c.device = QStringLiteral("/dev/ttyUSB0");
    c.baudRate = 921600;
    c.dataBits = 7;
    c.parity = QStringLiteral("even");
    c.stopBits = 2;
    c.flowControl = QStringLiteral("hardware");
    REQUIRE(c.device == "/dev/ttyUSB0");
    REQUIRE(c.baudRate == 921600);
    REQUIRE(c.dataBits == 7);
    REQUIRE(c.parity == "even");
    REQUIRE(c.stopBits == 2);
    REQUIRE(c.flowControl == "hardware");
}

TEST_CASE("TcpConfig: defaults", "[drivers][config]") {
    TcpConfig c;
    REQUIRE(c.host.isEmpty());
    REQUIRE(c.port == 0);
    REQUIRE(c.connectTimeout == std::chrono::milliseconds{5000});
}

TEST_CASE("TcpConfig: fields populate", "[drivers][config]") {
    TcpConfig c;
    c.host = QStringLiteral("127.0.0.1");
    c.port = 8080;
    c.connectTimeout = std::chrono::milliseconds{250};
    REQUIRE(c.host == "127.0.0.1");
    REQUIRE(c.port == 8080);
    REQUIRE(c.connectTimeout == std::chrono::milliseconds{250});
}

TEST_CASE("UdpConfig: defaults", "[drivers][config]") {
    UdpConfig c;
    REQUIRE(c.localBindAddress == QStringLiteral("0.0.0.0"));
    REQUIRE(c.localBindPort == 0);
    REQUIRE(c.remoteHost.isEmpty());
    REQUIRE(c.remotePort == 0);
    REQUIRE(c.multicastGroup.isEmpty());
    REQUIRE(c.multicastTtl == 1);
}

TEST_CASE("UdpConfig: fields populate (unicast)", "[drivers][config]") {
    UdpConfig c;
    c.localBindAddress = QStringLiteral("127.0.0.1");
    c.localBindPort = 12345;
    c.remoteHost = QStringLiteral("127.0.0.1");
    c.remotePort = 12346;
    REQUIRE(c.localBindAddress == "127.0.0.1");
    REQUIRE(c.localBindPort == 12345);
    REQUIRE(c.remoteHost == "127.0.0.1");
    REQUIRE(c.remotePort == 12346);
}

TEST_CASE("UdpConfig: fields populate (multicast)", "[drivers][config]") {
    UdpConfig c;
    c.localBindPort = 12345;
    c.multicastGroup = QStringLiteral("224.0.0.1");
    c.multicastTtl = 4;
    REQUIRE(c.multicastGroup == "224.0.0.1");
    REQUIRE(c.multicastTtl == 4);
}

TEST_CASE("ReplayConfig: defaults", "[drivers][config]") {
    ReplayConfig c;
    REQUIRE(c.sessionFilePath.isEmpty());
}

TEST_CASE("ReplayConfig: field populates", "[drivers][config]") {
    ReplayConfig c;
    c.sessionFilePath = QStringLiteral("/tmp/foo.sfreplay");
    REQUIRE(c.sessionFilePath == "/tmp/foo.sfreplay");
}

TEST_CASE("Configs: copyable value semantics", "[drivers][config]") {
    SerialConfig a;
    a.device = QStringLiteral("/dev/x");
    SerialConfig b = a;
    a.device = QStringLiteral("/dev/y");
    REQUIRE(b.device == "/dev/x");
    REQUIRE(a.device == "/dev/y");
}
