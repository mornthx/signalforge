// src/expression/expression_registrar.hpp
#pragma once

#include "buffer/signal_buffer_registry.hpp"
#include "expression/expression_engine.hpp"
#include "expression/expression_validator.hpp"

#include <QString>
#include <QStringList>
#include <vector>

namespace signalforge::expression {

/// App-level lifecycle hook that loads expression yaml files,
/// validates them against the base-signal catalog held by the
/// `SignalBufferRegistry`, and hands the result to an
/// `ExpressionEngine`. On any validation failure, all errors are
/// logged with file/line/expression-id context and the engine
/// remains stopped.
///
/// Multi-file expression sets are merged into a single virtual yaml
/// document before validation, so the validator's cycle detection
/// runs across all files (per plan §S5).
///
/// Lifetime: caller owns all three references; the registrar is
/// non-owning. Typical usage from `MainWindow` or `main.cpp`:
///
///   ExpressionRegistrar registrar(registry, engine, paths);
///   registrar.loadAndStart();
///
/// Freeze scope: the public API of this class freezes at M7 close
/// alongside the other M7 surface classes.
class ExpressionRegistrar {
public:
    ExpressionRegistrar(signalforge::buffer::SignalBufferRegistry& registry, ExpressionEngine& engine,
                        std::vector<QString> yamlPaths);

    /// Load all yaml files, validate the merged set against the
    /// registry's base signals, and on success start the engine.
    /// Returns true on success (engine started); false on any
    /// validation failure (engine left stopped). Errors are logged
    /// via SF_LOG_ERROR / SF_LOG_WARN.
    bool loadAndStart();

    /// Last validation result (errors). Empty on success or before
    /// `loadAndStart` is called.
    [[nodiscard]] const std::vector<ExpressionValidationError>& lastErrors() const noexcept;

private:
    signalforge::buffer::SignalBufferRegistry* registry_ = nullptr;
    ExpressionEngine* engine_ = nullptr;
    std::vector<QString> yamlPaths_;
    std::vector<ExpressionValidationError> lastErrors_;
};

}  // namespace signalforge::expression
