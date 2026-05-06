// src/expression/expression.cpp
//
// The `exprtk` header is included here (and only here) so the rest of
// the project never sees its ~700k SLOC of templates. PIMPL hides
// exprtk types behind `Expression::Impl`. Public callers see only
// QString / SignalValue.
//
// HALT trigger #2 (per plan §3) is enforced at this TU's compile time:
// if exprtk does not compile under C++23, the build fails here.
#include "expression/expression.hpp"

#include "observability/logging.hpp"

#include <QString>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// Suppress exprtk's known pre-C++23 noise (signed/unsigned comparison etc).
// We do not patch the upstream header.
// NOLINTBEGIN
#include <exprtk.hpp>
// NOLINTEND

namespace signalforge::expression {

namespace {

/// V1 base-function whitelist per spec §3.1. The validator (S3) text/AST-
/// searches for forbidden function names; the runtime parser settings
/// here block control-flow / assignment / string / local-var-def /
/// return / break-continue. Enforced together at the parse boundary.
constexpr std::array<const char*, 13> kBaseFuncWhitelist = {
    "min", "max", "abs", "sqrt", "log", "log10", "exp", "sin", "cos", "tan", "floor", "ceil", "round",
};

/// Apply spec §3.1 restricted-syntax settings to the parser. Only the
/// disable_* methods that take no args (or boolean) are used; per-base-
/// function disables (which take an enum) are deferred to the validator
/// where a textual / AST scan covers the §3.1 reject-list (`sgn`, `cot`,
/// statistical aggregates).
template <typename ParserT> void applyRestrictedSettings(ParserT& parser) {
    auto& s = parser.settings();

    // Reject control flow (`if`/`while`/`for`/`?:`/`switch`).
    s.disable_all_control_structures();

    // Reject local var definitions (`var x := ...`).
    s.disable_local_vardef();

    // Reject side-effecting assignment ops (=, +=, -=, etc.).
    s.disable_all_assignment_ops();

    // Note: exprtk 0.0.3's settings_store does not expose
    // disable_break_continue_statement / disable_return_statement /
    // disable_string_capabilities. Their effective disabling is covered
    // by `disable_all_control_structures` (which removes the loop/control
    // contexts that break/continue/return need). String capabilities are
    // not enabled by default on a `parser<double>` instance.
}

/// Convert a `SignalValue` to `double` per spec §3.3 type-promotion
/// rules. NaN-on-missing is the caller's responsibility (this helper
/// asserts the variant carries a numeric type).
[[nodiscard]] double promoteToDouble(const signalforge::decoder::SignalValue& v) {
    return std::visit(
        [](const auto& x) -> double {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, bool>) {
                return x ? 1.0 : 0.0;
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                if (std::abs(x) > (std::int64_t{1} << 52)) {
                    SF_LOG_WARN("expression: int64 source value {} exceeds 2^52; precision may be lost", x);
                }
                return static_cast<double>(x);
            } else if constexpr (std::is_same_v<T, double>) {
                return x;
            } else {
                // QString: validator rejects this path. Defensive NaN.
                return std::numeric_limits<double>::quiet_NaN();
            }
        },
        v);
}

/// Map a `double` result to the declared `ExpressionOutputType` per
/// spec §3.3.
[[nodiscard]] signalforge::decoder::SignalValue mapResult(double result, ExpressionOutputType outputType) {
    switch (outputType) {
    case ExpressionOutputType::Double:
        return signalforge::decoder::SignalValue{result};
    case ExpressionOutputType::Bool:
        return signalforge::decoder::SignalValue{result != 0.0};
    case ExpressionOutputType::Int64: {
        if (std::isnan(result) || std::isinf(result)) {
            SF_LOG_WARN("expression: result {} cannot be converted to int64", result);
            return signalforge::decoder::SignalValue{static_cast<std::int64_t>(0)};
        }
        const double rounded = std::round(result);
        if (rounded > static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
            rounded < static_cast<double>(std::numeric_limits<std::int64_t>::min())) {
            SF_LOG_WARN("expression: rounded result {} out of int64 range", rounded);
            return signalforge::decoder::SignalValue{static_cast<std::int64_t>(0)};
        }
        return signalforge::decoder::SignalValue{static_cast<std::int64_t>(rounded)};
    }
    }
    return signalforge::decoder::SignalValue{result};
}

}  // namespace

struct Expression::Impl {
    QString id;
    QString name;
    QString unit;
    std::optional<QString> description;
    QString formula;
    ExpressionOutputType outputType = ExpressionOutputType::Double;
    std::vector<QString> sourceSignalIds;

