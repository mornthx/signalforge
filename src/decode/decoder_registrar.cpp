#include "decode/decoder_registrar.hpp"

#include "decode/schema_decoder.hpp"
#include "decode/schema_validator.hpp"
#include "observability/logging.hpp"
#include "pipeline/frame_pipeline.hpp"
#include "pipeline/pipeline_manager.hpp"

#include <utility>

namespace signalforge::decoder {

DecoderRegistrar::DecoderRegistrar(signalforge::pipeline::PipelineManager* manager,
                                   std::unordered_map<QString, QString> driverTypeToSchemaPath,
                                   std::shared_ptr<SignalValueSink> defaultSink, QObject* parent)
    : QObject(parent), manager_(manager), driverTypeToSchemaPath_(std::move(driverTypeToSchemaPath)),
      defaultSink_(std::move(defaultSink)) {
    if (manager_ == nullptr) {
        SF_LOG_ERROR("DecoderRegistrar: constructed with null PipelineManager; no signals will fire");
        return;
    }
    connect(manager_, &signalforge::pipeline::PipelineManager::pipelineAttached, this,
            &DecoderRegistrar::onPipelineAttached);
    connect(manager_, &signalforge::pipeline::PipelineManager::pipelineDetached, this,
            &DecoderRegistrar::onPipelineDetached);
}

DecoderRegistrar::~DecoderRegistrar() = default;

std::size_t DecoderRegistrar::decoderCount() const {
    std::lock_guard lock(decoderMutex_);
    return decoders_.size();
}

QString DecoderRegistrar::driverTypeOf(const QString& driverId) {
    const int colon = driverId.indexOf(QLatin1Char(':'));
    if (colon < 0) {
        return {};
    }
    return driverId.left(colon);
}

void DecoderRegistrar::setSchemaForDriverType(const QString& driverType, const QString& schemaPath) {
    // Per ADR-008: the M5-frozen ctor map is now mutable via this
    // additive method. ConnectionManager calls this on YAML-load /
    // add / edit / remove. Empty schemaPath removes the entry.
    //
    // No reactive re-keying of decoders_: existing decoders for
    // `driverType` keep their original schema until they detach.
    // New pipelineAttached events use the updated map.
    std::lock_guard lock(decoderMutex_);
    if (schemaPath.isEmpty()) {
        driverTypeToSchemaPath_.erase(driverType);
        SF_LOG_INFO("DecoderRegistrar: schema cleared for driver type '{}'", driverType.toStdString());
    } else {
        driverTypeToSchemaPath_[driverType] = schemaPath;
        SF_LOG_INFO("DecoderRegistrar: schema for driver type '{}' set to '{}'", driverType.toStdString(),
                    schemaPath.toStdString());
    }
}

void DecoderRegistrar::onPipelineAttached(const QString& driverId, signalforge::pipeline::FramePipeline* pipeline) {
    if (pipeline == nullptr) {
        SF_LOG_ERROR("DecoderRegistrar[{}]: pipelineAttached delivered a null pipeline", driverId.toStdString());
        return;
    }

    const QString type = driverTypeOf(driverId);
    if (type.isEmpty()) {
        SF_LOG_INFO("DecoderRegistrar[{}]: driverId has no type prefix; no decoder attached", driverId.toStdString());
        return;
    }

    QString schemaPath;
    {
        // Lock the map read — `setSchemaForDriverType` (ADR-008
        // additive API) may mutate this map at runtime.
        std::lock_guard lock(decoderMutex_);
        const auto it = driverTypeToSchemaPath_.find(type);
        if (it == driverTypeToSchemaPath_.end() || it->second.isEmpty()) {
            SF_LOG_INFO("DecoderRegistrar[{}]: no schema configured for driver type '{}'; no decoder attached",
                        driverId.toStdString(), type.toStdString());
            return;
        }
        schemaPath = it->second;
    }

    const auto result = SchemaValidator::validateFile(schemaPath);
    if (!result.has_value()) {
        for (const auto& e : result.error()) {
            SF_LOG_ERROR("DecoderRegistrar[{}]: schema validation failed at {}:{} {}: {}", driverId.toStdString(),
                         e.filePath.toStdString(), e.lineNumber, e.fieldPath.toStdString(), e.message.toStdString());
        }
        return;
    }

    auto decoder = std::make_shared<SchemaDecoder>(*result, driverId);
    if (defaultSink_) {
        decoder->setSignalSink(defaultSink_);
    }
    pipeline->addSink(decoder);

    {
        std::lock_guard lock(decoderMutex_);
        decoders_[driverId.toStdString()] = decoder;
    }
    SF_LOG_INFO("DecoderRegistrar[{}]: decoder attached using schema '{}'", driverId.toStdString(),
                schemaPath.toStdString());
}

void DecoderRegistrar::onPipelineDetached(const QString& driverId) {
    std::shared_ptr<DecoderInterface> released;
    {
        std::lock_guard lock(decoderMutex_);
        auto it = decoders_.find(driverId.toStdString());
        if (it == decoders_.end()) {
            return;
        }
        released = std::move(it->second);
        decoders_.erase(it);
    }
    SF_LOG_INFO("DecoderRegistrar[{}]: decoder released after pipeline detach", driverId.toStdString());
    // `released` falls out of scope here; SchemaDecoder destructor unregisters
    // the sink cleanly.
}

}  // namespace signalforge::decoder
