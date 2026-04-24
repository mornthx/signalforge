// tests/unit/drivers/replay_driver_test.cpp
#include "drivers/replay_driver.hpp"
#include "frame/raw_frame.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest/QtTest>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;
using signalforge::drivers::ReplayConfig;
using signalforge::drivers::ReplayDriver;

namespace {

class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "replay_driver_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};

CoreAppHolder g_app;

std::filesystem::path makeFixtureFile(const std::string& suffix, std::size_t headerBytes, bool allZero = false) {
    auto path = std::filesystem::temp_directory_path() /
                ("sf_replay_fixture_" + suffix + "_" + std::to_string(::getpid()) + ".sfreplay");
    std::filesystem::remove(path);
    std::ofstream f(path, std::ios::binary);
    std::string data(headerBytes, allZero ? '\0' : '\xAB');
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    f.close();
    return path;
}

bool waitForState(DriverInterface& d, DriverState desired, int ms = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return d.state() == desired;
}

}  // namespace

TEST_CASE("ReplayDriver: default state is Idle", "[drivers][replay_driver]") {
    ReplayConfig cfg;
    cfg.sessionFilePath = QStringLiteral("/tmp/does-not-exist.sfreplay");
    ReplayDriver d{cfg};
    REQUIRE(d.state() == DriverState::Idle);
    REQUIRE(d.health() == DriverErrorCode::Success);
}

TEST_CASE("ReplayDriver: empty sessionFilePath returns ConfigInvalid", "[drivers][replay_driver]") {
    ReplayConfig cfg;  // sessionFilePath default-empty
    ReplayDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::ConfigInvalid);
    REQUIRE(d.state() == DriverState::Idle);  // no transition on sync failure
}

TEST_CASE("ReplayDriver: nonexistent file → ResourceUnavailable (async)", "[drivers][replay_driver]") {
    ReplayConfig cfg;
    cfg.sessionFilePath = QStringLiteral("/tmp/signalforge_nope_%1.sfreplay").arg(::getpid());
    std::filesystem::remove(cfg.sessionFilePath.toStdString());

    ReplayDriver d{cfg};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);

    REQUIRE(d.open() == DriverErrorCode::Success);  // accepted for async processing
    REQUIRE(waitForState(d, DriverState::Error));

    REQUIRE(errSpy.count() >= 1);
    const auto err = qvariant_cast<DriverError>(errSpy.at(0).at(0));
    REQUIRE(err.code == DriverErrorCode::ResourceUnavailable);
    REQUIRE(d.health() == DriverErrorCode::ResourceUnavailable);
}

TEST_CASE("ReplayDriver: empty-header file → ProtocolFailure", "[drivers][replay_driver]") {
    const auto path = makeFixtureFile("emptyheader", 16, /*allZero=*/true);
    ReplayConfig cfg;
    cfg.sessionFilePath = QString::fromStdString(path.string());
    ReplayDriver d{cfg};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Error));
    REQUIRE(errSpy.count() >= 1);
    const auto err = qvariant_cast<DriverError>(errSpy.at(0).at(0));
    REQUIRE(err.code == DriverErrorCode::ProtocolFailure);

    std::filesystem::remove(path);
}

TEST_CASE("ReplayDriver: short file (< 16 bytes) → ProtocolFailure", "[drivers][replay_driver]") {
    const auto path = makeFixtureFile("short", 5);
    ReplayConfig cfg;
    cfg.sessionFilePath = QString::fromStdString(path.string());
    ReplayDriver d{cfg};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Error));
    REQUIRE(errSpy.count() >= 1);
    const auto err = qvariant_cast<DriverError>(errSpy.at(0).at(0));
    REQUIRE(err.code == DriverErrorCode::ProtocolFailure);

    std::filesystem::remove(path);
}

TEST_CASE("ReplayDriver: valid header → open succeeds, stays Running without frames", "[drivers][replay_driver]") {
    const auto path = makeFixtureFile("valid", 16);
    ReplayConfig cfg;
    cfg.sessionFilePath = QString::fromStdString(path.string());
    ReplayDriver d{cfg};
    QSignalSpy frameSpy(&d, &DriverInterface::frameReceived);

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));

    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));

    // Sit in Running for a moment; skeleton must emit zero frames.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    REQUIRE(frameSpy.count() == 0);

    d.stop();
    REQUIRE(waitForState(d, DriverState::Open));

    d.close();
    REQUIRE(waitForState(d, DriverState::Idle));

    std::filesystem::remove(path);
}

TEST_CASE("ReplayDriver: write() always returns NotConfigured", "[drivers][replay_driver]") {
    const auto path = makeFixtureFile("valid2", 16);
    ReplayConfig cfg;
    cfg.sessionFilePath = QString::fromStdString(path.string());
    ReplayDriver d{cfg};

    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);  // Idle

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);  // Open

    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);  // Running (read-only)

    d.close();
    std::filesystem::remove(path);
}

TEST_CASE("ReplayDriver: close()/stop() are idempotent on Idle", "[drivers][replay_driver]") {
    ReplayConfig cfg;
    cfg.sessionFilePath = QStringLiteral("/tmp/unused.sfreplay");
    ReplayDriver d{cfg};
    QSignalSpy spy(&d, &DriverInterface::stateChanged);
    d.close();  // Idle → Idle, no-op
    d.stop();   // not Running, no-op
    REQUIRE(spy.count() == 0);
    REQUIRE(d.state() == DriverState::Idle);
}

TEST_CASE("ReplayDriver: statistics() snapshot always has zero counters", "[drivers][replay_driver]") {
    const auto path = makeFixtureFile("stats", 16);
    ReplayConfig cfg;
    cfg.sessionFilePath = QString::fromStdString(path.string());
    ReplayDriver d{cfg};
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));

    const auto s = d.statistics();
    REQUIRE(s.rx.framesTotal == 0);
    REQUIRE(s.rx.bytesTotal == 0);
    REQUIRE(s.tx.framesTotal == 0);

    d.close();
    std::filesystem::remove(path);
}

TEST_CASE("ReplayDriver: config() accessor returns stored config", "[drivers][replay_driver]") {
    ReplayConfig cfg;
    cfg.sessionFilePath = QStringLiteral("/tmp/abc.sfreplay");
    ReplayDriver d{cfg};
    REQUIRE(d.config().sessionFilePath == "/tmp/abc.sfreplay");
}