    // exprtk state. `slots` holds a `double` per source signal in
    // `sourceSignalIds` order. The symbol table binds each source
    // name to its slot; evaluate() writes the slot before calling
    // `compiled.value()`. `nameToSlot` caches the index lookup for
    // the hot path.
    exprtk::symbol_table<double> symbols;
    exprtk::expression<double> compiled;
    std::vector<double> sourceSlots;
    std::unordered_map<QString, std::size_t> nameToSlot;
};

Expression::Expression(QString id, QString name, QString unit, std::optional<QString> description, QString formula,
                       ExpressionOutputType outputType, std::vector<QString> sourceSignalIds)
    : impl_(std::make_unique<Impl>()) {
    impl_->id = std::move(id);
    impl_->name = std::move(name);
    impl_->unit = std::move(unit);
    impl_->description = std::move(description);
    impl_->formula = std::move(formula);
    impl_->outputType = outputType;
    impl_->sourceSignalIds = std::move(sourceSignalIds);

    // Allocate slots and register each source name with the symbol table.
    impl_->sourceSlots.assign(impl_->sourceSignalIds.size(), 0.0);
    for (std::size_t i = 0; i < impl_->sourceSignalIds.size(); ++i) {
        const auto& sigId = impl_->sourceSignalIds[i];
        const std::string nameStd = sigId.toStdString();
        impl_->symbols.add_variable(nameStd, impl_->sourceSlots[i]);
        impl_->nameToSlot.emplace(sigId, i);
    }
    impl_->symbols.add_constants();  // pi, epsilon, inf — read-only literals
    impl_->compiled.register_symbol_table(impl_->symbols);

    exprtk::parser<double> parser;
    applyRestrictedSettings(parser);

    if (!parser.compile(impl_->formula.toStdString(), impl_->compiled)) {
        // The validator should catch user-facing errors before construction.
        // If we get here, the formula is malformed and the validator missed it
        // (or this is a direct-construct path without validator). Throw with
        // exprtk's error text for diagnostic.
        std::string err = "exprtk compile failed for '" + impl_->formula.toStdString() + "':";
        for (std::size_t i = 0; i < parser.error_count(); ++i) {
            const auto e = parser.get_error(i);
            err += "\n  [";
            err += std::to_string(e.token.position);
            err += "] ";
            err += e.diagnostic;
        }
        throw std::runtime_error(err);
    }
}

Expression::~Expression() = default;

Expression::Expression(Expression&&) noexcept = default;
Expression& Expression::operator=(Expression&&) noexcept = default;

const QString& Expression::id() const noexcept {
    return impl_->id;
}
const QString& Expression::name() const noexcept {
    return impl_->name;
}
const QString& Expression::unit() const noexcept {
    return impl_->unit;
}
const std::optional<QString>& Expression::description() const noexcept {
    return impl_->description;
}
const QString& Expression::formula() const noexcept {
    return impl_->formula;
}
ExpressionOutputType Expression::outputType() const noexcept {
    return impl_->outputType;
}
const std::vector<QString>& Expression::sourceSignalIds() const noexcept {
    return impl_->sourceSignalIds;
}

signalforge::decoder::SignalValue
Expression::evaluate(const std::vector<std::pair<QString, signalforge::decoder::SignalValue>>& sources) const {
    // Default any unset source slot to NaN so missing sources propagate
    // (M8 chart will render gaps; spec §4.7).
    for (auto& slot : impl_->sourceSlots) {
        slot = std::numeric_limits<double>::quiet_NaN();
    }
    for (const auto& [sigId, value] : sources) {
        auto it = impl_->nameToSlot.find(sigId);
        if (it == impl_->nameToSlot.end()) {
            continue;  // not a source for this expression
        }
        impl_->sourceSlots[it->second] = promoteToDouble(value);
    }
    const double result = impl_->compiled.value();
    return mapResult(result, impl_->outputType);
}

signalforge::decoder::SignalMetadata Expression::derivedSignalMetadata() const {
    signalforge::decoder::SignalMetadata meta;
    meta.id = impl_->id;
    meta.name = impl_->name;
    meta.unit = impl_->unit;
    meta.description = impl_->description;
    switch (impl_->outputType) {
    case ExpressionOutputType::Double:
        meta.type = signalforge::decoder::SignalType::Double;
        break;
    case ExpressionOutputType::Bool:
        meta.type = signalforge::decoder::SignalType::Bool;
        break;
    case ExpressionOutputType::Int64:
        meta.type = signalforge::decoder::SignalType::Int64;
        break;
    }
    return meta;
}

}  // namespace signalforge::expression
