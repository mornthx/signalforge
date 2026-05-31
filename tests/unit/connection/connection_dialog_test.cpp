// tests/unit/connection/connection_dialog_test.cpp
//
// S5 — ConnectionDialog modal + dynamic QStackedWidget tests.
// Forces QT_QPA_PLATFORM=offscreen so the dialog runs without
// a display.

#include "connection/connection_dialog.hpp"

#include <QApplication>
#include <QByteArray>
#include <QGroupBox>
#include <QPushButton>
#include <QStackedWidget>
#include <catch2/catch_test_macros.hpp>
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
            static char argv0[] = "connection_dialog_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QApplication> app_;
};
GuiAppHolder g_app;

}  // namespace

TEST_CASE("S5: dialog constructs cleanly with empty schema list", "[connection][s5][dialog]") {
    conn::ConnectionDialog d({});
    REQUIRE(d.stackedWidget() != nullptr);
    REQUIRE(d.stackedWidget()->count() == 4);  // serial, tcp, udp, replay
}

TEST_CASE("S5: changing driver type flips the stack page", "[connection][s5][dialog]") {
    conn::ConnectionDialog d({});
    d.setDriverType(conn::DriverType::Serial);
    REQUIRE(d.stackedWidget()->currentIndex() == 0);
    d.setDriverType(conn::DriverType::Tcp);
    REQUIRE(d.stackedWidget()->currentIndex() == 1);
    d.setDriverType(conn::DriverType::Udp);
    REQUIRE(d.stackedWidget()->currentIndex() == 2);
    d.setDriverType(conn::DriverType::Replay);
    REQUIRE(d.stackedWidget()->currentIndex() == 3);
}

TEST_CASE("S5: empty dialog is invalid (display name missing)", "[connection][s5][validation]") {
    conn::ConnectionDialog d({});
    REQUIRE_FALSE(d.isValid());
    REQUIRE_FALSE(d.okButton()->isEnabled());
}

TEST_CASE("M18 UX: new connection dialog defaults to UDP workflow", "[connection][m18][dialog]") {
    conn::ConnectionDialog d({});
    REQUIRE(d.config().driverType == conn::DriverType::Udp);
    REQUIRE(d.stackedWidget()->currentIndex() == static_cast<int>(conn::DriverType::Udp));
}

TEST_CASE("S5: setConfig + readback round-trips Serial config", "[connection][s5][roundtrip]") {
    conn::ConnectionDialog d({QStringLiteral("schemaA"), QStringLiteral("schemaB")});

    conn::ConnectionConfig in;
    in.id = QStringLiteral("ser-x");
    in.displayName = QStringLiteral("Test serial");
    in.driverType = conn::DriverType::Serial;
    dr::SerialConfig s;
    s.device = QStringLiteral("/dev/ttyUSB7");
    s.baudRate = 230400;
    s.dataBits = 7;
    s.parity = QStringLiteral("odd");
    s.stopBits = 2;
    s.flowControl = QStringLiteral("software");
    in.driverConfig = s;
    in.decoderSchemaId = QStringLiteral("schemaB");
    in.autoConnectOnStartup = true;
    d.setConfig(in);

    REQUIRE(d.isValid());
    const auto out = d.config();
    REQUIRE(out.id == QStringLiteral("ser-x"));
    REQUIRE(out.displayName == QStringLiteral("Test serial"));
    REQUIRE(out.driverType == conn::DriverType::Serial);
    REQUIRE(out.decoderSchemaId == QStringLiteral("schemaB"));
    REQUIRE(out.autoConnectOnStartup);
    const auto& sBack = std::get<dr::SerialConfig>(out.driverConfig);
    REQUIRE(sBack.device == QStringLiteral("/dev/ttyUSB7"));
    REQUIRE(sBack.baudRate == 230400);
    REQUIRE(sBack.dataBits == 7);
    REQUIRE(sBack.parity == QStringLiteral("odd"));
    REQUIRE(sBack.stopBits == 2);
    REQUIRE(sBack.flowControl == QStringLiteral("software"));
}

TEST_CASE("S5: TCP validation rejects empty host or port 0", "[connection][s5][validation]") {
    conn::ConnectionDialog d({});
    conn::ConnectionConfig in;
    in.id = QStringLiteral("tcpx");
    in.displayName = QStringLiteral("X");
    in.driverType = conn::DriverType::Tcp;
    in.driverConfig = dr::TcpConfig{.host = QString(), .port = 9999};
    d.setConfig(in);
    REQUIRE_FALSE(d.isValid());

    in.driverConfig = dr::TcpConfig{.host = QStringLiteral("h"), .port = 9999};
    d.setConfig(in);
    REQUIRE(d.isValid());
}

