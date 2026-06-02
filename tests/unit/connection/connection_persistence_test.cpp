// tests/unit/connection/connection_persistence_test.cpp
//
// S4 — yaml save/load + canonical schema + binary payload
// round-trip + missing/invalid file paths.

#include "connection/connection_manager.hpp"
#include "decode/decoder_registrar.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace conn = signalforge::connection;
namespace dr = signalforge::drivers;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "connection_persistence_test";
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
    void resetManager() {
        manager_ = std::make_unique<conn::ConnectionManager>(*decoderRegistrar_);
    }

private:
    std::unique_ptr<signalforge::pipeline::PipelineManager> pipelineManager_;
    std::unique_ptr<signalforge::decoder::DecoderRegistrar> decoderRegistrar_;
    std::unique_ptr<conn::ConnectionManager> manager_;
};

}  // namespace

TEST_CASE("S4: round-trip save → load preserves all 4 driver types", "[connection][s4][persistence]") {
    TestFixture fx;
    auto& m = fx.manager();

    // Build one of each driver type.
    {
        conn::ConnectionConfig cfg;
        cfg.id = QStringLiteral("ser1");
        cfg.displayName = QStringLiteral("My Serial");
        cfg.driverType = conn::DriverType::Serial;
        dr::SerialConfig s;
        s.device = QStringLiteral("/dev/ttyUSB0");
        s.baudRate = 921600;
        s.dataBits = 7;
        s.parity = QStringLiteral("even");
        s.stopBits = 2;
        s.flowControl = QStringLiteral("hardware");
        cfg.driverConfig = s;
        cfg.decoderSchemaId = QStringLiteral("dev-board-frame-v1");
        REQUIRE_FALSE(m.addConnection(cfg).isEmpty());
    }
    {
        conn::ConnectionConfig cfg;
        cfg.id = QStringLiteral("tcp1");
        cfg.displayName = QStringLiteral("My TCP");
        cfg.driverType = conn::DriverType::Tcp;
        dr::TcpConfig t;
        t.host = QStringLiteral("192.168.1.50");
        t.port = 4242;
        t.connectTimeout = std::chrono::milliseconds{8000};
        cfg.driverConfig = t;
        REQUIRE_FALSE(m.addConnection(cfg).isEmpty());
    }
    {
        conn::ConnectionConfig cfg;
        cfg.id = QStringLiteral("udp1");
        cfg.driverType = conn::DriverType::Udp;
        dr::UdpConfig u;
        u.localBindAddress = QStringLiteral("127.0.0.1");
        u.localBindPort = 5005;
        u.remoteHost = QStringLiteral("10.0.0.1");
        u.remotePort = 5006;
        u.multicastGroup = QStringLiteral("239.0.0.1");
        u.multicastTtl = 4;
        u.videoEnabled = true;  // M35
        u.videoPort = 5104;     // M35
        cfg.driverConfig = u;
        REQUIRE_FALSE(m.addConnection(cfg).isEmpty());
    }
    {
        conn::ConnectionConfig cfg;
        cfg.id = QStringLiteral("rep1");
        cfg.driverType = conn::DriverType::Replay;
        dr::ReplayConfig r;
        r.sessionFilePath = QStringLiteral("/var/tmp/session.sfreplay");
        r.playbackSpeed = 0.5;
        r.loop = true;
        cfg.driverConfig = r;
        REQUIRE_FALSE(m.addConnection(cfg).isEmpty());
    }

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("conns.yaml"));
    REQUIRE(m.saveConfigFile(path));

    // Reload into a fresh manager.
    fx.resetManager();
    auto& m2 = fx.manager();
    REQUIRE(m2.loadConfigFile(path));
    REQUIRE(m2.connectionCount() == 4);

    // Verify Serial fields.
    auto* serConn = m2.connection(QStringLiteral("ser1"));
    REQUIRE(serConn != nullptr);
    const auto& serCfg = serConn->config();
    REQUIRE(serCfg.driverType == conn::DriverType::Serial);
    const auto& serBack = std::get<dr::SerialConfig>(serCfg.driverConfig);
    REQUIRE(serBack.device == QStringLiteral("/dev/ttyUSB0"));
    REQUIRE(serBack.baudRate == 921600);
    REQUIRE(serBack.dataBits == 7);
    REQUIRE(serBack.parity == QStringLiteral("even"));
    REQUIRE(serBack.stopBits == 2);
    REQUIRE(serBack.flowControl == QStringLiteral("hardware"));
    REQUIRE(serCfg.decoderSchemaId == QStringLiteral("dev-board-frame-v1"));

    // Verify TCP fields.
    auto* tcpConn = m2.connection(QStringLiteral("tcp1"));
    REQUIRE(tcpConn != nullptr);
    const auto& tcpBack = std::get<dr::TcpConfig>(tcpConn->config().driverConfig);
    REQUIRE(tcpBack.host == QStringLiteral("192.168.1.50"));
    REQUIRE(tcpBack.port == 4242);
    REQUIRE(tcpBack.connectTimeout == std::chrono::milliseconds{8000});

    // Verify UDP fields.
    auto* udpConn = m2.connection(QStringLiteral("udp1"));
    REQUIRE(udpConn != nullptr);
    const auto& udpBack = std::get<dr::UdpConfig>(udpConn->config().driverConfig);
    REQUIRE(udpBack.localBindPort == 5005);
    REQUIRE(udpBack.remoteHost == QStringLiteral("10.0.0.1"));
    REQUIRE(udpBack.remotePort == 5006);
    REQUIRE(udpBack.multicastGroup == QStringLiteral("239.0.0.1"));
    REQUIRE(udpBack.multicastTtl == 4u);
    REQUIRE(udpBack.videoEnabled);       // M35
    REQUIRE(udpBack.videoPort == 5104);  // M35

    // Verify Replay fields.
    auto* repConn = m2.connection(QStringLiteral("rep1"));
    REQUIRE(repConn != nullptr);
    const auto& repBack = std::get<dr::ReplayConfig>(repConn->config().driverConfig);
    REQUIRE(repBack.sessionFilePath == QStringLiteral("/var/tmp/session.sfreplay"));
    REQUIRE(repBack.playbackSpeed == 0.5);
    REQUIRE(repBack.loop);
}

