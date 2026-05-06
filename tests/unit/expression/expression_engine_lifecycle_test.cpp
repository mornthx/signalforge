// tests/unit/expression/expression_engine_lifecycle_test.cpp
//
// S4 — verifies ExpressionEngine lifecycle (start/stop/start cycles),
// single-tick evaluation produces a derived signal in the registry,
// multi-tick stat accumulation, and source-NaN propagation when a
// base signal has no value.
//
// Tests invoke onTick() directly via QMetaObject::invokeMethod to avoid
// dependency on a running QCoreApplication event loop. The QTimer
// dispatch path is exercised in S9 integration tests where the test
// binary creates a Qt event loop.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "expression/expression.hpp"
#include "expression/expression_engine.hpp"

#include <QMetaObject>
#include <QString>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <variant>

namespace ex7 = signalforge::expression;
namespace dm5 = signalforge::decoder;
namespace bm6 = signalforge::buffer;

namespace {

ex7::Expression makeExpr(QString id, QString formula, std::vector<QString> srcIds,
                         ex7::ExpressionOutputType out = ex7::ExpressionOutputType::Double) {
    return ex7::Expression(id, id, QStringLiteral(""), std::nullopt, std::move(formula), out, std::move(srcIds));
}

dm5::SignalMetadata makeBaseMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("base");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

void invokeTick(ex7::ExpressionEngine& engine) {
    QMetaObject::invokeMethod(&engine, "onTick", Qt::DirectConnection);
}

/// Drive enough ticks to cross the SignalBuffer publish cadence (100).
/// Each tick pushes one sample per derived signal; the buffer publishes
/// the segment on the 100th push. Calling `queryLatestOne` after fewer
/// pushes returns nullopt by design (M6 cadence-based publish).
void runTicksForFirstPublish(ex7::ExpressionEngine& engine) {
    for (int i = 0; i < 100; ++i) {
        invokeTick(engine);
    }
}

}  // namespace

TEST_CASE("S4: engine lifecycle start/stop is idempotent", "[engine][s4][lifecycle]") {
    // QTimer.isActive() requires a QCoreApplication to dispatch events.
    // Catch2WithMain doesn't create one, so we cannot reliably assert
    // isRunning() transitions in unit tests. The cadence behavior is
    // exercised in S9 integration tests where a Qt event loop runs.
    // Here we verify start/stop are non-throwing and idempotent.
    bm6::SignalBufferRegistry registry;
    ex7::ExpressionEngine engine(registry);

    engine.start();
    engine.start();  // idempotent
    engine.stop();
    engine.stop();  // idempotent
    engine.start();
    engine.stop();
    SUCCEED();
}

TEST_CASE("S4: setExpressions registers derived signals", "[engine][s4][register]") {
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeBaseMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
                                  makeBaseMeta(QStringLiteral("current"), dm5::SignalType::Double)});

    ex7::ExpressionEngine engine(registry);

    ex7::ExpressionSet set;
    set.expressions.push_back(makeExpr(QStringLiteral("power"), QStringLiteral("voltage * current"),
                                       {QStringLiteral("voltage"), QStringLiteral("current")}));
    set.baseSignalIds = {QStringLiteral("voltage"), QStringLiteral("current")};

    engine.setExpressions(std::move(set));
    REQUIRE(engine.expressionCount() == 1U);
    REQUIRE(registry.bufferFor(QStringLiteral("power")) != nullptr);
}

TEST_CASE("S4: single tick increments push count for derived signal", "[engine][s4][tick]") {
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeBaseMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
                                  makeBaseMeta(QStringLiteral("current"), dm5::SignalType::Double)});

    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionSet set;
    set.expressions.push_back(makeExpr(QStringLiteral("power"), QStringLiteral("voltage * current"),
                                       {QStringLiteral("voltage"), QStringLiteral("current")}));
    engine.setExpressions(std::move(set));

    auto t0 = std::chrono::steady_clock::now();
    registry.onSignal(t0, QStringLiteral("voltage"), dm5::SignalValue{12.0});
    registry.onSignal(t0, QStringLiteral("current"), dm5::SignalValue{2.0});

    invokeTick(engine);

    REQUIRE(engine.stats().ticksTotal == 1U);
    REQUIRE(engine.stats().evaluationsTotal == 1U);

    auto* powerBuf = registry.bufferFor(QStringLiteral("power"));
    REQUIRE(powerBuf != nullptr);
    REQUIRE(powerBuf->totalSamplesPushed() == 1U);
    // queryLatestOne returns nullopt until the buffer's publish cadence
    // (100) is crossed — see M6 design. The next test exercises
    // queryLatestOne after 100 ticks.
}

