// src/decode/decoder_interface.hpp
#pragma once

#include "frame/raw_frame.hpp"
#include "pipeline/frame_sink.hpp"

#include <QString>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace signalforge::decoder {

/// Value type of a decoded signal. M6 stores these; M7 evaluates
/// expressions over numeric variants; M8 displays them in charts.
///
/// Storage guidance for M6:
/// - bool: single bit (can pack 64 per uint64)
/// - int64_t: 8 bytes
/// - double: 8 bytes
/// - QString: pointer to shared storage (QString is ref-counted)
///
/// Expression semantics for M7:
/// - bool auto-converts to double (0.0 / 1.0)
/// - int64_t auto-converts to double (mantissa is 52 bits; values > 2^52
///   lose precision — M7 logs warn at registration if an expression uses
///   an int64 source whose current/recent values exceed 2^52)
/// - QString variables cannot be used in expressions — M7 rejects at
///   registration time with a clear error.
using SignalValue = std::variant<bool, std::int64_t, double, QString>;

/// Which variant the schema declares a signal to be.
enum class SignalType {
    Bool,
    Int64,
    Double,
    String,
};

/// Per-signal metadata. One SignalMetadata per signal emitted by this
/// decoder. Content is stable across the decoder's lifetime.
struct SignalMetadata {
    QString id;       ///< Unique identifier, stable across app run. "<driverId>/<fieldName>" convention.
    QString name;     ///< Human-readable; from yaml `name` field.
    QString unit;     ///< SI or engineering unit; from yaml `unit` field.
    SignalType type;  ///< Which variant this signal uses.
    std::optional<QString> description;  ///< Optional from yaml.
    std::optional<double> scale;         ///< Optional linear transform: raw * scale + offset = value.
    std::optional<double> offset;        ///< Optional.
};

/// Downstream consumer of decoded signals.
/// M5 provides LoggingSignalValueSink for test/stub use.
/// M6 provides the real SignalBuffer-backed implementation.
///
/// Freeze scope: this class is frozen at M5 close. Modifications
/// post-freeze require a new ADR.
class SignalValueSink {
public:
    virtual ~SignalValueSink() = default;

    /// Called for each decoded signal value.
    /// `timestamp` is the RawFrame.recvAt from the origin frame.
    /// `signalId` matches SignalMetadata.id of a registered signal.
    /// `value` is the variant with data.
    ///
    /// Thread affinity: called on the pipeline's thread (same thread
    /// as the Decoder's FrameSink::onFrame that produced this value).
    virtual void onSignal(std::chrono::steady_clock::time_point timestamp, const QString& signalId,
                          const SignalValue& value) = 0;

    /// Called once per decoder at registration time to publish the
    /// decoder's signal catalog. Consumer may use this to pre-allocate
    /// buffers, create UI bindings, etc.
    ///
    /// NOTE: parameter is named `signalsList` rather than `signals` to
    /// avoid collision with Qt's `signals` access-specifier macro (which
    /// expands to `public` under default QT_KEYWORDS). The M5 spec §4.1
    /// reference used `signals` verbatim; deviation logged in
    /// `.claude/M5-concerns.md`.
    virtual void onSignalsRegistered(const QString& driverId, const std::vector<SignalMetadata>& signalsList) {
        (void)driverId;
        (void)signalsList;
    }

    /// Called when the decoder is detaching (pipeline destroyed).
    /// Consumer should mark signals as "stream ended".
    virtual void onSignalsUnregistered(const QString& driverId) {
        (void)driverId;
    }
};

/// Abstract decoder: a FrameSink that translates RawFrames into SignalValues.
///
/// A decoder is registered with a FramePipeline via addSink(). On each
/// incoming frame, the decoder's onFrame (inherited from FrameSink)
/// parses the frame and calls setSignalSink's onSignal for each field.
///
/// Thread affinity: onFrame runs on the pipeline's thread; onSignal
/// callbacks happen on the same thread.
///
/// Freeze scope: this class + SignalValueSink + SignalValue + SignalMetadata
/// + SignalType are frozen at M5 close. Modifications post-freeze require ADR.
class DecoderInterface : public signalforge::pipeline::FrameSink {
public:
    /// Schema identifier for this decoder. Stable across decoder lifetime.
    /// Convention: relative path from schema root, e.g., "temp_sensor.yaml".
    [[nodiscard]] virtual QString schemaId() const = 0;

    /// Catalog of signals this decoder may emit. Called once after
    /// construction; consumer caches. Stable across decoder lifetime.
    [[nodiscard]] virtual std::vector<SignalMetadata> signalMetadata() const = 0;

    /// Attach a downstream consumer. Idempotent: attaching the same sink
    /// twice is a no-op (logs warning). Thread-safe; may be called before
    /// or after frames start flowing.
    virtual void setSignalSink(std::shared_ptr<SignalValueSink> sink) = 0;
};

}  // namespace signalforge::decoder
