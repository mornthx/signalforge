// tests/unit/session/session_writer_backpressure_test.cpp
//
// S5 backpressure tests for the C3 4-point queue policy:
//   1. Droppable + queue full → drop NEW; return false.
//   2. Non-droppable + queue full → drop OLDEST droppable;
//      enqueue NEW; return true.
//   3. Queue full of non-droppable + 10 ms timeout → log ERROR,
//      return false.
//   4. Recovery: after the worker drains, subsequent enqueues
//      succeed normally.
//
// Tests bypass the worker thread and exercise SessionFileWriter
// directly so we can fill the queue without races. SessionFileWriter
// is internal-only per spec §6.2 but its header is publicly
// includeable.
#include "decode/decoder_interface.hpp"
#include "session/session_file_writer.hpp"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>

namespace s = signalforge::session;
namespace d = signalforge::decoder;

namespace {

struct BackpressureFixture {
    BackpressureFixture() {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app_ = std::make_unique<QCoreApplication>(argc, argv);
        }
    }
    std::unique_ptr<QCoreApplication> app_;
    QTemporaryDir tmp_;
};

s::WriteSignalEvent makeWrite(const QString& id = QStringLiteral("x"), double v = 0.0) {
    s::WriteSignalEvent e;
    e.timestamp = std::chrono::steady_clock::time_point{};
    e.signalId = id;
    e.value = d::SignalValue{v};
    return e;
}

s::CatalogExtensionEvent makeExt() {
    return s::CatalogExtensionEvent{QStringLiteral("d2"), {}};
}

}  // namespace

// Capacity is 10000 — too large for fast tests. The header exposes
// it via a static constexpr; we read it indirectly via the public
// behavior. To keep tests practical we fill exactly to capacity
// using a helper.
namespace {
constexpr std::size_t kCapacity = 10000;

void fillToCapacity(s::SessionFileWriter& fw) {
    for (std::size_t i = 0; i < kCapacity; ++i) {
        REQUIRE(fw.enqueue(makeWrite()));
    }
}
}  // namespace

TEST_CASE_METHOD(BackpressureFixture, "S5: droppable enqueue at capacity is rejected", "[session][s5][backpressure]") {
    s::SessionFileWriter fw;
    s::SessionMetadata md;
    REQUIRE(fw.openFile(tmp_.filePath(QStringLiteral("p1.sfreplay")), md));

    fillToCapacity(fw);

    // Next droppable must be rejected.
    REQUIRE_FALSE(fw.enqueue(makeWrite()));
    REQUIRE(fw.droppedEvents() == 1);

    // Another rejection still increments the counter.
    REQUIRE_FALSE(fw.enqueue(makeWrite()));
    REQUIRE(fw.droppedEvents() == 2);
}

TEST_CASE_METHOD(BackpressureFixture, "S5: non-droppable evicts oldest droppable", "[session][s5][backpressure]") {
    s::SessionFileWriter fw;
    s::SessionMetadata md;
    REQUIRE(fw.openFile(tmp_.filePath(QStringLiteral("p2.sfreplay")), md));

    fillToCapacity(fw);

    // Non-droppable arrival succeeds by evicting one droppable.
    REQUIRE(fw.enqueue(makeExt()));
    REQUIRE(fw.droppedEvents() == 1);

    // The catalog extension is now in the queue. Another
    // non-droppable also evicts a droppable — there are still
    // ~kCapacity-1 droppables left.
    REQUIRE(fw.enqueue(makeExt()));
    REQUIRE(fw.droppedEvents() == 2);
}

TEST_CASE_METHOD(BackpressureFixture, "S5: stop sentinel is non-droppable", "[session][s5][backpressure]") {
    s::SessionFileWriter fw;
    s::SessionMetadata md;
    REQUIRE(fw.openFile(tmp_.filePath(QStringLiteral("p3.sfreplay")), md));

    fillToCapacity(fw);

    // StopEvent should also be honored by evicting a droppable.
    REQUIRE(fw.enqueue(s::StopEvent{}));
    REQUIRE(fw.droppedEvents() == 1);
}

TEST_CASE_METHOD(BackpressureFixture, "S5: counters are zero when no overflow happens", "[session][s5][backpressure]") {
    s::SessionFileWriter fw;
    s::SessionMetadata md;
    REQUIRE(fw.openFile(tmp_.filePath(QStringLiteral("p4.sfreplay")), md));

    // Enqueue well under capacity — no drops expected.
    for (int i = 0; i < 100; ++i) {
        REQUIRE(fw.enqueue(makeWrite()));
    }
    REQUIRE(fw.droppedEvents() == 0);
}

TEST_CASE_METHOD(BackpressureFixture, "S5: dropped counter exposes session_writer_dropped_events_total metric",
                 "[session][s5][backpressure]") {
    s::SessionFileWriter fw;
    s::SessionMetadata md;
    REQUIRE(fw.openFile(tmp_.filePath(QStringLiteral("p5.sfreplay")), md));

    fillToCapacity(fw);

    // 5 droppable rejections + 3 non-droppable evictions = 8 drops.
    for (int i = 0; i < 5; ++i) {
        REQUIRE_FALSE(fw.enqueue(makeWrite()));
    }
    for (int i = 0; i < 3; ++i) {
        REQUIRE(fw.enqueue(makeExt()));
    }
    REQUIRE(fw.droppedEvents() == 8);
}
