// tests/unit/replay/session_player_lifecycle_test.cpp
//
// M11 S3 — `signalforge::replay::SessionPlayer` open / close
// lifecycle. Worker thread is created at openFile but does not
// run a dispatch loop yet (S4). Tests focus on safe lifecycle
// transitions + counter readouts.
#include "decode/decoder_interface.hpp"
#include "replay/session_player.hpp"
#include "session/session_writer.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <buffer/signal_buffer_registry.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace r = signalforge::replay;
namespace s = signalforge::session;
namespace b = signalforge::buffer;
namespace d = signalforge::decoder;

namespace {

struct PlayerFixture {
    PlayerFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

struct NoopSink : public d::SignalValueSink {
    int signalCount = 0;
    int registrationCount = 0;
    void onSignal(std::chrono::steady_clock::time_point /*t*/, const QString& /*id*/,
                  const d::SignalValue& /*v*/) override {
        ++signalCount;
    }
    void onSignalsRegistered(const QString& /*driverId*/, const std::vector<d::SignalMetadata>& /*sigs*/) override {
        ++registrationCount;
    }
};

QString writeFile(QTemporaryDir& tmp, const QString& name) {
    const QString path = tmp.filePath(name);
    b::SignalBufferRegistry registry;
    s::SessionWriter writer(registry);
    d::SignalMetadata m;
    m.id = QStringLiteral("a");
    m.name = QStringLiteral("a");
    m.unit = QStringLiteral("u");
    m.type = d::SignalType::Double;
    writer.onSignalsRegistered(QStringLiteral("d1"), {m});
    REQUIRE(writer.start(path));
    const auto t0 = writer.metadata().recordingStart;
    writer.onSignal(t0 + std::chrono::microseconds(10), QStringLiteral("a"), d::SignalValue{1.0});
    writer.onSignal(t0 + std::chrono::microseconds(20), QStringLiteral("a"), d::SignalValue{2.0});
    (void)writer.stop();
    return path;
}

}  // namespace

TEST_CASE_METHOD(PlayerFixture, "M11 S3: SessionPlayer construction is safe + Idle defaults", "[replay][m11][s3]") {
    NoopSink sink;
    r::SessionPlayer player(sink);
    REQUIRE_FALSE(player.isOpen());
    REQUIRE_FALSE(player.isPlaying());
    REQUIRE(player.currentSpeed() == 1.0);
    REQUIRE(player.currentPositionNs() == 0);
    REQUIRE(player.currentRecordIndex() == 0);
    REQUIRE(player.durationNs() == 0);
    REQUIRE(player.totalRecords() == 0);
}

TEST_CASE_METHOD(PlayerFixture, "M11 S3: openFile success populates counters + announces catalog",
                 "[replay][m11][s3]") {
    const QString path = writeFile(tmp_, QStringLiteral("ok.sfreplay"));

    NoopSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));
    REQUIRE(player.isOpen());
    REQUIRE_FALSE(player.isPlaying());
    REQUIRE(player.durationNs() == 20000);  // last record at 20 µs
    REQUIRE(player.totalRecords() == 2);
    // Initial catalog has been announced exactly once.
    REQUIRE(sink.registrationCount == 1);
    // Worker hasn't started, so no signals dispatched.
    REQUIRE(sink.signalCount == 0);
}

TEST_CASE_METHOD(PlayerFixture, "M11 S3: openFile rejects nonexistent file", "[replay][m11][s3]") {
    NoopSink sink;
    r::SessionPlayer player(sink);
    REQUIRE_FALSE(player.openFile(tmp_.filePath(QStringLiteral("does-not-exist.sfreplay"))));
    REQUIRE_FALSE(player.isOpen());
}

TEST_CASE_METHOD(PlayerFixture, "M11 S3: closeFile is idempotent + resets counters", "[replay][m11][s3]") {
    const QString path = writeFile(tmp_, QStringLiteral("close.sfreplay"));

    NoopSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path));
    REQUIRE(player.totalRecords() == 2);

    player.closeFile();
    REQUIRE_FALSE(player.isOpen());
    REQUIRE(player.totalRecords() == 0);
    REQUIRE(player.durationNs() == 0);

    // Second close: safe.
    player.closeFile();
    REQUIRE_FALSE(player.isOpen());
}

TEST_CASE_METHOD(PlayerFixture, "M11 S3: opening a second file auto-closes the first", "[replay][m11][s3]") {
    const QString path1 = writeFile(tmp_, QStringLiteral("a.sfreplay"));
    const QString path2 = writeFile(tmp_, QStringLiteral("b.sfreplay"));

    NoopSink sink;
    r::SessionPlayer player(sink);
    REQUIRE(player.openFile(path1));
    REQUIRE(player.isOpen());
    REQUIRE(player.openFile(path2));
    REQUIRE(player.isOpen());
    // Both files share fixture metadata; counters consistent.
    REQUIRE(player.totalRecords() == 2);
}

TEST_CASE_METHOD(PlayerFixture, "M11 S3: destructor cleans up if file still open", "[replay][m11][s3]") {
    const QString path = writeFile(tmp_, QStringLiteral("dtor.sfreplay"));
    NoopSink sink;
    {
        r::SessionPlayer player(sink);
        REQUIRE(player.openFile(path));
        // Player goes out of scope while open; destructor must
        // close cleanly without throwing or leaving a worker
        // thread orphaned.
    }
    SUCCEED("destructor returned cleanly");
}
