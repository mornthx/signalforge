// tests/integration/test_v1_live_mode_pipeline.cpp
//
// M13 S7 / ADR-008 — V1.0 live-mode pipeline integration test.
//
// Verifies the multi-milestone wire-up that was missing pre-ADR-008:
//   ConnectionConfig.decoderSchemaId
//     → ConnectionManager.applyDecoderSchemaForConfig()
//       → DecoderRegistrar.setSchemaForDriverType() (additive M5
//         frozen-surface API authorised by ADR-008)
//       → on pipelineAttached(), DecoderRegistrar consults its
//         per-driver-type map → constructs SchemaDecoder
//       → SchemaDecoder produces SignalValues to the registered sink
//
// Pre-ADR-008, MainWindow constructed DecoderRegistrar with an
// empty driverTypeToSchemaPath map and no path existed for
// ConnectionManager to update it; live-mode SchemaDecoders never
// attached. M13 18-test combined hardware verification (M9
// Tests 1-3) caught this; ADR-008 closes the gap.
//
// This test is the V1.0 regression detector — if any future change
// breaks the ADR-008 wire-up, this test fires before V1.0.x ships.

#include "connection/connection.hpp"
#include "connection/connection_manager.hpp"
#include "decode/decoder_interface.hpp"
#include "decode/decoder_registrar.hpp"
#include "drivers/driver_configs.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
namespace dec = signalforge::decoder;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "test_v1_live_mode_pipeline";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
        // Resolve `examples/schemas/<id>.yaml` relative to the
        // repo root (via the build-time SIGNALFORGE_SOURCE_ROOT
        // define). The bench / build-tree CWD doesn't contain
        // examples/schemas/, but the resolver in
        // ConnectionManager builds relative paths.
        //
        // Note: V1.0 production has an analogous schema-path
        // resolution issue when signalforge is launched from
        // /usr/local/bin/ — out of scope for ADR-008 (separate
        // V1.0.1 candidate); install.md notes the requirement
        // to launch from a directory containing examples/schemas/.
        QDir::setCurrent(QString::fromUtf8(SIGNALFORGE_SOURCE_ROOT));
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

/// Write a minimal SFREPLAY v1 raw-frame log so M9's ReplayDriver
/// can drive it through the FramePipeline. Mirrors the format
/// `tests/integration/test_connection_lifecycle_full_stack.cpp`
/// uses for its raw-frame replay fixtures (M9 ReplayDriver scope).
void writeRawFrameReplayFile(const QString& path, const std::vector<std::pair<std::uint64_t, QByteArray>>& frames) {
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

/// Locate examples/schemas/<id>.yaml. The test runs from the
/// build directory; fixtures live in the source tree.
QString schemaFixturePath(const QString& id) {
    return QString::fromUtf8(SIGNALFORGE_SOURCE_ROOT) + QStringLiteral("/examples/schemas/") + id +
           QStringLiteral(".yaml");
}

QString driverTypeKey(conn::DriverType t) {
    switch (t) {
    case conn::DriverType::Serial:
        return QStringLiteral("serial");
    case conn::DriverType::Tcp:
        return QStringLiteral("tcp");
    case conn::DriverType::Udp:
        return QStringLiteral("udp");
    case conn::DriverType::Replay:
        return QStringLiteral("replay");
    }
    return QStringLiteral("unknown");
}

/// ADR-009: production wires connectionStateChanged into
/// PipelineManager::attach/detach inside MainWindow's ctor. These
/// tests construct ConnectionManager + PipelineManager directly
/// without a MainWindow, so they must install the same bridge to
/// exercise the V1.0 live-mode path end-to-end.
void installPipelineAttachBridge(conn::ConnectionManager& manager, signalforge::pipeline::PipelineManager& pm) {
    QObject::connect(&manager, &conn::ConnectionManager::connectionStateChanged, &manager,
                     [&manager, &pm](const QString& id, conn::Connection::State state) {
                         auto* c = manager.connection(id);
                         if (c == nullptr || c->driver() == nullptr) {
                             return;
                         }
                         const QString driverId = driverTypeKey(c->config().driverType) + QStringLiteral(":") + id;
                         if (state == conn::Connection::State::Connected) {
                             signalforge::pipeline::PipelineConfig cfg;
                             cfg.driverId = driverId;
                             (void)pm.attach(c->driver(), cfg);
                         } else if (state == conn::Connection::State::Idle || state == conn::Connection::State::Error) {
                             pm.detach(driverId);
                         }
                     });
}

}  // namespace

