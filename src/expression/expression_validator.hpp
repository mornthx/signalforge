// src/expression/expression_validator.hpp
#pragma once

#include "decode/decoder_interface.hpp"  // For SignalMetadata
#include "expression/expression.hpp"

#include <QString>
#include <expected>
#include <vector>

namespace signalforge::expression {

/// One validation error. Errors include yaml syntax issues, restricted-
/// syntax (whitelist) violations, type mismatches, missing source
/// signals, and cycle-detection results.
struct ExpressionValidationError {
    QString filePath;
    int lineNumber = -1;   ///< -1 if not mappable.
    QString expressionId;  ///< Empty if pre-expression error.
    QString message;
};

using ExpressionValidationResult = std::expected<ExpressionSet, std::vector<ExpressionValidationError>>;

/// Loads, validates, and compiles an expression yaml file.
///
/// Validation order (per spec §4.3 implementation steps):
///   1. yaml syntax (yaml-cpp parse)
///   2. Top-level keys (`schema_version`, `expressions`) present
///   3. Per-expression: required fields + uniqueness + base-id collisions
///   4. exprtk compilation + restricted-syntax whitelist enforcement
///   5. Source-id existence in `availableSignals`
///   6. Source-type compatibility (no QString-typed source)
///   7. Output-type compatibility hints
///   8. Cycle detection (DFS three-color)
///   9. Topological sort → final `ExpressionSet`
///
/// Freeze scope: this header is frozen at M7 close. See spec §6.1.
class ExpressionValidator {
public:
    /// Validate a yaml file. Returns a topologically-sorted
    /// `ExpressionSet` on success, or a list of errors. Errors include
    /// syntax errors, type errors, cycle detection, and out-of-whitelist
    /// syntax.
    ///
    /// `availableSignals` is used to validate that source signal IDs
    /// exist in the registry. Pass the catalog of base signals from
    /// `SignalBufferRegistry` after all decoders have registered.
    [[nodiscard]] static ExpressionValidationResult
    validateFile(const QString& yamlPath, const std::vector<signalforge::decoder::SignalMetadata>& availableSignals);

    /// Validate yaml content directly (for testing).
    [[nodiscard]] static ExpressionValidationResult
    validateString(const QString& yamlContent, const QString& virtualPath,
                   const std::vector<signalforge::decoder::SignalMetadata>& availableSignals);
};

}  // namespace signalforge::expression
