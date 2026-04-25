#pragma once

#include "decode/decoder_interface.hpp"
#include "decode/schema.hpp"

#include <QByteArray>
#include <QString>
#include <memory>
#include <mutex>
#include <vector>

namespace signalforge::observability {
class Metric;
}

namespace signalforge::decoder {

/// Yaml-schema-driven decoder. Constructed from a pre-validated Schema
/// (typically returned by SchemaValidator::validateFile). On every incoming
/// frame, walks the schema's layouts in order; the first whose `match.bytes`
/// equals the payload at `match.offset` wins, and that layout's fields are
/// extracted and emitted to the attached SignalValueSink.
///
/// Metric namespace per spec §3.10 (`<metric>_<driverId>`):
///   - decoder_frames_decoded
///   - decoder_frames_unmatched
///   - decoder_frames_malformed
///   - decoder_signals_emitted
///   - decoder_last_decode_us  (gauge, microseconds of most recent decode)
///
/// Thread affinity: `onFrame` runs on the pipeline's thread; `setSignalSink`
/// is safe from any thread and is idempotent.
///
/// Freeze scope: `DecoderInterface` is frozen at M5 close. This class is the
/// concrete implementation, not frozen — internal layout may change.
class SchemaDecoder : public DecoderInterface {
public:
    SchemaDecoder(Schema schema, QString driverId);
    ~SchemaDecoder() override;

    SchemaDecoder(const SchemaDecoder&) = delete;
    SchemaDecoder& operator=(const SchemaDecoder&) = delete;

    // DecoderInterface
    [[nodiscard]] QString schemaId() const override;
    [[nodiscard]] std::vector<SignalMetadata> signalMetadata() const override;
    void setSignalSink(std::shared_ptr<SignalValueSink> sink) override;

    // FrameSink (via DecoderInterface)
    void onFrame(const signalforge::frame::RawFrame& frame) override;
    [[nodiscard]] QString sinkName() const override;

private:
    /// Per-field cached metadata index. For top-level fields, `metaIndex` is
    /// the position of this field's signal in `metadataCatalog_`. For
    /// `BitField` fields, `bitFieldMetaIndices` enumerates one metadata index
    /// per `bit_fields` entry, in the same order as the schema declares them.
    struct CachedField {
        int metaIndex = -1;                    ///< -1 for BitField parents.
        std::vector<int> bitFieldMetaIndices;  ///< one per bit slice (BitField only).
    };

    struct CachedLayout {
        std::vector<CachedField> fields;
    };

    Schema schema_;
    QString driverId_;
    std::vector<SignalMetadata> metadataCatalog_;
    std::vector<CachedLayout> layoutCache_;

    mutable std::mutex sinkMutex_;
    std::shared_ptr<SignalValueSink> sink_;

    signalforge::observability::Metric* mDecoded_ = nullptr;
    signalforge::observability::Metric* mUnmatched_ = nullptr;
    signalforge::observability::Metric* mMalformed_ = nullptr;
    signalforge::observability::Metric* mEmitted_ = nullptr;
    signalforge::observability::Metric* mLastDecodeUs_ = nullptr;

    /// Try each layout in order; first match wins. Returns true iff a layout
    /// matched and was processed (whether or not signals emitted).
    bool tryDecodeFrame(const signalforge::frame::RawFrame& frame, std::shared_ptr<SignalValueSink> sink);

    /// Returns true iff `payload[match.offset .. +match.bytes.size()]` equals
    /// `match.bytes`. False if the payload is too short for the comparison.
    [[nodiscard]] static bool layoutMatches(const LayoutMatch& match, const QByteArray& payload) noexcept;
};

}  // namespace signalforge::decoder