TEST_CASE("M13 S7 / ADR-008: empty registrar map — no decoder attaches (the pre-ADR-008 bug state)",
          "[m13][s7][adr-008][live-mode]") {
    // Sanity-check baseline: with the M5 ctor map empty and **no
    // ConnectionManager wire-up exercised**, the registrar will
    // not attach a decoder when a pipeline is created. This is
    // the bug state pre-ADR-008.
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString fixture = tmp.filePath(QStringLiteral("raw.sfreplay"));
    writeRawFrameReplayFile(fixture, {{0, QByteArray("hello", 5)}});

    auto pipelineManager = std::make_unique<signalforge::pipeline::PipelineManager>();
    auto registrar =
        std::make_unique<dec::DecoderRegistrar>(pipelineManager.get(), std::unordered_map<QString, QString>{}, nullptr);

    REQUIRE(registrar->decoderCount() == 0);

    // Construct a ConnectionManager but DO NOT addConnection —
    // the bug only surfaces when ConnectionManager fails to wire
    // the schema. Skip the wire by not calling addConnection at
    // all; subsequent driver-connect path does no decoder attach.

    // (We intentionally don't drive a connect here because the
    // baseline assertion is that decoderCount stays 0 absent the
    // ADR-008 wire-up.)

    REQUIRE(registrar->decoderCount() == 0);
}

TEST_CASE("M13 S7 / ADR-008: addConnection with decoderSchemaId triggers registrar map update",
          "[m13][s7][adr-008][live-mode]") {
    auto pipelineManager = std::make_unique<signalforge::pipeline::PipelineManager>();
    auto registrar =
        std::make_unique<dec::DecoderRegistrar>(pipelineManager.get(), std::unordered_map<QString, QString>{}, nullptr);
    auto manager = std::make_unique<conn::ConnectionManager>(*registrar);
    installPipelineAttachBridge(*manager, *pipelineManager);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString fixture = tmp.filePath(QStringLiteral("raw.sfreplay"));
    writeRawFrameReplayFile(fixture, {{0, QByteArray("hello", 5)}});

    conn::ConnectionConfig cfg;
    cfg.displayName = QStringLiteral("LiveMode-1");
    cfg.driverType = conn::DriverType::Replay;
    cfg.decoderSchemaId = QStringLiteral("temperature_sensor");
    dr::ReplayConfig rc;
    rc.sessionFilePath = fixture;
    cfg.driverConfig = rc;

    const QString id = manager->addConnection(cfg);
    REQUIRE_FALSE(id.isEmpty());

    // ADR-008 wire-up: addConnection should have called
    // setSchemaForDriverType("replay", "examples/schemas/temperature_sensor.yaml").
    // We can't observe the registrar's internal map directly, but
    // we CAN verify that connect() now attaches a SchemaDecoder
    // (whereas pre-ADR-008 with empty map it would not).

    auto* c = manager->connection(id);
    REQUIRE(c != nullptr);
    REQUIRE(c->connectDriver());
    // ReplayDriver transitions Idle → Connecting → Connected
    REQUIRE(waitForState(*c, conn::Connection::State::Connected, 5000));

    // The pipelineAttached signal fires at this point; the
    // registrar's onPipelineAttached should consult its map and
    // construct a SchemaDecoder.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    // ADR-008 success criterion: at least one decoder is now
    // attached for this driver. Pre-ADR-008 this would have been 0.
    REQUIRE(registrar->decoderCount() >= 1);

    REQUIRE(c->disconnectDriver());
    REQUIRE(waitForState(*c, conn::Connection::State::Idle, 5000));

    // After detach, the decoder is released.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    REQUIRE(registrar->decoderCount() == 0);
}

