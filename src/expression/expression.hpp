// src/expression/expression.hpp
#pragma once

#include "decode/decoder_interface.hpp"  // For SignalValue, SignalType, SignalMetadata

#include <QString>
#include <chrono>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace signalforge::expression {

/// Output type tag for an expression result.
enum class ExpressionOutputType {
    Double,
    Bool,
    Int64,
};

/// One compiled expression.
///
/// Pure C++ (not a `QObject`); construction compiles the formula via
/// `exprtk` (PIMPL hides exprtk types from the public header) and
/// caches dependency metadata. `evaluate()` substitutes source
/// values into the bound symbol table and runs the parsed
/// expression in-place.
///
/// Validation (yaml syntax, restricted-syntax whitelist, source-id
/// existence, type compatibility, cycle detection) is the
/// responsibility of `ExpressionValidator`. By the time an
/// `Expression` is constructed via the validator's
/// `ExpressionSet`, the formula is guaranteed compilable.
///
/// Freeze scope: this header is frozen at M7 close. See spec §6.1.
class Expression {
public:
    /// Construct from validated parameters. Compiles the formula via
    /// `exprtk`; throws on internal compilation failure (validator
    /// should catch all user-facing errors before this).
    Expression(QString id, QString name, QString unit, std::optional<QString> description, QString formula,
               ExpressionOutputType outputType, std::vector<QString> sourceSignalIds);

    ~Expression();

    Expression(const Expression&) = delete;
    Expression& operator=(const Expression&) = delete;
    Expression(Expression&&) noexcept;
    Expression& operator=(Expression&&) noexcept;

    /// The expression's identifier (matches the derived signal's ID).
    [[nodiscard]] const QString& id() const noexcept;
    [[nodiscard]] const QString& name() const noexcept;
    [[nodiscard]] const QString& unit() const noexcept;
    [[nodiscard]] const std::optional<QString>& description() const noexcept;
    [[nodiscard]] const QString& formula() const noexcept;
    [[nodiscard]] ExpressionOutputType outputType() const noexcept;

    /// Source signal IDs this expression depends on. Order does not
    /// matter for evaluation (substituted by name).
    [[nodiscard]] const std::vector<QString>& sourceSignalIds() const noexcept;

    /// Evaluate with the provided source-value map (signalId →
    /// `SignalValue`). Returns the result as a `SignalValue` matching
    /// `outputType()`. Throws on runtime error (div-by-zero, domain
    /// error) — caller catches.
    [[nodiscard]] signalforge::decoder::SignalValue
    evaluate(const std::vector<std::pair<QString, signalforge::decoder::SignalValue>>& sources) const;

    /// Build the `SignalMetadata` for the derived signal this
    /// expression produces. Used during registration with
    /// `SignalBufferRegistry`.
    [[nodiscard]] signalforge::decoder::SignalMetadata derivedSignalMetadata() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// A validated, topologically-sorted collection of expressions.
///
/// Returned by `ExpressionValidator`. Consumed by
/// `ExpressionEngine::setExpressions(...)`.
struct ExpressionSet {
    std::vector<Expression> expressions;  ///< Topologically sorted.
    std::vector<QString> baseSignalIds;   ///< Union of all source IDs (for prefetch).
};

}  // namespace signalforge::expression
