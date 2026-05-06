// tests/integration/test_expression_engine_basic.cpp
//
// S9 integration test (spec §5.3-1): end-to-end expression evaluation.
// Registrar loads a yaml expression set, validates it against a
// SignalBufferRegistry pre-populated with base signals, hands the
// validated set to an ExpressionEngine, then we drive 100 ticks
// (M6 publish cadence) and verify the derived signal is queryable.

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

TEST_CASE("S9 integration: power = v * i end-to-end", "[integration][s9][expression][basic]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString yamlPath = tmp.filePath(QStringLiteral("power.yaml"));
    writeYaml(yamlPath, QStringLiteral("schema_version: 1\n"
                                       "expressions:\n"
                                       "  - id: power\n"
                                       "    name: \"Total power\"\n"
                                       "    unit: W\n"
                                       "    formula: voltage * current\n"
                                       "    type: double\n"));

    SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {baseMeta(QStringLiteral("voltage"), SignalType::Double),
                                  baseMeta(QStringLiteral("current"), SignalType::Double)});

    ExpressionEngine engine(registry);
    ExpressionRegistrar registrar(registry, engine, {yamlPath});
    REQUIRE(registrar.loadAndStart());
    REQUIRE(registrar.lastErrors().empty());

    // Cross M6's publish cadence (100 pushes per signal).
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("voltage"), SignalValue{12.0});
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("current"), SignalValue{2.0});
    }
    for (int i = 0; i < 100; ++i) {
        invokeTick(engine);
    }

    auto* powerBuf = registry.bufferFor(QStringLiteral("power"));
    REQUIRE(powerBuf != nullptr);
    auto latest = powerBuf->queryLatestOne();
    REQUIRE(latest.has_value());
    REQUIRE(std::get<double>(latest->value) == 24.0);
}