TEST_CASE("S4: 100 ticks crosses publish cadence and exposes latest value", "[engine][s4][publish]") {
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeBaseMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
                                  makeBaseMeta(QStringLiteral("current"), dm5::SignalType::Double)});

    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionSet set;
    set.expressions.push_back(makeExpr(QStringLiteral("power"), QStringLiteral("voltage * current"),
                                       {QStringLiteral("voltage"), QStringLiteral("current")}));
    engine.setExpressions(std::move(set));

    // Push each base signal 100 times so its queryLatestOne resolves
    // (M6 publish cadence = 100). Each tick of the engine then sees real
    // values and computes 12.0 * 2.0 = 24.0.
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("voltage"), dm5::SignalValue{12.0});
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("current"), dm5::SignalValue{2.0});
    }

    runTicksForFirstPublish(engine);

    auto* powerBuf = registry.bufferFor(QStringLiteral("power"));
    REQUIRE(powerBuf != nullptr);
    REQUIRE(powerBuf->totalSamplesPushed() == 100U);
    auto latest = powerBuf->queryLatestOne();
    REQUIRE(latest.has_value());
    REQUIRE(std::get<double>(latest->value) == 24.0);
}

TEST_CASE("S4: multi-tick increments stats counters", "[engine][s4][multi_tick]") {
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeBaseMeta(QStringLiteral("x"), dm5::SignalType::Double)});

    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionSet set;
    set.expressions.push_back(makeExpr(QStringLiteral("doubled"), QStringLiteral("x * 2"), {QStringLiteral("x")}));
    engine.setExpressions(std::move(set));

    auto t0 = std::chrono::steady_clock::now();
    registry.onSignal(t0, QStringLiteral("x"), dm5::SignalValue{5.0});

    for (int i = 0; i < 10; ++i) {
        invokeTick(engine);
    }

    const auto stats = engine.stats();
    REQUIRE(stats.ticksTotal == 10U);
    REQUIRE(stats.evaluationsTotal == 10U);
    REQUIRE(stats.lastTickDurationUs.count() >= 0);
}

TEST_CASE("S4: missing source produces NaN derived value", "[engine][s4][nan]") {
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeBaseMeta(QStringLiteral("a"), dm5::SignalType::Double),
                                  makeBaseMeta(QStringLiteral("b"), dm5::SignalType::Double)});

    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionSet set;
    set.expressions.push_back(
        makeExpr(QStringLiteral("sum"), QStringLiteral("a + b"), {QStringLiteral("a"), QStringLiteral("b")}));
    engine.setExpressions(std::move(set));

    // Only push `a`; `b` has no value. Push 100 times to cross publish
    // cadence so queryLatestOne resolves.
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("a"), dm5::SignalValue{1.0});
    }

    runTicksForFirstPublish(engine);

    auto* sumBuf = registry.bufferFor(QStringLiteral("sum"));
    REQUIRE(sumBuf != nullptr);
    auto latest = sumBuf->queryLatestOne();
    REQUIRE(latest.has_value());
    REQUIRE(std::isnan(std::get<double>(latest->value)));
}