TEST_CASE("S5: UDP requires bind or send intent", "[connection][s5][validation]") {
    conn::ConnectionDialog d({});
    conn::ConnectionConfig in;
    in.displayName = QStringLiteral("U");
    in.driverType = conn::DriverType::Udp;
    // Default UDP config has localBindAddress="0.0.0.0", port 0,
    // empty remoteHost, port 0 — neither bind nor send intent.
    in.driverConfig = dr::UdpConfig{};
    d.setConfig(in);
    REQUIRE_FALSE(d.isValid());

    // Bind intent via local port.
    dr::UdpConfig u2;
    u2.localBindPort = 5005;
    in.driverConfig = u2;
    d.setConfig(in);
    REQUIRE(d.isValid());

    // Send intent via remote host + port.
    dr::UdpConfig u3;
    u3.remoteHost = QStringLiteral("10.0.0.1");
    u3.remotePort = 5005;
    in.driverConfig = u3;
    d.setConfig(in);
    REQUIRE(d.isValid());
}

TEST_CASE("S5: Replay validation rejects empty path or non-positive speed", "[connection][s5][validation]") {
    conn::ConnectionDialog d({});
    conn::ConnectionConfig in;
    in.displayName = QStringLiteral("R");
    in.driverType = conn::DriverType::Replay;
    dr::ReplayConfig r;
    r.sessionFilePath = QString();
    r.playbackSpeed = 1.0;
    in.driverConfig = r;
    d.setConfig(in);
    REQUIRE_FALSE(d.isValid());

    r.sessionFilePath = QStringLiteral("/some/path.sfreplay");
    in.driverConfig = r;
    d.setConfig(in);
    REQUIRE(d.isValid());
}

TEST_CASE("S5: setConfig pre-fills auto-connect commands; config() reads them back", "[connection][s5][commands]") {
    conn::ConnectionDialog d({});
    conn::ConnectionConfig in;
    in.id = QStringLiteral("c1");
    in.displayName = QStringLiteral("Cmds");
    in.driverType = conn::DriverType::Tcp;
    in.driverConfig = dr::TcpConfig{.host = QStringLiteral("h"), .port = 80};
    conn::AutoConnectCommand cmd;
    cmd.name = QStringLiteral("init");
    cmd.payload = QByteArray::fromRawData("AB\0CD", 5);
    cmd.payload.detach();
    cmd.timeout = std::chrono::milliseconds{2500};
    cmd.delayBefore = std::chrono::milliseconds{100};
    in.autoConnectCommands.push_back(cmd);
    d.setConfig(in);

    REQUIRE(d.isValid());
    const auto out = d.config();
    REQUIRE(out.autoConnectCommands.size() == 1);
    REQUIRE(out.autoConnectCommands[0].name == QStringLiteral("init"));
    REQUIRE(out.autoConnectCommands[0].payload.size() == 5);
    REQUIRE(out.autoConnectCommands[0].payload[2] == '\0');
    REQUIRE(out.autoConnectCommands[0].timeout.count() == 2500);
    REQUIRE(out.autoConnectCommands[0].delayBefore.count() == 100);
}

TEST_CASE("M18 UX: advanced auto-connect commands are collapsed by default", "[connection][m18][dialog]") {
    conn::ConnectionDialog d({});
    REQUIRE(d.advancedCommandsGroup() != nullptr);
    REQUIRE(d.advancedCommandsGroup()->isCheckable());
    REQUIRE_FALSE(d.advancedCommandsGroup()->isChecked());
    REQUIRE(d.advancedCommandsBody() != nullptr);
    REQUIRE(d.advancedCommandsBody()->isHidden());
}

TEST_CASE("M18 UX: advanced auto-connect commands expand when config already has commands",
          "[connection][m18][dialog]") {
    conn::ConnectionDialog d({});
    conn::ConnectionConfig in;
    in.id = QStringLiteral("c1");
    in.displayName = QStringLiteral("Cmds");
    in.driverType = conn::DriverType::Tcp;
    in.driverConfig = dr::TcpConfig{.host = QStringLiteral("h"), .port = 80};
    conn::AutoConnectCommand cmd;
    cmd.name = QStringLiteral("init");
    cmd.payload = QByteArray::fromHex("4142");
    in.autoConnectCommands.push_back(cmd);

    d.setConfig(in);

    REQUIRE(d.advancedCommandsGroup() != nullptr);
    REQUIRE(d.advancedCommandsGroup()->isChecked());
    REQUIRE_FALSE(d.advancedCommandsBody()->isHidden());
}

TEST_CASE("M18 UX: connection dialog exposes validation button, not a fake test connection action",
          "[connection][m18][dialog]") {
    conn::ConnectionDialog d({});
    REQUIRE(d.validateButton() != nullptr);
    REQUIRE(d.validateButton()->text() == QStringLiteral("Validate fields"));
}
