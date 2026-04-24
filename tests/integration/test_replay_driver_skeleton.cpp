// tests/integration/test_replay_driver_skeleton.cpp
#include "drivers/replay_driver.hpp"
#include "frame/raw_frame.hpp"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QString>
#include <QtTest/QtTest>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

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
            static char argv0[] = "test_replay_driver_skeleton";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};
CoreAppHolder g_app;

std::filesystem::path fixturePath() {
#ifdef SIGNALFORGE_FIXTURES_DIR
    return std::filesystem::path{SIGNALFORGE_FIXTURES_DIR} / "minimal_session.sfreplay";
#else
    return std::filesystem::path{"../../../tests/integration/fixtures/minimal_session.sfreplay"};
#endif
}

bool waitForState(DriverInterface& d, DriverState desired, int ms = 2000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (d.state() != desired && std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    return d.state() == desired;
}

}  // namespace

TEST_CASE("integration replay: valid fixture opens, runs without frames, closes clean", "[integration][replay]") {
    const auto path = fixturePath();
    REQUIRE(std::filesystem::exists(path));

    ReplayConfig cfg;
    cfg.sessionFilePath = QString::fromStdString(path.string());
    ReplayDriver d{cfg};
    QSignalSpy frameSpy(&d, &DriverInterface::frameReceived);
    QSignalSpy stateSpy(&d, &DriverInterface::stateChanged);

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));

    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));

    // Run for 1 s; skeleton must emit no frames.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    REQUIRE(frameSpy.count() == 0);

    d.stop();
    REQUIRE(waitForState(d, DriverState::Open));
    d.close();
    REQUIRE(waitForState(d, DriverState::Idle));
}

TEST_CASE("integration replay: nonexistent file transitions to Error", "[integration][replay]") {
    ReplayConfig cfg;
    cfg.sessionFilePath = QStringLiteral("/tmp/does-not-exist-%1.sfreplay").arg(::getpid());
    std::filesystem::remove(cfg.sessionFilePath.toStdString());

    ReplayDriver d{cfg};
    QSignalSpy errSpy(&d, &DriverInterface::errorOccurred);
    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Error));
    REQUIRE(errSpy.count() >= 1);
    const auto err = qvariant_cast<DriverError>(errSpy.at(0).at(0));
    REQUIRE(err.code == DriverErrorCode::ResourceUnavailable);
}

TEST_CASE("integration replay: write() always NotConfigured", "[integration][replay]") {
    const auto path = fixturePath();
    ReplayConfig cfg;
    cfg.sessionFilePath = QString::fromStdString(path.string());
    ReplayDriver d{cfg};

    REQUIRE(d.open() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Open));
    REQUIRE(d.write(QByteArray{"x", 1}) == DriverErrorCode::NotConfigured);
    REQUIRE(d.start() == DriverErrorCode::Success);
    REQUIRE(waitForState(d, DriverState::Running));
    REQUIRE(d.write(QByteArray{"y", 1}) == DriverErrorCode::NotConfigured);
    d.close();
}