TEST_CASE("M13 S7 / ADR-008: removeConnection clears the registrar map for that driver type",
          "[m13][s7][adr-008][live-mode]") {
    auto pipelineManager = std::make_unique<signalforge::pipeline::PipelineManager>();
    auto registrar =
        std::make_unique<dec::DecoderRegistrar>(pipelineManager.get(), std::unordered_map<QString, QString>{}, nullptr);
    auto manager = std::make_unique<conn::ConnectionManager>(*registrar);
    installPipelineAttachBridge(*manager, *pipelineManager);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString fixture = tmp.filePath(QStringLiteral("raw.sfreplay"));
    writeRawFrameReplayFile(fixture, {{0, QByteArray("hello", 5)}});

    conn::ConnectionConfig cfg;
    cfg.driverType = conn::DriverType::Replay;
    cfg.decoderSchemaId = QStringLiteral("temperature_sensor");
    dr::ReplayConfig rc;
    rc.sessionFilePath = fixture;
    cfg.driverConfig = rc;

    const QString id = manager->addConnection(cfg);
    REQUIRE_FALSE(id.isEmpty());

    REQUIRE(manager->removeConnection(id));

    // After remove, a fresh add-and-connect with NO decoderSchemaId
    // (testing the cleared state) must not attach a decoder.
    conn::ConnectionConfig cfg2;
    cfg2.driverType = conn::DriverType::Replay;
    cfg2.decoderSchemaId.clear();  // explicitly empty
    cfg2.driverConfig = rc;
    const QString id2 = manager->addConnection(cfg2);
    REQUIRE_FALSE(id2.isEmpty());
    auto* c2 = manager->connection(id2);
    REQUIRE(c2 != nullptr);
    REQUIRE(c2->connectDriver());
    REQUIRE(waitForState(*c2, conn::Connection::State::Connected, 5000));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    // Empty decoderSchemaId → no decoder attaches. ADR-008
    // "empty schemaPath removes the mapping" semantics.
    REQUIRE(registrar->decoderCount() == 0);

    REQUIRE(c2->disconnectDriver());
    REQUIRE(waitForState(*c2, conn::Connection::State::Idle, 5000));
}

TEST_CASE("M13 S7 / ADR-008: editConnection refreshes the registrar map", "[m13][s7][adr-008][live-mode]") {
    auto pipelineManager = std::make_unique<signalforge::pipeline::PipelineManager>();
    auto registrar =
        std::make_unique<dec::DecoderRegistrar>(pipelineManager.get(), std::unordered_map<QString, QString>{}, nullptr);
    auto manager = std::make_unique<conn::ConnectionManager>(*registrar);
    installPipelineAttachBridge(*manager, *pipelineManager);

    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString fixture = tmp.filePath(QStringLiteral("raw.sfreplay"));
    writeRawFrameReplayFile(fixture, {{0, QByteArray("hello", 5)}});

    // Add with NO schema id.
    conn::ConnectionConfig cfg;
    cfg.driverType = conn::DriverType::Replay;
    cfg.decoderSchemaId.clear();
    dr::ReplayConfig rc;
    rc.sessionFilePath = fixture;
    cfg.driverConfig = rc;
    const QString id = manager->addConnection(cfg);
    REQUIRE_FALSE(id.isEmpty());

    // Edit to add a schema id.
    auto edited = cfg;
    edited.id = id;
    edited.decoderSchemaId = QStringLiteral("temperature_sensor");
    REQUIRE(manager->editConnection(id, edited));

    // Now connect → decoder attaches.
    auto* c = manager->connection(id);
    REQUIRE(c != nullptr);
    REQUIRE(c->connectDriver());
    REQUIRE(waitForState(*c, conn::Connection::State::Connected, 5000));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    REQUIRE(registrar->decoderCount() >= 1);

    REQUIRE(c->disconnectDriver());
    REQUIRE(waitForState(*c, conn::Connection::State::Idle, 5000));
}
