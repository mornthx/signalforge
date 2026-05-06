// src/expression/expression_validator.cpp
#include "expression/expression_validator.hpp"

#include <utility>

namespace signalforge::expression {

ExpressionValidationResult
ExpressionValidator::validateFile(const QString& /*yamlPath*/,
                                  const std::vector<signalforge::decoder::SignalMetadata>& /*availableSignals*/) {
    // S3 wires the full yaml + cycle + type validation pipeline.
    std::vector<ExpressionValidationError> errs;
    ExpressionValidationError e;
    e.lineNumber = -1;
    e.message = QStringLiteral("ExpressionValidator::validateFile is not yet implemented (S1 stub)");
    errs.push_back(std::move(e));
    return std::unexpected(std::move(errs));
}

ExpressionValidationResult
ExpressionValidator::validateString(const QString& /*yamlContent*/, const QString& /*virtualPath*/,
                                    const std::vector<signalforge::decoder::SignalMetadata>& /*availableSignals*/) {
    // S3 wires the full validation pipeline.
    std::vector<ExpressionValidationError> errs;
    ExpressionValidationError e;
    e.lineNumber = -1;
    e.message = QStringLiteral("ExpressionValidator::validateString is not yet implemented (S1 stub)");
    errs.push_back(std::move(e));
    return std::unexpected(std::move(errs));
}

}  // namespace signalforge::expression
