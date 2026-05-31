// src/query/filter_expr.hpp
#pragma once

#include <QString>
#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace signalforge::query {

/// A single field's value, as seen by the filter engine. Decoupled from
/// `decoder::SignalValue` so the engine stays a dependency-free leaf reusable
/// by both the parsed-signals table (M30) and the raw-packet view (M31).
using FieldValue = std::variant<double, bool, QString>;

/// Resolves a field name to its value for one row, or nullopt if the row has
/// no such field. The engine treats a missing field as a non-match.
using FieldLookup = std::function<std::optional<FieldValue>(const QString&)>;

/// A parsed Wireshark-style display filter, e.g.
/// `source == udp:rig && value > 20 || name contains temp`.
///
/// Grammar (recursive descent):
///   expr    := orExpr
///   orExpr  := andExpr ( "||" andExpr )*
///   andExpr := unary  ( "&&" unary  )*
///   unary   := "!" unary | primary
///   primary := "(" expr ")" | comparison
///   compare := IDENT OP literal
///   OP      := == | != | < | <= | > | >= | contains
///   literal := number | "quoted string" | bareword | true | false
///
/// Comparisons coerce by the literal's kind: a numeric literal forces a
/// numeric compare (the field is coerced to double); otherwise a
/// case-insensitive string compare. `contains` is always substring.
class FilterExpr {
public:
    /// AST node (opaque to callers). Defined in the .cpp.
    struct Node;

    /// Outcome of `parse` (defined after the class — it holds a FilterExpr,
    /// which must be complete first).
    struct ParseResult;

    FilterExpr() = default;  ///< Empty filter — matches every row.

    /// Parse `source`. Whitespace-only / empty source yields an empty filter
    /// (matches all). Never throws.
    [[nodiscard]] static ParseResult parse(const QString& source);

    /// True if `lookup`'s row satisfies the filter. An empty filter is always
    /// true.
    [[nodiscard]] bool matches(const FieldLookup& lookup) const;

    /// Whether this is the match-all empty filter.
    [[nodiscard]] bool isEmpty() const {
        return root_ == nullptr;
    }

private:
    explicit FilterExpr(std::shared_ptr<const Node> root) : root_(std::move(root)) {}
    std::shared_ptr<const Node> root_;
};

/// Outcome of `FilterExpr::parse`: on success `expr` holds a usable filter; on
/// failure `error` is non-empty and `errorPos` is the byte offset of the problem.
struct FilterExpr::ParseResult {
    std::optional<FilterExpr> expr;
    QString error;
    int errorPos = -1;
    [[nodiscard]] bool ok() const {
        return error.isEmpty();
    }
};

}  // namespace signalforge::query
