// tests/integration/test_expression_engine_qstring_rejection.cpp
//
// S9 integration test (spec §5.3-5): QString-typed source signals
// must be rejected at registration time. exprtk's evaluation context
// is double-typed, and there is no sensible coercion from QString to
// double. (Per spec §3.1 / decision M7.3-P.)

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

TEST_CASE("S9 integration: QString source rejected at registration", "[integration][s9][expression][qstring_reject]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString yamlPath = tmp.filePath(QStringLiteral("qstring_use.yaml"));
    writeYaml(yamlPath, QStringLiteral("schema_version: 1\n"
                                       "expressions:\n"
                                       "  - id: derived\n"
                                       "    formula: label_signal + 1.0\n"));

    SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {baseMeta(QStringLiteral("label_signal"), SignalType::String)});

    ExpressionEngine engine(registry);
    ExpressionRegistrar registrar(registry, engine, {yamlPath});
    REQUIRE_FALSE(registrar.loadAndStart());

    bool sawQStringRejection = false;
    bool errorReferencesExpression = false;
    for (const auto& e : registrar.lastErrors()) {
        if (e.message.contains(QStringLiteral("QString"))) {
            sawQStringRejection = true;
        }
        if (e.expressionId == QStringLiteral("derived")) {
            errorReferencesExpression = true;
        }
    }
    REQUIRE(sawQStringRejection);
    REQUIRE(errorReferencesExpression);

    REQUIRE(registry.bufferFor(QStringLiteral("derived")) == nullptr);
}