TEST_CASE("S4: dependency-chain evaluates in topological order", "[engine][s4][topo]") {
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeBaseMeta(QStringLiteral("base_x"), dm5::SignalType::Double)});

    ex7::ExpressionEngine engine(registry);

    // Two derived signals where `b` depends on `a`. Engine must run `a`
    // first within the tick so `b`'s queryLatestOne sees a fresh `a`
    // value.
    ex7::ExpressionSet set;
    set.expressions.push_back(makeExpr(QStringLiteral("a"), QStringLiteral("base_x * 2"), {QStringLiteral("base_x")}));
    set.expressions.push_back(makeExpr(QStringLiteral("b"), QStringLiteral("a + 1"), {QStringLiteral("a")}));
    engine.setExpressions(std::move(set));

    // Push base value 100 times so its queryLatestOne resolves to 10.0,
    // then run 200 ticks: the first ~100 ticks build up `a`'s buffer
    // (publishing on the 100th); the next 100 ticks let `b`'s reads of
    // `a` see the published value, and `b` itself publishes on its
    // 100th sample → query returns the stable result.
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("base_x"), dm5::SignalValue{10.0});
    }

    for (int i = 0; i < 200; ++i) {
        invokeTick(engine);
    }

    auto* aBuf = registry.bufferFor(QStringLiteral("a"));
    auto* bBuf = registry.bufferFor(QStringLiteral("b"));
    REQUIRE(aBuf != nullptr);
    REQUIRE(bBuf != nullptr);
    REQUIRE(std::get<double>(aBuf->queryLatestOne()->value) == 20.0);
    // After 200 ticks, both a and b have published; b sees a=20 → b=21.
    REQUIRE(std::get<double>(bBuf->queryLatestOne()->value) == 21.0);
}

TEST_CASE("S4: empty expression set ticks cleanly", "[engine][s4][empty]") {
    bm6::SignalBufferRegistry registry;
    ex7::ExpressionEngine engine(registry);
    REQUIRE(engine.expressionCount() == 0U);

    invokeTick(engine);
    invokeTick(engine);

    const auto stats = engine.stats();
    REQUIRE(stats.ticksTotal == 2U);
    REQUIRE(stats.evaluationsTotal == 0U);
}

TEST_CASE("S8: setExpressions called twice unregisters then re-registers", "[engine][s8][reset]") {
    bm6::SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeBaseMeta(QStringLiteral("a"), dm5::SignalType::Double)});

    ex7::ExpressionEngine engine(registry);
    {
        ex7::ExpressionSet first;
        first.expressions.push_back(makeExpr(QStringLiteral("d1"), QStringLiteral("a + 1"), {QStringLiteral("a")}));
        engine.setExpressions(std::move(first));
        REQUIRE(engine.expressionCount() == 1U);
    }
    // Second call must unregister the prior derived signals first, then
    // register the new set. No exceptions, no leaks (verified by ASan
    // when the suite runs under debug-asan in CI).
    {
        ex7::ExpressionSet second;
        second.expressions.push_back(makeExpr(QStringLiteral("d2"), QStringLiteral("a + 2"), {QStringLiteral("a")}));
        second.expressions.push_back(makeExpr(QStringLiteral("d3"), QStringLiteral("a + 3"), {QStringLiteral("a")}));
        engine.setExpressions(std::move(second));
        REQUIRE(engine.expressionCount() == 2U);
    }

    invokeTick(engine);
    const auto stats = engine.stats();
    REQUIRE(stats.evaluationsTotal == 2U);
}

TEST_CASE("S8: bufferFor==nullptr yields NaN (unregistered source)", "[engine][s8][nan][nullbuf]") {
    // Expression references a base signal that the registry has never
    // heard of. bufferFor returns nullptr; the engine must substitute
    // NaN rather than crash. This complements the S4 NaN test, which
    // covered the "registered but no published value" path.
    bm6::SignalBufferRegistry registry;
    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionSet set;
    set.expressions.push_back(
        makeExpr(QStringLiteral("derived"), QStringLiteral("ghost + 1.0"), {QStringLiteral("ghost")}));
    engine.setExpressions(std::move(set));

    runTicksForFirstPublish(engine);

    auto* derivedBuf = registry.bufferFor(QStringLiteral("derived"));
    REQUIRE(derivedBuf != nullptr);
    auto latest = derivedBuf->queryLatestOne();
    REQUIRE(latest.has_value());
    REQUIRE(std::isnan(std::get<double>(latest->value)));
}
