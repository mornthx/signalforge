// src/expression/expression_validator.cpp
#include "expression/expression_validator.hpp"

#include "observability/logging.hpp"

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <yaml-cpp/yaml.h>

// exprtk header is included here for the pre-construct compile check.
// Same NOLINT comment as expression.cpp.
// NOLINTBEGIN
#include <exprtk.hpp>
// NOLINTEND

namespace signalforge::expression {

namespace {

/// Reserved/whitelisted identifiers — anything matching these is NOT a
/// source signal reference. Sync with `applyRestrictedSettings` in
/// `expression.cpp` (S2).
const std::unordered_set<std::string>& reservedIdentifiers() {
    static const std::unordered_set<std::string> kReserved = {
        // Whitelisted math functions (spec §3.1).
        "min",
        "max",
        "abs",
        "sqrt",
        "log",
        "log10",
        "exp",
        "sin",
        "cos",
        "tan",
        "floor",
        "ceil",
        "round",
        // Logical operators (spec §3.1 allows them as keywords).
        "and",
        "or",
        "not",
        "xor",
        "nand",
        "nor",
        "xnor",
        // exprtk constants (added by `add_constants`).
        "pi",
        "epsilon",
        "inf",
        // Reserved keywords from exprtk (control flow we disabled, but the
        // identifiers can appear in user-error formulas).
        "if",
        "else",
        "while",
        "for",
        "switch",
        "case",
        "break",
        "continue",
        "return",
        "var",
        "true",
        "false",
    };
    return kReserved;
}

/// Set of forbidden function names (spec §3.1 reject-list). Anything
/// matching these in the formula text triggers a whitelist-violation
/// error before exprtk gets a chance to parse. exprtk's own settings
/// catch most control-flow forms (S2's `disable_all_control_structures`),
/// but per-base-function whitelist enforcement lives here so the error
/// message is user-facing.
const std::unordered_set<std::string>& forbiddenFunctionNames() {
    static const std::unordered_set<std::string> kForbidden = {
        // exprtk built-ins outside our whitelist.
        "acos",
        "acosh",
        "asin",
        "asinh",
        "atan",
        "atan2",
        "atanh",
        "avg",
        "cbrt",
        "clamp",
        "cosh",
        "cot",
        "csc",
        "deg2rad",
        "expm1",
        "frac",
        "iclamp",
        "log1p",
        "log2",
        "logn",
        "mean",
        "median",
        "mod",
        "mul",
        "ncdf",
        "pow",
        "rad2deg",
        "sec",
        "sgn",
        "sinh",
        "sum",
        "tanh",
        "trunc",
        // Statistical / aggregate-over-time (spec §2.2-5).
        "stddev",
        "variance",
        // Forbidden control-flow forms even as functions (exprtk supports
        // `if(cond, then, else)` as a function too).
        "if",
    };
    return kForbidden;
}

/// Walk the formula text and extract identifiers that are user-defined
/// (not reserved, not forbidden). The result is the set of source
/// signal IDs the formula references.
[[nodiscard]] std::vector<QString> extractSourceIds(const QString& formula) {
    static const QRegularExpression kIdent(QStringLiteral("[a-zA-Z_][a-zA-Z0-9_]*"));
    std::vector<QString> out;
    std::unordered_set<std::string> seen;
    auto it = kIdent.globalMatch(formula);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString cap = m.captured();
        const std::string capStd = cap.toStdString();
        if (reservedIdentifiers().count(capStd) > 0) {
            continue;
        }
        if (forbiddenFunctionNames().count(capStd) > 0) {
            continue;  // forbidden — caller catches via separate check
        }
        if (seen.insert(capStd).second) {
            out.push_back(cap);
        }
    }
    return out;
}

/// True if the formula text mentions any forbidden function name.
[[nodiscard]] std::optional<QString> firstForbiddenName(const QString& formula) {
    static const QRegularExpression kIdent(QStringLiteral("[a-zA-Z_][a-zA-Z0-9_]*"));
    auto it = kIdent.globalMatch(formula);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString cap = m.captured();
        if (forbiddenFunctionNames().count(cap.toStdString()) > 0) {
            return cap;
        }
    }
    return std::nullopt;
}

