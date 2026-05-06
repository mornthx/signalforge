// tests/integration/test_expression_engine_type_promotion.cpp
//
// S9 integration test (spec §5.3-4): bool / int64 source signals
// promote correctly into exprtk's double-typed evaluation context.
// (Per spec §3.1 / decision M7.3-P: bool → 0.0/1.0; int64 → double.)

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
#include <cstdint>
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

TEST_CASE("S9 integration: bool source promotes to 0.0/1.0 in formula",
          "[integration][s9][expression][type_promote][bool]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString yamlPath = tmp.filePath(QStringLiteral("bool_promote.yaml"));
    // bool * 5: true → 5, false → 0.
    writeYaml(yamlPath, QStringLiteral("schema_version: 1\n"
                                       "expressions:\n"
                                       "  - id: scaled\n"
                                       "    formula: flag * 5.0\n"
                                       "    type: double\n"));

    SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"), {baseMeta(QStringLiteral("flag"), SignalType::Bool)});

    ExpressionEngine engine(registry);
    ExpressionRegistrar registrar(registry, engine, {yamlPath});
    REQUIRE(registrar.loadAndStart());

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("flag"), SignalValue{true});
    }
    for (int i = 0; i < 100; ++i) {
        invokeTick(engine);
    }

    auto* scaledBuf = registry.bufferFor(QStringLiteral("scaled"));
    REQUIRE(scaledBuf != nullptr);
    auto latest = scaledBuf->queryLatestOne();
    REQUIRE(latest.has_value());
    REQUIRE(std::get<double>(latest->value) == 5.0);
}

TEST_CASE("S9 integration: int64 source promotes to double in formula",
          "[integration][s9][expression][type_promote][int64]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString yamlPath = tmp.filePath(QStringLiteral("int64_promote.yaml"));
    // counter / 4 with counter=42: 10.5 (as double).
    writeYaml(yamlPath, QStringLiteral("schema_version: 1\n"
                                       "expressions:\n"
                                       "  - id: ratio\n"
                                       "    formula: counter / 4.0\n"
                                       "    type: double\n"));

    SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {baseMeta(QStringLiteral("counter"), SignalType::Int64)});

    ExpressionEngine engine(registry);
    ExpressionRegistrar registrar(registry, engine, {yamlPath});
    REQUIRE(registrar.loadAndStart());

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        registry.onSignal(t0 + std::chrono::microseconds(i), QStringLiteral("counter"),
                          SignalValue{static_cast<std::int64_t>(42)});
    }
    for (int i = 0; i < 100; ++i) {
        invokeTick(engine);
    }

    auto* ratioBuf = registry.bufferFor(QStringLiteral("ratio"));
    REQUIRE(ratioBuf != nullptr);
    auto latest = ratioBuf->queryLatestOne();
    REQUIRE(latest.has_value());
    REQUIRE(std::get<double>(latest->value) == 10.5);
}
