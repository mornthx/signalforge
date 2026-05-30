#pragma once

#include "decode/schema.hpp"

#include <QByteArray>
#include <QString>
#include <vector>

namespace signalforge::decoder {

/// One node in a dissected frame: a schema field (or a bit slice of a
/// `BitField`, or the synthetic magic-byte fingerprint) resolved against a
/// concrete payload. Carries both the byte range (so the UI can highlight the
/// originating bytes in the hex pane) and the decoded value (so the same
/// numbers the pipeline produced are shown in the tree).
struct DissectedField {
    QString name;                          ///< Field / bit-slice name, or "(magic)".
    int byteOffset = 0;                    ///< First byte of this field in the payload.
    int byteLength = 0;                    ///< Bytes the field spans (for a bit slice: its container's span).
    int bitStart = -1;                     ///< Bit slice start within the container; -1 if not a bit slice.
    int bitCount = 0;                      ///< Bit slice width; 0 if not a bit slice.
    QString rawHex;                        ///< Raw bytes as space-separated hex, in payload order ("aa", "e3 05").
    QString value;                         ///< Decoded value, formatted for display ("23.45", "true", "ON").
    QString unit;                          ///< Engineering unit, if any.
    QString typeLabel;                     ///< Encoding summary, e.g. "uint16", "int16 ×0.01", "bool", "char[4]".
    bool truncated = false;                ///< Field lies (partly) beyond the payload; `value` is unavailable.
    std::vector<DissectedField> children;  ///< Bit slices of a BitField parent.
};

/// The result of dissecting one frame against a schema.
struct Dissection {
    bool matched = false;                ///< A layout's magic fingerprint matched the payload.
    QString layoutName;                  ///< Name of the matched layout (empty if unmatched).
    QString diagnostic;                  ///< Human-readable note: why unmatched, or why malformed.
    int payloadSize = 0;                 ///< Convenience copy of the payload length.
    std::vector<DissectedField> fields;  ///< Top-level fields, in schema order.
};

/// Resolves a frame's raw bytes into a human-readable dissection tree, using
/// the **same** byte-decoding primitives as `SchemaDecoder` (see
/// `field_codec.hpp`) so the Raw view never disagrees with the decoded
/// signals. Pure and stateless apart from the schema it carries; safe to call
/// from the UI thread for any payload.
///
/// Unlike the decoder, the dissector does not bail on a malformed frame: it
/// dissects every in-bounds field and marks the rest `truncated`, because a
/// short/garbled frame is exactly what a user reaches for the Raw view to
/// understand.
class FrameDissector {
public:
    explicit FrameDissector(Schema schema);

    /// Dissect `payload` against the first layout whose magic matches.
    [[nodiscard]] Dissection dissect(const QByteArray& payload) const;

    [[nodiscard]] const Schema& schema() const noexcept {
        return schema_;
    }

private:
    Schema schema_;
};

}  // namespace signalforge::decoder