TEST_CASE("S4: AutoConnectCommand binary payload survives round-trip including NUL/CRLF",
          "[connection][s4][persistence][binary]") {
    TestFixture fx;
    auto& m = fx.manager();

    conn::ConnectionConfig cfg;
    cfg.id = QStringLiteral("withcmds");
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig r;
    r.sessionFilePath = QStringLiteral("/tmp/x.sfreplay");
    cfg.driverConfig = r;

    conn::AutoConnectCommand c1;
    c1.name = QStringLiteral("hello");
    // "AB\0CD\r\n" — 7 bytes including embedded NUL and CRLF.
    c1.payload = QByteArray::fromRawData("AB\0CD\r\n", 7);
    c1.payload.detach();  // own the bytes
    c1.expected = QByteArray::fromRawData("\xff\xfe\x00\x01\x0a", 5);
    c1.expected->detach();
    c1.timeout = std::chrono::milliseconds{750};
    c1.delayBefore = std::chrono::milliseconds{125};

    conn::AutoConnectCommand c2;
    c2.name = QStringLiteral("noresp");
    c2.payload = QByteArray("RESET\n");

    cfg.autoConnectCommands = {c1, c2};
    REQUIRE_FALSE(m.addConnection(cfg).isEmpty());

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("cmds.yaml"));
    REQUIRE(m.saveConfigFile(path));

    fx.resetManager();
    auto& m2 = fx.manager();
    REQUIRE(m2.loadConfigFile(path));
    auto* loaded = m2.connection(QStringLiteral("withcmds"));
    REQUIRE(loaded != nullptr);
    const auto& cmds = loaded->config().autoConnectCommands;
    REQUIRE(cmds.size() == 2);

    REQUIRE(cmds[0].name == QStringLiteral("hello"));
    REQUIRE(cmds[0].payload.size() == 7);
    REQUIRE(cmds[0].payload[2] == '\0');
    REQUIRE(cmds[0].payload[5] == '\r');
    REQUIRE(cmds[0].payload[6] == '\n');
    REQUIRE(cmds[0].expected.has_value());
    REQUIRE(cmds[0].expected->size() == 5);
    REQUIRE(static_cast<unsigned char>((*cmds[0].expected)[0]) == 0xff);
    REQUIRE(cmds[0].timeout.count() == 750);
    REQUIRE(cmds[0].delayBefore.count() == 125);

    REQUIRE(cmds[1].payload == QByteArray("RESET\n"));
    REQUIRE_FALSE(cmds[1].expected.has_value());
}

TEST_CASE("S4: load missing file returns false and leaves manager empty", "[connection][s4][persistence]") {
    TestFixture fx;
    auto& m = fx.manager();
    REQUIRE_FALSE(m.loadConfigFile(QStringLiteral("/tmp/this-file-definitely-does-not-exist.yaml")));
    REQUIRE(m.connectionCount() == 0);
}

TEST_CASE("S4: load malformed yaml returns false and leaves manager empty", "[connection][s4][persistence]") {
    TestFixture fx;
    auto& m = fx.manager();
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("bad.yaml"));
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(":[\\n,@@malformed:");
    f.close();
    REQUIRE_FALSE(m.loadConfigFile(path));
    REQUIRE(m.connectionCount() == 0);
}

TEST_CASE("S4: load with wrong schema_version returns false", "[connection][s4][persistence]") {
    TestFixture fx;
    auto& m = fx.manager();
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("v2.yaml"));
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("schema_version: 99\nconnections: []\n");
    f.close();
    REQUIRE_FALSE(m.loadConfigFile(path));
}

TEST_CASE("S4: save then load yields bit-identical file on second save", "[connection][s4][persistence][stable]") {
    TestFixture fx;
    auto& m = fx.manager();
    conn::ConnectionConfig cfg;
    cfg.id = QStringLiteral("only");
    cfg.driverType = conn::DriverType::Tcp;
    cfg.driverConfig = dr::TcpConfig{.host = QStringLiteral("a"), .port = 80};
    REQUIRE_FALSE(m.addConnection(cfg).isEmpty());

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString p1 = tmp.filePath(QStringLiteral("a.yaml"));
    const QString p2 = tmp.filePath(QStringLiteral("b.yaml"));
    REQUIRE(m.saveConfigFile(p1));

    fx.resetManager();
    auto& m2 = fx.manager();
    REQUIRE(m2.loadConfigFile(p1));
    REQUIRE(m2.saveConfigFile(p2));

    QFile f1(p1), f2(p2);
    REQUIRE(f1.open(QIODevice::ReadOnly));
    REQUIRE(f2.open(QIODevice::ReadOnly));
    REQUIRE(f1.readAll() == f2.readAll());
}

TEST_CASE("S4: empty manager round-trip writes empty 'connections' sequence", "[connection][s4][persistence]") {
    TestFixture fx;
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("empty.yaml"));
    REQUIRE(fx.manager().saveConfigFile(path));
    fx.resetManager();
    REQUIRE(fx.manager().loadConfigFile(path));
    REQUIRE(fx.manager().connectionCount() == 0);
}
