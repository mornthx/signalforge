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
