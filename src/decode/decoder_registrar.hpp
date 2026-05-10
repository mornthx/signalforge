#pragma once

#include "decode/decoder_interface.hpp"

#include <QObject>
#include <QString>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace signalforge::pipeline {
class FramePipeline;
class PipelineManager;
}  // namespace signalforge::pipeline

namespace signalforge::decoder {

/// Listens on `PipelineManager::pipelineAttached` and registers a
/// `SchemaDecoder` as a sink for each newly-attached pipeline whose driver
/// type appears in the configured map.
///
/// For M5, the driver-type → schema-path map is hard-coded by the application.
/// M9 replaces this with per-connection user selection driven by the
/// Connection Manager UI.
///
/// Driver-type extraction: the registrar splits each `driverId` on `:` and
/// uses the prefix as the type key (`serial:0` → `serial`).  Unknown types
/// or missing/empty schema paths log INFO and are skipped silently — they
/// are not errors, since not every driver in M5 has a schema yet.
///
/// Lifetime / threading:
/// - The registrar is a QObject and lives on the same thread as
///   `PipelineManager` (typically the UI / event loop thread).  Signal
///   delivery is automatic via Qt's connection mechanism.
/// - Per spec §7.6, the `pipelineAttached` handler must complete in well
///   under 100 ms on the receiving thread.  For typical schemas, validate +
///   construct is O(milliseconds).
class DecoderRegistrar : public QObject {
    Q_OBJECT

public:
    /// `manager` must outlive the registrar. `driverTypeToSchemaPath` maps
    /// e.g. `"serial"` → `"examples/schemas/temperature_sensor.yaml"`.
    /// An entry with an empty path is treated as "no decoder for this type".
    /// `defaultSink` is attached to every decoder constructed by this
    /// registrar; pass `nullptr` to skip wiring a sink (decoders will still
    /// register with pipelines but produce no signals until a sink is
    /// attached externally).
    DecoderRegistrar(signalforge::pipeline::PipelineManager* manager,
                     std::unordered_map<QString, QString> driverTypeToSchemaPath,
                     std::shared_ptr<SignalValueSink> defaultSink, QObject* parent = nullptr);
    ~DecoderRegistrar() override;

    DecoderRegistrar(const DecoderRegistrar&) = delete;
    DecoderRegistrar& operator=(const DecoderRegistrar&) = delete;

    /// Number of decoders this registrar currently has attached.  Useful
    /// for tests and the performance panel.
    [[nodiscard]] std::size_t decoderCount() const;

    /// Returns the driver type prefix of `driverId` (everything before the
    /// first `:`).  Returns an empty QString if the id has no `:`.
    [[nodiscard]] static QString driverTypeOf(const QString& driverId);

    /// Update the schema path for a given driver type at runtime.
    ///
    /// Affects pipelines attached **after** this call; existing
    /// decoders for `driverType` are NOT re-keyed (they continue to
    /// serve their original schema until they detach). Empty
    /// `schemaPath` removes the mapping (matches "no decoder for this
    /// type" semantics from the M5 ctor map).
    ///
    /// Thread-safe — protected by the existing `decoderMutex_`.
    ///
    /// **Added in M13 S7 (ADR-008).** M5 spec §4.6 deferred the
    /// per-connection schema selection mechanism to M9; M9
    /// implemented the YAML persistence + dialog UI but did not wire
    /// runtime updates. This method closes the gap. See
    /// `docs/architecture/decisions/ADR-008-decoder-registrar-runtime-schema.md`.
    ///
    /// Known V1.0 limitation: the map is per-driver-type, not per-
    /// connection-id. Multi-connection-same-type users see "last
    /// config wins" for new pipeline attachments.
    void setSchemaForDriverType(const QString& driverType, const QString& schemaPath);

private slots:
    void onPipelineAttached(const QString& driverId, signalforge::pipeline::FramePipeline* pipeline);
    void onPipelineDetached(const QString& driverId);

private:
    signalforge::pipeline::PipelineManager* manager_ = nullptr;
    std::unordered_map<QString, QString> driverTypeToSchemaPath_;
    std::shared_ptr<SignalValueSink> defaultSink_;

    mutable std::mutex decoderMutex_;
    std::unordered_map<std::string, std::shared_ptr<DecoderInterface>> decoders_;
};

}  // namespace signalforge::decoder
