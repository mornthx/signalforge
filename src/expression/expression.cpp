// src/expression/expression.cpp
//
// S1 deliverable. The `exprtk` header is included here (and only here)
// so the rest of the project never sees its ~700k SLOC of templates.
// PIMPL hides exprtk types behind `Expression::Impl`. Public callers
// see only QString / SignalValue.
//
// HALT trigger #2 (per plan §3) is enforced at this TU's compile time:
// if exprtk does not compile under C++23, the build fails here.
#include "expression/expression.hpp"

// ----------------------------------------------------------------------------
// exprtk preflight + PIMPL holder.
//
// exprtk is a single-header library (`build/<preset>/_deps/exprtk-src/exprtk.hpp`)
// pinned at 0.0.3 in `cmake/dependencies.cmake`. We instantiate
// `parser<double>`, `expression<double>`, and `symbol_table<double>` so the
// linker emits their template definitions, validating the version
// compiles and links under C++23 (HALT trigger #2).
// ----------------------------------------------------------------------------

// Suppress exprtk's known pre-C++23 noise (signed/unsigned comparison etc).
// We do not patch the upstream header.
// NOLINTBEGIN
#include <exprtk.hpp>
// NOLINTEND

#include <utility>

namespace signalforge::expression {

struct Expression::Impl {
    QString id;
    QString name;
    QString unit;
    std::optional<QString> description;
    QString formula;
    ExpressionOutputType outputType = ExpressionOutputType::Double;
    std::vector<QString> sourceSignalIds;

    // exprtk state (unused in S1 stub; populated in S2). Kept here so the
    // PIMPL has a real exprtk site for the preflight gate.
    exprtk::symbol_table<double> symbols;
    exprtk::expression<double> compiled;
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
    // S2 wires the symbol table + parser + per-source slots.
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
Expression::evaluate(const std::vector<std::pair<QString, signalforge::decoder::SignalValue>>& /*sources*/) const {
    // S2 / S4 wire the evaluation path. S1 returns a default value matching
    // the declared output type so callers in tests can sanity-check the
    // construction path without exercising real exprtk evaluation.
    switch (impl_->outputType) {
    case ExpressionOutputType::Double:
        return signalforge::decoder::SignalValue{0.0};
    case ExpressionOutputType::Bool:
        return signalforge::decoder::SignalValue{false};
    case ExpressionOutputType::Int64:
        return signalforge::decoder::SignalValue{static_cast<std::int64_t>(0)};
    }
    return signalforge::decoder::SignalValue{0.0};
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
