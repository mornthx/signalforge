#pragma once

#include "decode/schema.hpp"

#include <QByteArray>
#include <cstdint>
#include <cstring>

/// Low-level, side-effect-free primitives for reading scalar fields out of a
/// byte payload according to a schema `FieldDef`/`Layout`.
///
/// These were originally file-private helpers inside `schema_decoder.cpp`.
/// They are hoisted here so that the live `SchemaDecoder` (which emits
/// `SignalValue`s into the pipeline) and the UI-side `FrameDissector` (which
/// produces a human-readable dissection tree for the Raw view) share **one**
/// source of truth for endianness, sign-extension and float aliasing. A Raw
/// dissection that disagreed with the decoded signal would make the debugging
/// tool lie; sharing the primitives makes that class of bug impossible.
///
/// All readers assume the caller has already bounds-checked the access.
namespace signalforge::decoder::codec {

[[nodiscard]] inline bool encodingIsFloat(FieldEncoding e) noexcept {
    return e == FieldEncoding::Float32 || e == FieldEncoding::Float64;
}

[[nodiscard]] inline bool encodingIsSignedInt(FieldEncoding e) noexcept {
    return e == FieldEncoding::Int8 || e == FieldEncoding::Int16 || e == FieldEncoding::Int32 ||
           e == FieldEncoding::Int64;
}

[[nodiscard]] inline bool encodingIsUnsignedInt(FieldEncoding e) noexcept {
    return e == FieldEncoding::Uint8 || e == FieldEncoding::Uint16 || e == FieldEncoding::Uint32 ||
           e == FieldEncoding::Uint64;
}

[[nodiscard]] inline bool encodingIsNumeric(FieldEncoding e) noexcept {
    return encodingIsFloat(e) || encodingIsSignedInt(e) || encodingIsUnsignedInt(e);
}

/// Read `nBytes` from `bytes` and assemble a uint64 per `endianness`. The
/// caller has already verified `bytes` spans at least `nBytes`.
[[nodiscard]] inline std::uint64_t readUnsigned(const std::uint8_t* bytes, int nBytes, Endianness endianness) noexcept {
    std::uint64_t v = 0;
    if (endianness == Endianness::Little) {
        for (int i = 0; i < nBytes; ++i) {
            v |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
        }
    } else {
        for (int i = 0; i < nBytes; ++i) {
            v = (v << 8) | static_cast<std::uint64_t>(bytes[i]);
        }
    }
    return v;
}

/// Sign-extend the low `nBytes * 8` bits of `raw` to 64 bits.
[[nodiscard]] inline std::int64_t signExtend(std::uint64_t raw, int nBytes) noexcept {
    if (nBytes <= 0 || nBytes >= 8) {
        return static_cast<std::int64_t>(raw);
    }
    const int shift = (8 - nBytes) * 8;
    const std::int64_t s = static_cast<std::int64_t>(raw << shift);
    return s >> shift;
}

/// Assemble 4 bytes into an IEEE-754 single and promote to double.
[[nodiscard]] inline double readFloat32(const std::uint8_t* bytes, Endianness endianness) noexcept {
    std::uint8_t buf[4];
    if (endianness == Endianness::Little) {
        std::memcpy(buf, bytes, 4);
    } else {
        buf[0] = bytes[3];
        buf[1] = bytes[2];
        buf[2] = bytes[1];
        buf[3] = bytes[0];
    }
    float f = 0.0f;
    std::memcpy(&f, buf, sizeof(f));
    return static_cast<double>(f);
}

/// Assemble 8 bytes into an IEEE-754 double.
[[nodiscard]] inline double readFloat64(const std::uint8_t* bytes, Endianness endianness) noexcept {
    std::uint8_t buf[8];
    if (endianness == Endianness::Little) {
        std::memcpy(buf, bytes, 8);
    } else {
        for (int i = 0; i < 8; ++i) {
            buf[i] = bytes[7 - i];
        }
    }
    double d = 0.0;
    std::memcpy(&d, buf, sizeof(d));
    return d;
}

/// True iff `payload[match.offset .. +match.bytes.size()]` equals `match.bytes`.
/// False if the payload is too short, or the match has no bytes.
[[nodiscard]] inline bool layoutMatches(const LayoutMatch& match, const QByteArray& payload) noexcept {
    if (match.bytes.empty()) {
        return false;
    }
    const int needed = match.offset + static_cast<int>(match.bytes.size());
    if (payload.size() < needed) {
        return false;
    }
    const auto* data = reinterpret_cast<const std::uint8_t*>(payload.constData()) + match.offset;
    for (std::size_t i = 0; i < match.bytes.size(); ++i) {
        if (data[i] != match.bytes[i]) {
            return false;
        }
    }
    return true;
}

}  // namespace signalforge::decoder::codec
