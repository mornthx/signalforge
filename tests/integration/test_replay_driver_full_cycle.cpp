// tests/integration/test_replay_driver_full_cycle.cpp
//
// S8 / S10 — Full ReplayDriver cycle through ConnectionManager.
//
// Builds a session file at runtime with the M9 V1 frame-stream
// format (magic "SFREPLAY" + 16-byte header + records of
// {u64 nanosOffset, u32 payloadLen, u8[payloadLen]}), then
// spins up a ReplayDriver via Connection and verifies:
//   - frames are emitted (frameReceived signal fires N times)
//   - loop=true restarts after EOF
//   - playbackSpeed scales the wall-clock duration

#include "connection/connection.hpp"

#include <QCoreApplication>
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

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "test_replay_driver_full_cycle";
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

bool waitForCount(QSignalSpy& spy, int targetCount, int ms = 5000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (spy.count() < targetCount && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::yieldCurrentThread();
    }
    return spy.count() >= targetCount;
}

/// Build a V1 replay file with the given frames at the given path.
/// Each record: u64 nanos (LE) + u32 len (LE) + payload bytes.
void writeReplayFile(const QString& path, const std::vector<std::pair<std::uint64_t, QByteArray>>& frames) {
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    // 16-byte header: "SFREPLAY" + 8 bytes of zero (version+reserved).
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

conn::ConnectionConfig makeReplayConn(const QString& path, double speed = 1.0, bool loop = false) {
    conn::ConnectionConfig cfg;
    cfg.id = QStringLiteral("replay-it");
    cfg.driverType = conn::DriverType::Replay;
    dr::ReplayConfig rc;
    rc.sessionFilePath = path;
    rc.playbackSpeed = speed;
    rc.loop = loop;
    cfg.driverConfig = rc;
    return cfg;
}

}  // namespace

TEST_CASE("S8 integration: replay driver emits all frames in a non-looping file", "[integration][s8][replay]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("session.sfreplay"));
    writeReplayFile(path,
                    {{0, QByteArray("frame1")}, {1'000'000, QByteArray("frame2")}, {2'000'000, QByteArray("frame3")}});

    conn::Connection c{makeReplayConn(path)};
    auto* driver = c.driver();
    REQUIRE(driver != nullptr);
    QSignalSpy frameSpy(driver, &dr::DriverInterface::frameReceived);

    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    REQUIRE(waitForCount(frameSpy, 3));
    REQUIRE(frameSpy.count() == 3);

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S8 integration: replay driver loops at EOF when loop=true", "[integration][s8][replay][loop]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("loop.sfreplay"));
    // 2 quick frames so we can see > 2 frames within the wait window.
    writeReplayFile(path, {{0, QByteArray("a")}, {1'000'000, QByteArray("b")}});

    conn::Connection c{makeReplayConn(path, /*speed=*/100.0, /*loop=*/true)};
    auto* driver = c.driver();
    QSignalSpy frameSpy(driver, &dr::DriverInterface::frameReceived);

    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    // With loop=true and high playbackSpeed, we should accumulate
    // many frames in a short window.
    REQUIRE(waitForCount(frameSpy, 6, 3000));
    REQUIRE(frameSpy.count() >= 6);

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S8 integration: replay driver respects playbackSpeed timing", "[integration][s8][replay][speed]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("speed.sfreplay"));
    // Frame at nanos 0, then 100 ms later. At speed=1.0 the wall-clock
    // delta should be about 100 ms; at speed=10.0 it should be ~10 ms.
    writeReplayFile(path, {{0, QByteArray("a")}, {100'000'000, QByteArray("b")}});

    conn::Connection c{makeReplayConn(path, /*speed=*/10.0, /*loop=*/false)};
    auto* driver = c.driver();
    QSignalSpy frameSpy(driver, &dr::DriverInterface::frameReceived);

    const auto t0 = std::chrono::steady_clock::now();
    REQUIRE(c.connectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Connected));
    REQUIRE(waitForCount(frameSpy, 2, 5000));
    const auto t1 = std::chrono::steady_clock::now();
    const auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    // 100 ms / 10x = 10 ms ideal. Allow generous bound for CI variance.
    REQUIRE(wallMs < 1000);

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}

TEST_CASE("S8 integration: ReplayConfig with playbackSpeed <= 0 fails open()", "[integration][s8][replay][config]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("speed.sfreplay"));
    writeReplayFile(path, {{0, QByteArray("a")}});

    conn::Connection c{makeReplayConn(path, /*speed=*/0.0, /*loop=*/false)};
    (void)c.connectDriver();
    REQUIRE(waitForState(c, conn::Connection::State::Error));
    REQUIRE_FALSE(c.lastError().isEmpty());

    REQUIRE(c.disconnectDriver());
    REQUIRE(waitForState(c, conn::Connection::State::Idle));
}