/// Apply spec §3.1 restricted-syntax settings (mirrors S2's helper —
/// kept private here so we don't expose exprtk types from a header).
template <typename ParserT> void applyValidatorSettings(ParserT& parser) {
    auto& s = parser.settings();
    s.disable_all_control_structures();
    s.disable_local_vardef();
    s.disable_all_assignment_ops();
}

/// Run exprtk compile on the formula with the given source IDs as
/// bound variables. Returns nullopt on success, error text on failure.
[[nodiscard]] std::optional<QString> exprtkCompileError(const QString& formula, const std::vector<QString>& sourceIds) {
    exprtk::symbol_table<double> symbols;
    std::vector<double> valueSlots(sourceIds.size(), 0.0);
    for (std::size_t i = 0; i < sourceIds.size(); ++i) {
        symbols.add_variable(sourceIds[i].toStdString(), valueSlots[i]);
    }
    symbols.add_constants();

    exprtk::expression<double> compiled;
    compiled.register_symbol_table(symbols);

    exprtk::parser<double> parser;
    applyValidatorSettings(parser);
    if (parser.compile(formula.toStdString(), compiled)) {
        return std::nullopt;
    }
    QString err = QStringLiteral("formula compile error:");
    for (std::size_t i = 0; i < parser.error_count(); ++i) {
        const auto e = parser.get_error(i);
        err += QStringLiteral(" [%1] %2")
                   .arg(static_cast<int>(e.token.position))
                   .arg(QString::fromStdString(e.diagnostic));
    }
    return err;
}

struct ParsedExpression {
    QString id;
    QString name;
    QString unit;
    std::optional<QString> description;
    QString formula;
    ExpressionOutputType outputType = ExpressionOutputType::Double;
    int line = -1;
    std::vector<QString> sourceIds;
};

/// Convert an `ExpressionOutputType` keyword from yaml to the enum.
/// Returns nullopt if the keyword is missing or unknown. Default is
/// `Double` per spec §4.4.
[[nodiscard]] std::optional<ExpressionOutputType> parseOutputType(const std::string& s) {
    if (s == "double") {
        return ExpressionOutputType::Double;
    }
    if (s == "bool") {
        return ExpressionOutputType::Bool;
    }
    if (s == "int64") {
        return ExpressionOutputType::Int64;
    }
    return std::nullopt;
}

/// DFS three-color cycle detection. Returns nullopt if no cycle, or
/// the cycle path as a vector of expression IDs.
[[nodiscard]] std::optional<std::vector<QString>>
findCycle(const std::unordered_map<QString, std::vector<QString>>& graph) {
    enum Color { White, Gray, Black };
    std::unordered_map<QString, Color> color;
    std::unordered_map<QString, QString> parent;

    // Initialize all nodes white.
    for (const auto& [node, _] : graph) {
        color[node] = White;
    }

    std::vector<QString> cycle;
    std::function<bool(const QString&)> dfs = [&](const QString& node) -> bool {
        color[node] = Gray;
        auto graphIt = graph.find(node);
        if (graphIt != graph.end()) {
            for (const auto& neighbor : graphIt->second) {
                if (color.find(neighbor) == color.end()) {
                    continue;  // neighbor is a base signal, not a node
                }
                if (color[neighbor] == Gray) {
                    // Back-edge: build cycle path from `neighbor` through
                    // parent chain to `node`, then close the loop.
                    cycle.push_back(neighbor);
                    QString cur = node;
                    while (cur != neighbor && parent.count(cur) > 0) {
                        cycle.push_back(cur);
                        cur = parent[cur];
                    }
                    cycle.push_back(neighbor);  // close the cycle
                    std::reverse(cycle.begin(), cycle.end());
                    return true;
                }
                if (color[neighbor] == White) {
                    parent[neighbor] = node;
                    if (dfs(neighbor)) {
                        return true;
                    }
                }
            }
        }
        color[node] = Black;
        return false;
    };

    for (const auto& [node, _] : graph) {
        if (color[node] == White) {
            if (dfs(node)) {
                return cycle;
            }
        }
    }
    return std::nullopt;
}

