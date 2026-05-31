#include "decode/frame_dissector.hpp"

#include "decode/field_codec.hpp"

#include <QLatin1Char>
#include <QLatin1String>
#include <algorithm>
#include <cstdint>
#include <utility>

namespace signalforge::decoder {

namespace {

namespace codec = signalforge::decoder::codec;

/// Space-separated hex of `[offset, offset+len)` in payload order. Returns the
/// in-bounds prefix if the range overruns the payload.
QString hexOf(const QByteArray& payload, int offset, int len) {
    const int end = std::min<int>(offset + len, static_cast<int>(payload.size()));
    QString out;
    out.reserve(std::max(0, end - offset) * 3);
    for (int i = offset; i < end; ++i) {
        const auto byte = static_cast<unsigned char>(payload.at(i));
        out += QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0'));
        if (i + 1 < end) {
            out += QLatin1Char(' ');
        }
    }
    return out;
}

/// Compact display string for a decoded double (trims trailing zeros).
QString formatDouble(double v) {
    QString s = QString::number(v, 'g', 12);
    return s;
}

/// "uint16", "int16 ×0.01 +5", "float32", "char[4]", "bitfield[1]".
QString numericTypeLabel(const FieldDef& field) {
    QString base;
    switch (field.encoding) {
    case FieldEncoding::Int8:
        base = QStringLiteral("int8");
        break;
    case FieldEncoding::Int16:
        base = QStringLiteral("int16");
        break;
    case FieldEncoding::Int32:
        base = QStringLiteral("int32");
        break;
    case FieldEncoding::Int64:
        base = QStringLiteral("int64");
        break;
    case FieldEncoding::Uint8:
        base = QStringLiteral("uint8");
        break;
    case FieldEncoding::Uint16:
        base = QStringLiteral("uint16");
        break;
    case FieldEncoding::Uint32:
        base = QStringLiteral("uint32");
        break;
    case FieldEncoding::Uint64:
        base = QStringLiteral("uint64");
        break;
    case FieldEncoding::Float32:
        base = QStringLiteral("float32");
        break;
    case FieldEncoding::Float64:
        base = QStringLiteral("float64");
        break;
    default:
        base = QStringLiteral("num");
        break;
    }
    if (field.scale.has_value()) {
        base += QStringLiteral(" ×%1").arg(formatDouble(*field.scale));
    }
    if (field.offsetTransform.has_value()) {
        const double off = *field.offsetTransform;
        base +=
            (off < 0) ? QStringLiteral(" %1").arg(formatDouble(off)) : QStringLiteral(" +%1").arg(formatDouble(off));
    }
    return base;
}

/// Decode a numeric field exactly as SchemaDecoder would, returning the
/// display string. `fieldBytes` is bounds-checked by the caller.
QString decodeNumeric(const std::uint8_t* fieldBytes, const FieldDef& field, Endianness end) {
    if (codec::encodingIsFloat(field.encoding)) {
        const double raw = (field.encoding == FieldEncoding::Float32) ? codec::readFloat32(fieldBytes, end)
                                                                      : codec::readFloat64(fieldBytes, end);
        double value = raw;
        if (field.scale.has_value()) {
            value *= *field.scale;
        }
        if (field.offsetTransform.has_value()) {
            value += *field.offsetTransform;
        }
        return formatDouble(value);
    }

    const std::uint64_t raw = codec::readUnsigned(fieldBytes, field.sizeBytes, end);
    std::int64_t intRaw;
    if (codec::encodingIsSignedInt(field.encoding)) {
        intRaw = codec::signExtend(raw, field.sizeBytes);
    } else {
        intRaw = static_cast<std::int64_t>(raw);
    }
    if (field.scale.has_value() || field.offsetTransform.has_value()) {
        double v = static_cast<double>(intRaw);
        if (field.scale.has_value()) {
            v *= *field.scale;
        }
        if (field.offsetTransform.has_value()) {
            v += *field.offsetTransform;
        }
        return formatDouble(v);
    }
    return QString::number(intRaw);
}

/// Dissect a single field against an in-bounds (or truncated) byte payload.
DissectedField dissectField(const QByteArray& payload, const FieldDef& field, Endianness layoutEnd) {
    DissectedField node;
    node.name = field.name;
    node.byteOffset = field.offset;
    node.byteLength = field.sizeBytes;
    node.unit = field.unit;
    node.rawHex = hexOf(payload, field.offset, field.sizeBytes);

    const Endianness end = field.endianness.value_or(layoutEnd);
    const int payloadSize = static_cast<int>(payload.size());

    if (field.offset < 0 || field.offset + field.sizeBytes > payloadSize) {
        node.truncated = true;
        node.value = QStringLiteral("—");
        node.typeLabel = (field.encoding == FieldEncoding::BitField)      ? QStringLiteral("bitfield")
                         : (field.encoding == FieldEncoding::FixedString) ? QStringLiteral("string")
                                                                          : numericTypeLabel(field);
        return node;
    }

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(payload.constData());
    const std::uint8_t* fieldBytes = bytes + field.offset;

    if (field.encoding == FieldEncoding::BitField) {
        node.typeLabel = QStringLiteral("bitfield[%1B]").arg(field.sizeBytes);
        const std::uint64_t raw = codec::readUnsigned(fieldBytes, field.sizeBytes, end);
        node.value = QStringLiteral("0x%1").arg(raw, field.sizeBytes * 2, 16, QLatin1Char('0'));
        for (const BitFieldDef& bit : field.bitFields) {
            DissectedField child;
            child.name = bit.name;
            child.byteOffset = field.offset;  // highlight spans the whole container
            child.byteLength = field.sizeBytes;
            child.bitStart = bit.bitStart;
            child.bitCount = bit.bitCount;
            child.rawHex = node.rawHex;
            const std::uint64_t mask =
                (bit.bitCount >= 64) ? ~std::uint64_t{0} : ((std::uint64_t{1} << bit.bitCount) - 1);
            const std::uint64_t slice = (raw >> bit.bitStart) & mask;
            if (bit.bitCount == 1) {
                child.value = (slice != 0) ? QStringLiteral("true") : QStringLiteral("false");
                child.typeLabel = QStringLiteral("bit %1").arg(bit.bitStart);
            } else {
                child.value = QString::number(static_cast<std::int64_t>(slice));
                child.typeLabel = QStringLiteral("bits %1..%2").arg(bit.bitStart).arg(bit.bitStart + bit.bitCount - 1);
            }
            node.children.push_back(std::move(child));
        }
        return node;
    }

    if (field.encoding == FieldEncoding::FixedString) {
        node.typeLabel = QStringLiteral("char[%1]").arg(field.sizeBytes);
        const QByteArray slice(reinterpret_cast<const char*>(fieldBytes), field.sizeBytes);
        int len = slice.indexOf('\0');
        if (len < 0) {
            len = field.sizeBytes;
        }
        node.value = QString::fromUtf8(slice.constData(), len);
        return node;
    }

    node.typeLabel = numericTypeLabel(field);
    node.value = decodeNumeric(fieldBytes, field, end);
    return node;
}

}  // namespace

FrameDissector::FrameDissector(Schema schema) : schema_(std::move(schema)) {}

Dissection FrameDissector::dissect(const QByteArray& payload) const {
    Dissection result;
    result.payloadSize = static_cast<int>(payload.size());

    const Layout* matched = nullptr;
    for (const Layout& layout : schema_.layouts) {
        if (codec::layoutMatches(layout.match, payload)) {
            matched = &layout;
            break;
        }
    }

    if (matched == nullptr) {
        result.matched = false;
        result.diagnostic = schema_.layouts.empty()
                                ? QStringLiteral("schema defines no layouts")
                                : QStringLiteral("no layout fingerprint matched these %1 bytes").arg(payload.size());
        return result;
    }

    result.matched = true;
    result.layoutName = matched->name;
    if (payload.size() < matched->minPayloadBytes) {
        result.diagnostic = QStringLiteral("malformed: %1 bytes < min_payload_bytes %2")
                                .arg(payload.size())
                                .arg(matched->minPayloadBytes);
    }

    // Synthetic node for the magic-byte fingerprint, so every byte the layout
    // pins is accounted for in the tree (Wireshark shows protocol preambles).
    if (!matched->match.bytes.empty()) {
        DissectedField magic;
        magic.name = QStringLiteral("(magic)");
        magic.byteOffset = matched->match.offset;
        magic.byteLength = static_cast<int>(matched->match.bytes.size());
        magic.typeLabel = QStringLiteral("fingerprint");
        magic.rawHex = hexOf(payload, magic.byteOffset, magic.byteLength);
        magic.value = magic.rawHex;
        result.fields.push_back(std::move(magic));
    }

    result.fields.reserve(result.fields.size() + matched->fields.size());
    for (const FieldDef& field : matched->fields) {
        result.fields.push_back(dissectField(payload, field, matched->endianness));
    }
    return result;
}

}  // namespace signalforge::decoder
