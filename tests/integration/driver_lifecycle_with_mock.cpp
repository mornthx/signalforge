// tests/integration/driver_lifecycle_with_mock.cpp
#include "drivers/driver_interface.hpp"
#include "frame/raw_frame.hpp"
#include "tests/mocks/mock_driver.hpp"

#include <QByteArray>
#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QSignalSpy>
#include <QThread>
#include <QVariant>
#include <QtTest/QtTest>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

using signalforge::drivers::DriverError;
using signalforge::drivers::DriverErrorCode;
using signalforge::drivers::DriverInterface;
using signalforge::drivers::DriverState;
using signalforge::test::MockDriver;

namespace {

// Launch a QCoreApplication once per process. Catch2 shares the process
// across tests, so this is a one-time setup.
class CoreAppHolder {
public:
    CoreAppHolder() {
        signalforge::frame::registerMetatypes();
        signalforge::drivers::registerMetatypes();
        if (QCoreApplication::instance() == nullptr) {
            static int argc = 1;
            static char argv0[] = "integration_test";
            static char* argv[] = {argv0, nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }

private:
    std::unique_ptr<QCoreApplication> app_;
};

CoreAppHolder g_app;

}  // namespace

TEST_CASE("integration: cross-thread driver lifecycle with QueuedConnection frames", "[integration][driver]") {
    // Construction on main thread.
    auto driver = std::make_unique<MockDriver>();
    REQUIRE(driver->thread() == QThread::currentThread());
    REQUIRE(driver->state() == DriverState::Idle);

    // Create dedicated IO QThread; move driver onto it.
    auto ioThread = std::make_unique<QThread>();
    ioThread->setObjectName("integration-io");
    driver->moveToThread(ioThread.get());
    ioThread->start();
    REQUIRE(driver->thread() == ioThread.get());

    // Main-thread consumer: captures frames delivered via Qt::QueuedConnection.
    std::atomic<int> receivedCount{0};
    QObject consumer;
    QObject::connect(
        driver.get(), &DriverInterface::frameReceived, &consumer,
        [&receivedCount](const signalforge::frame::RawFrame& /*frame*/) {
            REQUIRE(QThread::currentThread() == QCoreApplication::instance()->thread());
            receivedCount.fetch_add(1, std::memory_order_relaxed);
        },
        Qt::QueuedConnection);

    QSignalSpy stateSpy(driver.get(), &DriverInterface::stateChanged);

    // open() on the IO thread via queued invocation. Returns asynchronously;
    // we assert the resulting state via signal capture.
    QMetaObject::invokeMethod(driver.get(), [&] { (void)driver->open(); }, Qt::QueuedConnection);
    QTest::qWait(50);
    REQUIRE(driver->state() == DriverState::Open);

    // start() on the IO thread.
    QMetaObject::invokeMethod(driver.get(), [&] { (void)driver->start(); }, Qt::QueuedConnection);
    QTest::qWait(50);
    REQUIRE(driver->state() == DriverState::Running);

    // Inject 1000 frames from the IO thread via queued invocations.
    constexpr int kFrameCount = 1000;
    for (int i = 0; i < kFrameCount; ++i) {
        QMetaObject::invokeMethod(
            driver.get(), [&, i] { driver->emitFrame(QByteArray("f", 1)); }, Qt::QueuedConnection);
    }

    // Drain both event loops until all frames have been delivered or a
    // generous timeout fires.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (receivedCount.load(std::memory_order_relaxed) < kFrameCount && std::chrono::steady_clock::now() < deadline) {
        QTest::qWait(10);
    }

    REQUIRE(receivedCount.load() == kFrameCount);

    // stop() + close().
    QMetaObject::invokeMethod(driver.get(), [&] { driver->stop(); }, Qt::QueuedConnection);
    QTest::qWait(50);
    REQUIRE(driver->state() == DriverState::Open);

    QMetaObject::invokeMethod(driver.get(), [&] { driver->close(); }, Qt::QueuedConnection);
    QTest::qWait(50);
    REQUIRE(driver->state() == DriverState::Idle);

    // Tear down the IO thread cleanly.
    ioThread->quit();
    ioThread->wait();

    // Driver is back on the IO thread object's (now-stopped) affinity; delete
    // on main after pulling ownership back is unsafe without moveToThread
    // back. Destroy via deleteLater or let unique_ptr run on main — the
    // thread is stopped so delete on main is safe for our synchronous mock.
}

TEST_CASE("integration: signal emission thread affinity", "[integration][driver]") {
    auto driver = std::make_unique<MockDriver>();
    auto ioThread = std::make_unique<QThread>();
    ioThread->setObjectName("integration-affinity");
    driver->moveToThread(ioThread.get());
    ioThread->start();

    // Capture the thread on which frameReceived fires using a
    // Qt::DirectConnection so the slot runs on the emitter's thread.
    std::atomic<QThread*> emitterThread{nullptr};
    QObject probe;
    QObject::connect(
        driver.get(), &DriverInterface::frameReceived, &probe,
        [&emitterThread](const signalforge::frame::RawFrame&) {
            emitterThread.store(QThread::currentThread(), std::memory_order_relaxed);
        },
        Qt::DirectConnection);

    QMetaObject::invokeMethod(driver.get(), [&] { (void)driver->open(); }, Qt::QueuedConnection);
    QTest::qWait(50);
    QMetaObject::invokeMethod(driver.get(), [&] { (void)driver->start(); }, Qt::QueuedConnection);
    QTest::qWait(50);
    QMetaObject::invokeMethod(driver.get(), [&] { driver->emitFrame(QByteArray("hi", 2)); }, Qt::QueuedConnection);
    QTest::qWait(100);

    REQUIRE(emitterThread.load() == ioThread.get());
    REQUIRE(emitterThread.load() != QCoreApplication::instance()->thread());

    QMetaObject::invokeMethod(driver.get(), [&] { driver->close(); }, Qt::QueuedConnection);
    QTest::qWait(50);
    ioThread->quit();
    ioThread->wait();
}