/// Topological sort via Kahn's algorithm. Returns nodes in evaluation
/// order (sources before consumers). Pre-condition: graph is acyclic.
[[nodiscard]] std::vector<QString> topologicalSort(const std::unordered_map<QString, std::vector<QString>>& graph,
                                                   const std::unordered_set<QString>& expressionIds) {
    // Build in-degree map for expression nodes only.
    std::unordered_map<QString, int> inDeg;
    for (const auto& id : expressionIds) {
        inDeg[id] = 0;
    }
    for (const auto& [node, neighbors] : graph) {
        for (const auto& n : neighbors) {
            if (expressionIds.count(n) > 0) {
                inDeg[n]++;
            }
        }
    }
    std::vector<QString> order;
    std::vector<QString> queue;
    for (const auto& [id, deg] : inDeg) {
        if (deg == 0) {
            queue.push_back(id);
        }
    }
    while (!queue.empty()) {
        const QString cur = queue.back();
        queue.pop_back();
        order.push_back(cur);
        auto it = graph.find(cur);
        if (it == graph.end()) {
            continue;
        }
        for (const auto& neighbor : it->second) {
            if (expressionIds.count(neighbor) > 0) {
                if (--inDeg[neighbor] == 0) {
                    queue.push_back(neighbor);
                }
            }
        }
    }
    // Note: callers reverse this if they want sources-before-consumers.
    return order;
}

}  // namespace

