#pragma once

#include <QString>
#include <cstdint>
#include <optional>
#include <vector>

namespace signalforge::decoder {

/// Byte order for multi-byte integer and float decoding.
enum class Endianness {
    Little,
    Big,
};

/// Encoding of a single field in the byte payload.
///
/// Frozen at M5 close. Adding new encodings post-freeze requires bumping
/// schema_version (current: 1).
enum class FieldEncoding {
    Int8,
    Int16,
    Int32,
    Int64,
    Uint8,
    Uint16,
    Uint32,
    Uint64,
    Float32,
    Float64,
    Bool,         ///< Single bit; only valid as a BitFieldDef child.
    BitField,     ///< Container: emits one signal per `bit_fields` entry.
    FixedString,  ///< Fixed byte range decoded as UTF-8 (null-terminated).
};

/// One bit slice inside a `BitField` parent. Emits its own signal.
struct BitFieldDef {
    QString name;
    int bitStart = 0;  ///< 0-based bit index; 0 = LSB of first byte.
    int bitCount = 1;  ///< 1 → bool signal; >1 → int64 signal in [0, 2^bitCount).
    std::optional<QString> description;
};

/// One field in a layout. May emit a single signal or, for `BitField`,
/// expand into multiple signals via `bitFields`.
struct FieldDef {
    QString name;
    int offset = 0;  ///< Byte offset into the payload.
    FieldEncoding encoding = FieldEncoding::Uint8;
    int sizeBytes = 0;                     ///< Encoding-derived for numeric types; user-supplied for FixedString.
    std::optional<Endianness> endianness;  ///< Field-level override of layout default.
    std::optional<double> scale;           ///< Optional; applied to numeric: value = raw * scale + offsetTransform.
    std::optional<double>
        offsetTransform;  ///< Linear-transform offset; named distinctly to avoid clash with byte `offset`.
    QString unit;
    std::optional<QString> description;

    /// Only valid for `encoding == BitField`. Each entry produces a separate signal.
    std::vector<BitFieldDef> bitFields;
};

/// Magic-byte fingerprint identifying which layout an incoming frame matches.
struct LayoutMatch {
    int offset = 0;                   ///< Byte offset of the magic within the payload.
    std::vector<std::uint8_t> bytes;  ///< Required bytes at `offset` for a match.
};

/// One frame layout. A schema may contain multiple layouts distinguished by
/// `match`; the first whose bytes match the incoming frame wins.
struct Layout {
    QString name;
    Endianness endianness = Endianness::Little;  ///< Required: validator rejects schemas without this.
    LayoutMatch match;
    int minPayloadBytes = 0;  ///< Minimum frame size; frames smaller are malformed.
    std::vector<FieldDef> fields;
};

/// Top-level user-authored schema, post-validation.
struct Schema {
    int schemaVersion = 1;  ///< Frozen: only 1 is accepted in V1.
    QString id;             ///< Path or stable identifier; populated by validator from `validateFile`.
    QString description;
    std::vector<Layout> layouts;
};

}  // namespace signalforge::decoder
