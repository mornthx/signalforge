// tests/unit/expression/expression_registrar_test.cpp
//
// S5 — verifies ExpressionRegistrar's load-and-start orchestration:
// happy path with a single valid file, validation failure leaves the
// engine stopped + populates errors, missing-file path errors cleanly,
// and multi-file merge runs cycle detection across files.

#include "buffer/signal_buffer_registry.hpp"
#include "decode/decoder_interface.hpp"
#include "expression/expression_engine.hpp"
#include "expression/expression_registrar.hpp"

#include <QDir>
#include <QString>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>

namespace ex7 = signalforge::expression;
namespace dm5 = signalforge::decoder;
namespace bm6 = signalforge::buffer;

namespace {

#ifndef SIGNALFORGE_FIXTURES_DIR
#define SIGNALFORGE_FIXTURES_DIR ""
#endif

QString fixturePath(const QString& subdir, const QString& name) {
    return QStringLiteral("%1/%2/%3.yaml").arg(QStringLiteral(SIGNALFORGE_FIXTURES_DIR), subdir, name);
}

dm5::SignalMetadata makeMeta(QString id, dm5::SignalType type) {
    dm5::SignalMetadata m;
    m.id = std::move(id);
    m.name = QStringLiteral("test");
    m.unit = QStringLiteral("");
    m.type = type;
    return m;
}

void populateBaseSignals(bm6::SignalBufferRegistry& registry) {
    registry.onSignalsRegistered(QStringLiteral("base-driver"),
                                 {makeMeta(QStringLiteral("voltage"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("current"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("voltage_raw"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("temperature"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("counter"), dm5::SignalType::Int64),
                                  makeMeta(QStringLiteral("power_input"), dm5::SignalType::Double),
                                  makeMeta(QStringLiteral("label_signal"), dm5::SignalType::String)});
}

}  // namespace

TEST_CASE("S5: registrar loadAndStart succeeds for valid yaml", "[registrar][s5][happy]") {
    bm6::SignalBufferRegistry registry;
    populateBaseSignals(registry);
    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionRegistrar registrar(registry, engine,
                                       {fixturePath(QStringLiteral("valid_expressions"), QStringLiteral("multi"))});

    REQUIRE(registrar.loadAndStart());
    REQUIRE(registrar.lastErrors().empty());
    REQUIRE(engine.expressionCount() == 3U);
}

TEST_CASE("S5: registrar fails on validation error", "[registrar][s5][failure]") {
    bm6::SignalBufferRegistry registry;
    populateBaseSignals(registry);
    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionRegistrar registrar(
        registry, engine, {fixturePath(QStringLiteral("invalid_expressions"), QStringLiteral("cycle_simple"))});

    REQUIRE_FALSE(registrar.loadAndStart());
    REQUIRE_FALSE(registrar.lastErrors().empty());
    // Engine should not be started; expressionCount stays at 0.
    REQUIRE(engine.expressionCount() == 0U);
}

TEST_CASE("S5: registrar errors cleanly on missing file", "[registrar][s5][missing]") {
    bm6::SignalBufferRegistry registry;
    populateBaseSignals(registry);
    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionRegistrar registrar(registry, engine, {QStringLiteral("/nonexistent/path/to/expressions.yaml")});

    REQUIRE_FALSE(registrar.loadAndStart());
    REQUIRE_FALSE(registrar.lastErrors().empty());
    REQUIRE(engine.expressionCount() == 0U);
}

TEST_CASE("S5: registrar errors on empty path list", "[registrar][s5][empty]") {
    bm6::SignalBufferRegistry registry;
    populateBaseSignals(registry);
    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionRegistrar registrar(registry, engine, {});

    REQUIRE_FALSE(registrar.loadAndStart());
}

TEST_CASE("S5: registrar merges multi-file expression sets", "[registrar][s5][multifile]") {
    // Construct two yaml files in a temp dir; verify they merge for
    // validation (cycle detection runs across them).
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString fileA = tmp.filePath(QStringLiteral("a.yaml"));
    const QString fileB = tmp.filePath(QStringLiteral("b.yaml"));

    // a.yaml: derived signal that depends on a base signal.
    {
        QFile f(fileA);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("schema_version: 1\nexpressions:\n  - id: power\n    formula: voltage * current\n");
    }
    // b.yaml: derived signal that depends on `power` from file A —
    // forward-reference across files is allowed (validator does
    // topological sort regardless of file boundaries).
    {
        QFile f(fileB);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("schema_version: 1\nexpressions:\n  - id: power_kw\n    formula: power / 1000\n");
    }

    bm6::SignalBufferRegistry registry;
    populateBaseSignals(registry);
    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionRegistrar registrar(registry, engine, {fileA, fileB});

    REQUIRE(registrar.loadAndStart());
    REQUIRE(engine.expressionCount() == 2U);
}

TEST_CASE("S5: registrar detects cross-file cycle", "[registrar][s5][multifile_cycle]") {
    QTemporaryDir tmp;
    REQUIRE(tmp.isValid());
    const QString fileA = tmp.filePath(QStringLiteral("a.yaml"));
    const QString fileB = tmp.filePath(QStringLiteral("b.yaml"));

    // a.yaml: alpha depends on beta.
    {
        QFile f(fileA);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("schema_version: 1\nexpressions:\n  - id: alpha\n    formula: beta + 1\n");
    }
    // b.yaml: beta depends on alpha. Cycle.
    {
        QFile f(fileB);
        REQUIRE(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("schema_version: 1\nexpressions:\n  - id: beta\n    formula: alpha * 2\n");
    }

    bm6::SignalBufferRegistry registry;
    populateBaseSignals(registry);
    ex7::ExpressionEngine engine(registry);
    ex7::ExpressionRegistrar registrar(registry, engine, {fileA, fileB});

    REQUIRE_FALSE(registrar.loadAndStart());
    REQUIRE(engine.expressionCount() == 0U);
    // Confirm the error mentions a cycle.
    bool sawCycle = false;
    for (const auto& err : registrar.lastErrors()) {
        if (err.message.contains(QStringLiteral("cycle"), Qt::CaseInsensitive)) {
            sawCycle = true;
            break;
        }
    }
    REQUIRE(sawCycle);
}