ExpressionValidationResult
ExpressionValidator::validateString(const QString& yamlContent, const QString& virtualPath,
                                    const std::vector<signalforge::decoder::SignalMetadata>& availableSignals) {
    std::vector<ExpressionValidationError> errors;

    auto pushErr = [&](int line, QString exprId, QString msg) {
        ExpressionValidationError e;
        e.filePath = virtualPath;
        e.lineNumber = line;
        e.expressionId = std::move(exprId);
        e.message = std::move(msg);
        errors.push_back(std::move(e));
    };

    YAML::Node root;
    try {
        root = YAML::Load(yamlContent.toStdString());
    } catch (const YAML::Exception& e) {
        pushErr(e.mark.line + 1, QString{}, QStringLiteral("yaml parse error: %1").arg(QString::fromStdString(e.msg)));
        return std::unexpected(std::move(errors));
    }
    if (!root || !root.IsMap()) {
        pushErr(-1, QString{}, QStringLiteral("yaml root must be a mapping"));
        return std::unexpected(std::move(errors));
    }

    // Top-level shape check.
    if (!root["schema_version"]) {
        pushErr(-1, QString{}, QStringLiteral("missing required key 'schema_version' at top level"));
    } else {
        const auto v = root["schema_version"];
        if (!v.IsScalar() || v.as<int>(0) != 1) {
            pushErr(v.Mark().line + 1, QString{}, QStringLiteral("schema_version must be 1"));
        }
    }
    const auto exprList = root["expressions"];
    if (!exprList) {
        pushErr(-1, QString{}, QStringLiteral("missing required key 'expressions' at top level"));
        return std::unexpected(std::move(errors));
    }
    if (!exprList.IsSequence()) {
        pushErr(exprList.Mark().line + 1, QString{}, QStringLiteral("'expressions' must be a sequence"));
        return std::unexpected(std::move(errors));
    }

    // Parse each expression entry.
    std::vector<ParsedExpression> parsed;
    parsed.reserve(exprList.size());
    std::unordered_set<QString> seenIds;

    // Build base-signal lookup for collision + type checks.
    std::unordered_map<QString, signalforge::decoder::SignalType> baseSignalTypes;
    for (const auto& meta : availableSignals) {
        baseSignalTypes[meta.id] = meta.type;
    }

    for (std::size_t i = 0; i < exprList.size(); ++i) {
        const auto entry = exprList[i];
        const int entryLine = entry.Mark().line + 1;
        if (!entry.IsMap()) {
            pushErr(entryLine, QString{}, QStringLiteral("expression entry must be a mapping"));
            continue;
        }
        ParsedExpression p;
        p.line = entryLine;
        // Required: id
        if (!entry["id"] || !entry["id"].IsScalar()) {
            pushErr(entryLine, QString{}, QStringLiteral("expression missing required field 'id'"));
            continue;
        }
        p.id = QString::fromStdString(entry["id"].as<std::string>());
        if (p.id.isEmpty()) {
            pushErr(entry["id"].Mark().line + 1, QString{}, QStringLiteral("expression 'id' must be non-empty"));
            continue;
        }
        if (!seenIds.insert(p.id).second) {
            pushErr(entry["id"].Mark().line + 1, p.id, QStringLiteral("duplicate expression id '%1'").arg(p.id));
            continue;
        }
        if (baseSignalTypes.count(p.id) > 0) {
            pushErr(entry["id"].Mark().line + 1, p.id,
                    QStringLiteral("expression id '%1' collides with an existing base signal id").arg(p.id));
            continue;
        }
        // Required: formula
        if (!entry["formula"] || !entry["formula"].IsScalar()) {
            pushErr(entryLine, p.id, QStringLiteral("expression '%1' missing required field 'formula'").arg(p.id));
            continue;
        }
        p.formula = QString::fromStdString(entry["formula"].as<std::string>());
        if (p.formula.trimmed().isEmpty()) {
            pushErr(entry["formula"].Mark().line + 1, p.id,
                    QStringLiteral("expression '%1' has empty formula").arg(p.id));
            continue;
        }
        // Optional: name, unit, description, type
        if (entry["name"] && entry["name"].IsScalar()) {
            p.name = QString::fromStdString(entry["name"].as<std::string>());
        } else {
            p.name = p.id;
        }
        if (entry["unit"] && entry["unit"].IsScalar()) {
            p.unit = QString::fromStdString(entry["unit"].as<std::string>());
        }
        if (entry["description"] && entry["description"].IsScalar()) {
            p.description = QString::fromStdString(entry["description"].as<std::string>());
        }
        if (entry["type"] && entry["type"].IsScalar()) {
            const auto t = parseOutputType(entry["type"].as<std::string>());
            if (!t) {
                pushErr(
                    entry["type"].Mark().line + 1, p.id,
                    QStringLiteral("expression '%1' has unknown output type (allowed: double, bool, int64)").arg(p.id));
                continue;
            }
            p.outputType = *t;
        }

        // Forbidden-function check (spec §3.1 reject-list).
        if (auto forbidden = firstForbiddenName(p.formula)) {
            pushErr(entry["formula"].Mark().line + 1, p.id,
                    QStringLiteral("expression '%1' formula uses forbidden function/keyword '%2' "
                                   "(allowed: arithmetic, comparison, logical, and the math built-ins "
                                   "min/max/abs/sqrt/log/log10/exp/sin/cos/tan/floor/ceil/round)")
                        .arg(p.id, *forbidden));
            continue;
        }

        // Source-id extraction from the formula text.
        p.sourceIds = extractSourceIds(p.formula);

        // exprtk compile preflight (catches operator-level errors).
        if (auto err = exprtkCompileError(p.formula, p.sourceIds)) {
            pushErr(entry["formula"].Mark().line + 1, p.id, QStringLiteral("expression '%1' %2").arg(p.id, *err));
            continue;
        }

        parsed.push_back(std::move(p));
    }

    // Cross-expression: source-id existence + type checks.
    std::unordered_set<QString> expressionIdSet;
    for (const auto& p : parsed) {
        expressionIdSet.insert(p.id);
    }
    for (const auto& p : parsed) {
        for (const auto& src : p.sourceIds) {
            if (expressionIdSet.count(src) > 0) {
                continue;  // a derived signal computed earlier in the set
            }
            const auto baseIt = baseSignalTypes.find(src);
            if (baseIt == baseSignalTypes.end()) {
                pushErr(p.line, p.id,
                        QStringLiteral("expression '%1' references unknown source signal '%2' "
                                       "(not in registry's available-signals list)")
                            .arg(p.id, src));
                continue;
            }
            if (baseIt->second == signalforge::decoder::SignalType::String) {
                pushErr(p.line, p.id,
                        QStringLiteral("expression '%1' references QString-typed source '%2' "
                                       "(QString sources are not supported in expressions)")
                            .arg(p.id, src));
            }
        }
    }

    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }

    // Build dependency graph: expression → its source IDs (only the
    // ones that ARE other expressions; base signals are leaves).
    std::unordered_map<QString, std::vector<QString>> graph;
    for (const auto& p : parsed) {
        graph[p.id];  // ensure node exists
        for (const auto& src : p.sourceIds) {
            if (expressionIdSet.count(src) > 0) {
                graph[p.id].push_back(src);
            }
        }
    }

    // Cycle detection.
    if (auto cyc = findCycle(graph)) {
        QStringList path;
        for (const auto& n : *cyc) {
            path.append(n);
        }
        const QString cycleStr = path.join(QStringLiteral(" -> "));
        pushErr(-1, QString{}, QStringLiteral("Expression set validation failed: cycle detected: %1").arg(cycleStr));
        return std::unexpected(std::move(errors));
    }

    // Topological sort. Kahn returns a valid ordering; for evaluation we
    // want sources-before-consumers, so start from in-degree-0 (which is
    // expressions whose sources are all base signals or already in
    // earlier topo positions).
    auto order = topologicalSort(graph, expressionIdSet);
    // Kahn's `order` is sources-before-consumers when in-degree is "I
    // have N expressions depending ON me". Our edges go expression ->
    // its-sources, so an expression's in-degree counts how many other
    // expressions depend on it as a source. To evaluate a then b where
    // b depends on a, we need a first. With our edge direction, a has
    // in-degree >= 1 (b points to it), b has in-degree 0. So Kahn's
    // order starts with b — that's the WRONG order. Reverse it to get
    // sources-before-consumers.
    std::reverse(order.begin(), order.end());

    ExpressionSet result;
    result.expressions.reserve(parsed.size());
    std::unordered_map<QString, std::size_t> idToIdx;
    for (std::size_t i = 0; i < parsed.size(); ++i) {
        idToIdx[parsed[i].id] = i;
    }
    for (const auto& id : order) {
        auto it = idToIdx.find(id);
        if (it == idToIdx.end()) {
            continue;
        }
        auto& p = parsed[it->second];
        try {
            result.expressions.emplace_back(p.id, p.name, p.unit, p.description, p.formula, p.outputType, p.sourceIds);
        } catch (const std::exception& e) {
            // Should not happen — validator's exprtk preflight already
            // succeeded. Log and bail.
            ExpressionValidationError err;
            err.filePath = virtualPath;
            err.lineNumber = p.line;
            err.expressionId = p.id;
            err.message = QStringLiteral("internal: Expression construction failed despite validator preflight: %1")
                              .arg(QString::fromUtf8(e.what()));
            std::vector<ExpressionValidationError> v;
            v.push_back(std::move(err));
            return std::unexpected(std::move(v));
        }
    }

    // Build baseSignalIds (union of all source IDs that are NOT
    // themselves expressions).
    std::unordered_set<QString> baseSeen;
    for (const auto& p : parsed) {
        for (const auto& src : p.sourceIds) {
            if (expressionIdSet.count(src) > 0) {
                continue;
            }
            if (baseSeen.insert(src).second) {
                result.baseSignalIds.push_back(src);
            }
        }
    }

    return result;
}

ExpressionValidationResult
ExpressionValidator::validateFile(const QString& yamlPath,
                                  const std::vector<signalforge::decoder::SignalMetadata>& availableSignals) {
    QFile f(yamlPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        std::vector<ExpressionValidationError> errors;
        ExpressionValidationError e;
        e.filePath = yamlPath;
        e.lineNumber = -1;
        e.message = QStringLiteral("cannot open file: %1").arg(f.errorString());
        errors.push_back(std::move(e));
        return std::unexpected(std::move(errors));
    }
    const QString content = QString::fromUtf8(f.readAll());
    return validateString(content, yamlPath, availableSignals);
}

}  // namespace signalforge::expression
