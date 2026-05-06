// tests/integration/test_expression_engine_cycle_rejection.cpp
//
// S9 integration test (spec §5.3-3): circular expression dependencies
// must be rejected at registration time with an actionable error
// (cycle path is part of the message). Engine remains stopped.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "expression/expression_engine.hpp"
#include "expression/expression_registrar.hpp"

#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

using signalforge::buffer::SignalBufferRegistry;
using signalforge::decoder::SignalMetadata;
using signalforge::decoder::SignalType;
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

}  // namespace

TEST_CASE("S9 integration: circular dependencies are rejected with clear error",
          "[integration][s9][expression][cycle]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString yamlPath = tmp.filePath(QStringLiteral("cycle.yaml"));
    // x depends on y; y depends on x → 2-node cycle.
    writeYaml(yamlPath, QStringLiteral("schema_version: 1\n"
                                       "expressions:\n"
                                       "  - id: x\n"
                                       "    formula: y + 1\n"
                                       "  - id: y\n"
                                       "    formula: x * 2\n"));

    SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {baseMeta(QStringLiteral("base_z"), SignalType::Double)});

    ExpressionEngine engine(registry);
    ExpressionRegistrar registrar(registry, engine, {yamlPath});
    REQUIRE_FALSE(registrar.loadAndStart());

    bool sawCycle = false;
    bool sawCyclePath = false;
    for (const auto& e : registrar.lastErrors()) {
        if (e.message.contains(QStringLiteral("cycle"), Qt::CaseInsensitive)) {
            sawCycle = true;
        }
        // Either x → y → x or y → x → y must appear in the message.
        if (e.message.contains(QStringLiteral("x")) && e.message.contains(QStringLiteral("y")) &&
            e.message.contains(QStringLiteral("->"))) {
            sawCyclePath = true;
        }
    }
    REQUIRE(sawCycle);
    REQUIRE(sawCyclePath);

    // Engine must not have any derived signal published.
    REQUIRE(registry.bufferFor(QStringLiteral("x")) == nullptr);
    REQUIRE(registry.bufferFor(QStringLiteral("y")) == nullptr);
}
