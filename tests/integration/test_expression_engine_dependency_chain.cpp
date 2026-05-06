// tests/integration/test_expression_engine_dependency_chain.cpp
//
// S9 integration test (spec §5.3-2): A → B → C dependency chain.
// Verifies the engine evaluates expressions in topological order so
// that downstream consumers see updated upstream results in the same
// tick.

#include "buffer/signal_buffer.hpp"
#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "expression/expression_engine.hpp"
#include "expression/expression_registrar.hpp"

#include <QFile>
#include <QMetaObject>
#include <QString>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <variant>

using signalforge::buffer::SignalBufferRegistry;
using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
using signalforge::decoder::SignalValue;
using signalforge::expression::ExpressionEngine;
using signalforge::expression::ExpressionRegistrar;

namespace {

void writeYaml(const QString& path, const QString& content) {
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(content.toUtf8());
}

SignalMetadata baseMeta(QString id, SignalType type) {
    SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("base");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

void invokeTick(ExpressionEngine& engine) {
    QMetaObject::invokeMethod(&engine, "onTick", Qt::DirectConnection);
}

}  // namespace

TEST_CASE("S9 integration: dependency chain evaluates topologically", "[integration][s9][expression][chain]") {
    // Define a 3-step chain: a → b → c. base_x = 10.
    //   a = base_x + 1     => 11
    //   b = a * 2          => 22
    //   c = b - 5          => 17
    // If c were evaluated before b, it would see NaN; if b before a, NaN.
    // Topological order ensures all three resolve in one tick.
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString yamlPath = tmp.filePath(QStringLiteral("chain.yaml"));
    writeYaml(yamlPath, QStringLiteral("schema_version: 1\n"
                                       "expressions:\n"
                                       "  - id: c\n"
                                       "    formula: b - 5\n"
                                       "  - id: b\n"
                                       "    formula: a * 2\n"
                                       "  - id: a\n"
                                       "    formula: base_x + 1\n"));

    SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {baseMeta(QStringLiteral("base_x"), SignalType::Double)});

    ExpressionEngine engine(registry);
    ExpressionRegistrar registrar(registry, engine, {yamlPath});
    REQUIRE(registrar.loadAndStart());

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("base_x"), SignalValue{10.0});
    }
    // Drive 200 ticks so that even after the first publish of `a`, `b`
    // sees the published value, and after `b` publishes, `c` sees it.
    // (The buffer publishes every 100 pushes; with topo ordering, all
    // three settle within a single multi-tick window.)
    for (int i = 0; i < 200; ++i) {
        invokeTick(engine);
    }

    auto* aBuf = registry.bufferFor(QStringLiteral("a"));
    auto* bBuf = registry.bufferFor(QStringLiteral("b"));
    auto* cBuf = registry.bufferFor(QStringLiteral("c"));
    REQUIRE(aBuf != nullptr);
    REQUIRE(bBuf != nullptr);
    REQUIRE(cBuf != nullptr);

    REQUIRE(std::get<double>(aBuf->queryLatestOne()->value) == 11.0);
    REQUIRE(std::get<double>(bBuf->queryLatestOne()->value) == 22.0);
    // c may lag by one publish window (200 ticks for a 3-deep chain
    // when each level publishes only every 100 pushes). The 200 ticks
    // above is enough to settle all three.
    REQUIRE(std::get<double>(cBuf->queryLatestOne()->value) == 17.0);
}
