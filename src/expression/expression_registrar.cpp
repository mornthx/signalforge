// src/expression/expression_registrar.cpp
#include "expression/expression_registrar.hpp"

#include "buffer/signal_buffer.hpp"
#include "observability/logging.hpp"

#include <QFile>
#include <QString>
#include <sstream>
#include <utility>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace signalforge::expression {

namespace {

[[nodiscard]] std::vector<signalforge::decoder::SignalMetadata>
collectAvailableSignals(const signalforge::buffer::SignalBufferRegistry& registry) {
    std::vector<signalforge::decoder::SignalMetadata> out;
    const auto ids = registry.signalIds();
    out.reserve(static_cast<std::size_t>(ids.size()));
    for (const auto& id : ids) {
        if (auto* buf = registry.bufferFor(id)) {
            out.push_back(buf->metadata());
        }
    }
    return out;
}

/// Read yaml files and merge their `expressions` sequences into a
/// single virtual document. Returns the merged yaml string on success
/// or an empty optional + sets `errors` on per-file IO/parse failure.
[[nodiscard]] std::optional<std::string> mergeYamlFiles(const std::vector<QString>& paths,
                                                        std::vector<ExpressionValidationError>& errors) {
    YAML::Node merged;
    merged["schema_version"] = 1;
    YAML::Node mergedExprs(YAML::NodeType::Sequence);

    for (const auto& path : paths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            ExpressionValidationError e;
            e.filePath = path;
            e.lineNumber = -1;
            e.message = QStringLiteral("cannot open file: %1").arg(f.errorString());
            errors.push_back(std::move(e));
            return std::nullopt;
        }
        const std::string content = f.readAll().toStdString();
        YAML::Node fileNode;
        try {
            fileNode = YAML::Load(content);
        } catch (const YAML::Exception& e) {
            ExpressionValidationError err;
            err.filePath = path;
            err.lineNumber = e.mark.line + 1;
            err.message = QStringLiteral("yaml parse error: %1").arg(QString::fromStdString(e.msg));
            errors.push_back(std::move(err));
            return std::nullopt;
        }
        if (!fileNode || !fileNode.IsMap()) {
            ExpressionValidationError err;
            err.filePath = path;
            err.lineNumber = -1;
            err.message = QStringLiteral("yaml root must be a mapping");
            errors.push_back(std::move(err));
            return std::nullopt;
        }
        const auto exprsNode = fileNode["expressions"];
        if (!exprsNode || !exprsNode.IsSequence()) {
            ExpressionValidationError err;
            err.filePath = path;
            err.lineNumber = -1;
            err.message = QStringLiteral("file is missing top-level 'expressions' sequence");
            errors.push_back(std::move(err));
            return std::nullopt;
        }
        for (const auto& expr : exprsNode) {
            mergedExprs.push_back(expr);
        }
    }

    merged["expressions"] = mergedExprs;
    std::stringstream ss;
    ss << merged;
    return ss.str();
}

}  // namespace

ExpressionRegistrar::ExpressionRegistrar(signalforge::buffer::SignalBufferRegistry& registry, ExpressionEngine& engine,
                                         std::vector<QString> yamlPaths)
    : registry_(&registry), engine_(&engine), yamlPaths_(std::move(yamlPaths)) {}

bool ExpressionRegistrar::loadAndStart() {
    lastErrors_.clear();

    if (yamlPaths_.empty()) {
        SF_LOG_WARN("expression: ExpressionRegistrar invoked with no yaml paths; engine not started");
        return false;
    }

    auto mergedOpt = mergeYamlFiles(yamlPaths_, lastErrors_);
    if (!mergedOpt) {
        for (const auto& err : lastErrors_) {
            SF_LOG_ERROR("expression: registrar load error in '{}' line {}: {}", err.filePath.toStdString(),
                         err.lineNumber, err.message.toStdString());
        }
        return false;
    }

    const auto availableSignals = collectAvailableSignals(*registry_);

    auto result = ExpressionValidator::validateString(QString::fromStdString(*mergedOpt), QStringLiteral("[merged]"),
                                                      availableSignals);
    if (!result.has_value()) {
        lastErrors_ = result.error();
        for (const auto& err : lastErrors_) {
            SF_LOG_ERROR("expression: validation failed for '{}' line {}: expr='{}': {}", err.filePath.toStdString(),
                         err.lineNumber, err.expressionId.toStdString(), err.message.toStdString());
        }
        return false;
    }

    engine_->setExpressions(std::move(*result));
    engine_->start();
    SF_LOG_INFO("expression: registered {} expressions across {} yaml file(s); engine started",
                engine_->expressionCount(), yamlPaths_.size());
    return true;
}

const std::vector<ExpressionValidationError>& ExpressionRegistrar::lastErrors() const noexcept {
    return lastErrors_;
}

}  // namespace signalforge::expression
