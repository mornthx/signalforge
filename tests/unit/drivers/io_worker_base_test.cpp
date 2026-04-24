// tests/unit/drivers/io_worker_base_test.cpp
#include "drivers/io_worker_base.hpp"

#include <QMetaObject>
#include <QThread>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using signalforge::drivers::IoWorkerBase;

namespace {

// Minimal concrete subclass for testing.
class StubWorker : public IoWorkerBase {
public:
    explicit StubWorker(const QString& name) : IoWorkerBase(name) {}

    std::atomic<bool> started{false};
    std::atomic<Qt::HANDLE> startedThread{nullptr};

protected:
    void onStarted() override {
        startedThread.store(QThread::currentThreadId(), std::memory_order_relaxed);
        started.store(true, std::memory_order_release);
    }
};

}  // namespace

TEST_CASE("IoWorkerBase: threadName accessor returns constructor value", "[drivers][io_worker_base]") {
    StubWorker w{QStringLiteral("test-thread")};
    REQUIRE(w.threadName() == QStringLiteral("test-thread"));
}

TEST_CASE("IoWorkerBase: onThreadStart invokes onStarted on IO thread", "[drivers][io_worker_base]") {
    QThread thread;
    thread.setObjectName(QStringLiteral("iwb-test"));

    StubWorker worker{QStringLiteral("iwb-test")};
    const auto mainTid = QThread::currentThreadId();
    worker.moveToThread(&thread);

    QObject::connect(&thread, &QThread::started, &worker, &IoWorkerBase::onThreadStart);

    thread.start();

    // Poll for the started flag (event loop on IO thread processes the
    // queued-invocation from QThread::started).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!worker.started.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
        QThread::msleep(5);
    }

    REQUIRE(worker.started.load());
    REQUIRE(worker.startedThread.load() != nullptr);
    REQUIRE(worker.startedThread.load() != mainTid);

    thread.quit();
    REQUIRE(thread.wait(2000));
}
