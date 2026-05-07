// tests/integration/test_expression_engine_restricted_syntax.cpp
//
// S9 integration test (spec §5.3-6): restricted-syntax violations
// (`if`, `while`, `?:`, etc.) must be rejected at registration time
// with an actionable error pointing at the offending expression.
// (Per spec §3.1 whitelist + decision M7.1-B.)

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

bool runRejection(const QString& formula, const QString& exprId) {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString yamlPath = tmp.filePath(QStringLiteral("restricted.yaml"));
    // Quote the formula so yaml-cpp does not treat `:` (in `a ? b : c`)
    // or other punctuation as map syntax. Inside double-quoted yaml
    // scalars, only `"` and `\` need escaping.
    QString escaped = formula;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    writeYaml(yamlPath, QStringLiteral("schema_version: 1\n"
                                       "expressions:\n"
                                       "  - id: %1\n"
                                       "    formula: \"%2\"\n")
                            .arg(exprId, escaped));

    SignalBufferRegistry registry;
    registry.onSignalsRegistered(QStringLiteral("base-driver"), {baseMeta(QStringLiteral("a"), SignalType::Double),
                                                                 baseMeta(QStringLiteral("b"), SignalType::Double)});

    ExpressionEngine engine(registry);
    ExpressionRegistrar registrar(registry, engine, {yamlPath});
    const bool started = registrar.loadAndStart();
    if (started) {
        return false;  // Should have rejected.
    }
    if (registry.bufferFor(exprId) != nullptr) {
        return false;
    }
    bool messageReferencesExpr = false;
    for (const auto& e : registrar.lastErrors()) {
        if (e.expressionId == exprId) {
            messageReferencesExpr = true;
        }
    }
    return messageReferencesExpr;
}

}  // namespace

TEST_CASE("S9 integration: 'if(...)' formula rejected", "[integration][s9][expression][restricted][if]") {
    REQUIRE(runRejection(QStringLiteral("if(a > 0, a, b)"), QStringLiteral("ifexp")));
}

TEST_CASE("S9 integration: 'while(...)' formula rejected", "[integration][s9][expression][restricted][while]") {
    REQUIRE(runRejection(QStringLiteral("while(a < 5){ a := a + 1; }"), QStringLiteral("whilexp")));
}

TEST_CASE("S9 integration: ternary '?:' formula rejected", "[integration][s9][expression][restricted][ternary]") {
    REQUIRE(runRejection(QStringLiteral("a > 0 ? a : b"), QStringLiteral("ternexp")));
}

TEST_CASE("S9 integration: 'for' formula rejected", "[integration][s9][expression][restricted][for]") {
    REQUIRE(runRejection(QStringLiteral("for(var i := 0; i < 10; i += 1) { a := a + i; }"), QStringLiteral("forexp")));
}
