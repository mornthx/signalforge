// tests/integration/test_connection_lifecycle_full_stack.cpp
//
// S10 — full-stack lifecycle integration test exercising the
// MainWindow-equivalent topology: PipelineManager + DecoderRegistrar
// + ConnectionManager. Focuses on the integration points unit
// tests don't cover (manager wiring + dialog + pipeline pumping).

#include "connection/connection_dialog.hpp"
#include "connection/connection_list_widget.hpp"
#include "connection/connection_manager.hpp"
#include "connection/connection_status_widget.hpp"
#include "decode/decoder_registrar.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QtTest>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
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
            static char argv0[] = "test_connection_lifecycle_full_stack";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QApplication> app_;
};
GuiAppHolder g_app;

class FullStackFixture {
public:
    FullStackFixture()
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

void writeReplayFile(const QString& path, const std::vector<std::pair<std::uint64_t, QByteArray>>& frames) {
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QByteArray header(16, '\0');
    std::memcpy(header.data(), "SFREPLAY", 8);
    f.write(header);
    for (const auto& [nanos, payload] : frames) {
        std::uint64_t n = nanos;
        std::uint32_t len = static_cast<std::uint32_t>(payload.size());
        f.write(reinterpret_cast<const char*>(&n), 8);
        f.write(reinterpret_cast<const char*>(&len), 4);
        f.write(payload);
    }
    f.close();
}

conn::ConnectionConfig makeReplayCfg(const QString& id, const QString& path) {
    conn::ConnectionConfig cfg;
    cfg.id = id;
    cfg.displayName = QStringLiteral("Replay ") + id;
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig rc;
    rc.sessionFilePath = path;
    cfg.driverConfig = rc;
    return cfg;
}

}  // namespace

TEST_CASE("S10 integration: full lifecycle — add, connect, disconnect, remove", "[integration][s10][lifecycle]") {
    FullStackFixture fx;
    auto& m = fx.manager();

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString session = tmp.filePath(QStringLiteral("life.sfreplay"));
    writeReplayFile(session, {{0, QByteArray("hi")}});

    QSignalSpy addedSpy(&m, &conn::ConnectionManager::connectionAdded);
    QSignalSpy stateSpy(&m, &conn::ConnectionManager::connectionStateChanged);
    QSignalSpy removedSpy(&m, &conn::ConnectionManager::connectionRemoved);

    const QString id = m.addConnection(makeReplayCfg(QStringLiteral("life-1"), session));
    REQUIRE_FALSE(id.isEmpty());
    REQUIRE(addedSpy.count() == 1);

    REQUIRE(m.connectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Connected));
    REQUIRE(stateSpy.count() >= 2);

    REQUIRE(m.disconnectConnection(id));
    REQUIRE(waitForState(*m.connection(id), conn::Connection::State::Idle));

    REQUIRE(m.removeConnection(id));
    REQUIRE(removedSpy.count() == 1);
}

TEST_CASE("S10 integration: persistence round-trip via app default config path", "[integration][s10][persistence]") {
    FullStackFixture fx;

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString session = tmp.filePath(QStringLiteral("p.sfreplay"));
    writeReplayFile(session, {{0, QByteArray("z")}});
    const QString configPath = tmp.filePath(QStringLiteral("connections.yaml"));

    REQUIRE_FALSE(fx.manager().addConnection(makeReplayCfg(QStringLiteral("p-1"), session)).isEmpty());
    REQUIRE(fx.manager().saveConfigFile(configPath));
    // Build a fresh fixture and verify load reproduces the entry.
    FullStackFixture fx2;
    REQUIRE(fx2.manager().loadConfigFile(configPath));
    REQUIRE(fx2.manager().connectionCount() == 1);
    REQUIRE(fx2.manager().connection(QStringLiteral("p-1")) != nullptr);
}

TEST_CASE("S10 integration: dialog rejects invalid configs across all driver types", "[integration][s10][dialog]") {
    conn::ConnectionDialog dlg({QStringLiteral("schema1")});

    {
        // TCP: empty host invalid.
        conn::ConnectionConfig in;
        in.displayName = QStringLiteral("X");
        in.driverType = conn::DriverType::Tcp;
        in.driverConfig = dr::TcpConfig{.host = QString(), .port = 80};
        dlg.setConfig(in);
        REQUIRE_FALSE(dlg.isValid());
    }
    {
        // Serial: empty device invalid.
        conn::ConnectionConfig in;
        in.displayName = QStringLiteral("X");
        in.driverType = conn::DriverType::Serial;
        dr::SerialConfig s;
        s.device = QString();
        in.driverConfig = s;
        dlg.setConfig(in);
        REQUIRE_FALSE(dlg.isValid());
    }
    {
        // Replay: empty path invalid.
        conn::ConnectionConfig in;
        in.displayName = QStringLiteral("X");
        in.driverType = conn::DriverType::Replay;
        dr::ReplayConfig r;
        r.sessionFilePath = QString();
        in.driverConfig = r;
        dlg.setConfig(in);
        REQUIRE_FALSE(dlg.isValid());
    }
}

TEST_CASE("S10 integration: list widget mirrors manager across full lifecycle", "[integration][s10][widgets]") {
    FullStackFixture fx;

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString session = tmp.filePath(QStringLiteral("w.sfreplay"));
    writeReplayFile(session, {{0, QByteArray("X")}});

    conn::ConnectionListWidget list(&fx.manager());
    conn::ConnectionStatusWidget status(&fx.manager());

    REQUIRE(list.listWidget()->count() == 0);
    REQUIRE(status.label()->text() == QStringLiteral("0/0 connected"));

    const QString id = fx.manager().addConnection(makeReplayCfg(QStringLiteral("w-1"), session));
    REQUIRE(list.listWidget()->count() == 1);
    REQUIRE(status.label()->text() == QStringLiteral("0/1 connected"));

    REQUIRE(fx.manager().connectConnection(id));
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Connected));
    REQUIRE(status.label()->text() == QStringLiteral("1/1 connected"));

    REQUIRE(fx.manager().disconnectConnection(id));
    REQUIRE(waitForState(*fx.manager().connection(id), conn::Connection::State::Idle));
    REQUIRE(fx.manager().removeConnection(id));
    REQUIRE(list.listWidget()->count() == 0);
}
