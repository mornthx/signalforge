#include "decode/schema_validator.hpp"

#include "observability/logging.hpp"

#include <QFileInfo>
#include <algorithm>
#include <array>
#include <string>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

namespace signalforge::decoder {

namespace {

int markLine(const YAML::Mark& mark) {
    if (mark.is_null()) {
        return -1;
    }
    return mark.line + 1;  // yaml-cpp is 0-based; user-facing is 1-based.
}

int nodeLine(const YAML::Node& node) {
    return markLine(node.Mark());
}

QString qstr(const std::string& s) {
    return QString::fromStdString(s);
}

void addError(std::vector<ValidationError>& errors, const QString& filePath, int line, const QString& fieldPath,
              const QString& message) {
    errors.push_back({filePath, line, fieldPath, message});
}

struct EncodingMeta {
    FieldEncoding encoding;
    int sizeBytes;   ///< Canonical byte size; 0 means user-specified (FixedString) or container (BitField).
    bool multiByte;  ///< True iff endianness must be resolvable.
    const char* keyword;
};

constexpr std::array<EncodingMeta, 13> kEncodings = {{
    {FieldEncoding::Int8, 1, false, "int8"},
    {FieldEncoding::Int16, 2, true, "int16"},
    {FieldEncoding::Int32, 4, true, "int32"},
    {FieldEncoding::Int64, 8, true, "int64"},
    {FieldEncoding::Uint8, 1, false, "uint8"},
    {FieldEncoding::Uint16, 2, true, "uint16"},
    {FieldEncoding::Uint32, 4, true, "uint32"},
    {FieldEncoding::Uint64, 8, true, "uint64"},
    {FieldEncoding::Float32, 4, true, "float32"},
    {FieldEncoding::Float64, 8, true, "float64"},
    {FieldEncoding::Bool, 1, false, "bool"},
    {FieldEncoding::BitField, 0, false, "bitfield"},
    {FieldEncoding::FixedString, 0, false, "fixed_string"},
}};

const EncodingMeta* lookupEncoding(const std::string& keyword) {
    for (const auto& meta : kEncodings) {
        if (keyword == meta.keyword) {
            return &meta;
        }
    }
    return nullptr;
}

std::optional<Endianness> parseEndianness(const std::string& s) {
    if (s == "little") {
        return Endianness::Little;
    }
    if (s == "big") {
        return Endianness::Big;
    }
    return std::nullopt;
}

/// Validate one field.  Appends errors on problems; returns false on a fatal
/// problem that prevents this field from being usable.  Layout-level
/// `defaultEndianness` is `std::nullopt` when the layout itself failed to
/// resolve endianness; per-field endianness can still be required.
bool validateField(const YAML::Node& fieldNode, FieldDef& out, std::optional<Endianness> defaultEndianness,
                   const QString& filePath, const QString& fieldPath, std::vector<ValidationError>& errors) {
    bool ok = true;
    const int line = nodeLine(fieldNode);

    if (!fieldNode.IsMap()) {
        addError(errors, filePath, line, fieldPath, QStringLiteral("field must be a mapping"));
        return false;
    }

    if (auto n = fieldNode["name"]; n && n.IsScalar()) {
        out.name = qstr(n.as<std::string>());
        if (out.name.isEmpty()) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".name",
                     QStringLiteral("'name' must be a non-empty string"));
            ok = false;
        }
    } else {
        addError(errors, filePath, line, fieldPath + ".name", QStringLiteral("'name' is required (non-empty string)"));
        ok = false;
    }

    if (auto n = fieldNode["offset"]; n && n.IsScalar()) {
        try {
            out.offset = n.as<int>();
        } catch (const YAML::Exception&) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".offset",
                     QStringLiteral("'offset' must be a non-negative integer"));
            ok = false;
        }
        if (out.offset < 0) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".offset",
                     QStringLiteral("'offset' must be >= 0 (got %1)").arg(out.offset));
            ok = false;
        }
    } else {
        addError(errors, filePath, line, fieldPath + ".offset",
                 QStringLiteral("'offset' is required (non-negative byte offset)"));
        ok = false;
    }

    const EncodingMeta* enc = nullptr;
    if (auto n = fieldNode["encoding"]; n && n.IsScalar()) {
        const auto kw = n.as<std::string>();
        enc = lookupEncoding(kw);
        if (enc == nullptr) {
            QString allowed;
            for (std::size_t i = 0; i < kEncodings.size(); ++i) {
                if (i != 0) {
                    allowed += QStringLiteral(", ");
                }
                allowed += QString::fromUtf8(kEncodings[i].keyword);
            }
            addError(errors, filePath, nodeLine(n), fieldPath + ".encoding",
                     QStringLiteral("invalid encoding '%1' (must be one of: %2)").arg(qstr(kw), allowed));
            ok = false;
        } else {
            out.encoding = enc->encoding;
        }
    } else {
        addError(errors, filePath, line, fieldPath + ".encoding", QStringLiteral("'encoding' is required"));
        ok = false;
    }

    int parsedSize = 0;
    bool sizeProvided = false;
    if (auto n = fieldNode["size_bytes"]; n && n.IsScalar()) {
        sizeProvided = true;
        try {
            parsedSize = n.as<int>();
        } catch (const YAML::Exception&) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".size_bytes",
                     QStringLiteral("'size_bytes' must be a positive integer"));
            ok = false;
        }
        if (parsedSize <= 0) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".size_bytes",
                     QStringLiteral("'size_bytes' must be > 0 (got %1)").arg(parsedSize));
            ok = false;
        }
    }

    if (enc != nullptr) {
        if (enc->encoding == FieldEncoding::Bool) {
            // `bool` is reserved as a child encoding of bitfield (bit_count: 1);
            // it is not a valid top-level field encoding.  See concerns.md #3.
            addError(errors, filePath, line, fieldPath + ".encoding",
                     QStringLiteral(
                         "encoding 'bool' is only valid inside a 'bitfield' field's 'bit_fields' (use bit_count: 1)"));
            ok = false;
        } else if (enc->sizeBytes != 0) {
            // Numeric type with a canonical size.
            if (sizeProvided && parsedSize != enc->sizeBytes) {
                addError(errors, filePath, line, fieldPath + ".size_bytes",
                         QStringLiteral("'size_bytes' (%1) is inconsistent with encoding '%2' (expected %3)")
                             .arg(parsedSize)
                             .arg(QString::fromUtf8(enc->keyword))
                             .arg(enc->sizeBytes));
                ok = false;
            }
            out.sizeBytes = enc->sizeBytes;
        } else if (enc->encoding == FieldEncoding::BitField) {
            // Container size must be a power of two in {1,2,4,8}.
            if (!sizeProvided) {
                addError(errors, filePath, line, fieldPath + ".size_bytes",
                         QStringLiteral("'size_bytes' is required for encoding 'bitfield' (one of 1, 2, 4, 8)"));
                ok = false;
            } else if (parsedSize != 1 && parsedSize != 2 && parsedSize != 4 && parsedSize != 8) {
                addError(errors, filePath, line, fieldPath + ".size_bytes",
                         QStringLiteral("'size_bytes' for encoding 'bitfield' must be 1, 2, 4, or 8 (got %1)")
                             .arg(parsedSize));
                ok = false;
            } else {
                out.sizeBytes = parsedSize;
            }
        } else if (enc->encoding == FieldEncoding::FixedString) {
            if (!sizeProvided) {
                addError(errors, filePath, line, fieldPath + ".size_bytes",
                         QStringLiteral("'size_bytes' is required for encoding 'fixed_string'"));
                ok = false;
            } else {
                out.sizeBytes = parsedSize;
            }
        }
    }

    if (auto n = fieldNode["endianness"]; n && n.IsScalar()) {
        const auto kw = n.as<std::string>();
        auto e = parseEndianness(kw);
        if (!e.has_value()) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".endianness",
                     QStringLiteral("invalid endianness '%1' (must be 'little' or 'big')").arg(qstr(kw)));
            ok = false;
        } else {
            out.endianness = e;
        }
    }

    if (auto n = fieldNode["scale"]; n && n.IsScalar()) {
        try {
            out.scale = n.as<double>();
        } catch (const YAML::Exception&) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".scale", QStringLiteral("'scale' must be a number"));
            ok = false;
        }
    }

    if (auto n = fieldNode["offset_transform"]; n && n.IsScalar()) {
        try {
            out.offsetTransform = n.as<double>();
        } catch (const YAML::Exception&) {
            addError(errors, filePath, nodeLine(n), fieldPath + ".offset_transform",
                     QStringLiteral("'offset_transform' must be a number"));
            ok = false;
        }
    }

    if (auto n = fieldNode["unit"]; n && n.IsScalar()) {
        out.unit = qstr(n.as<std::string>());
    }

    if (auto n = fieldNode["description"]; n && n.IsScalar()) {
        out.description = qstr(n.as<std::string>());
    }

    // Multi-byte numeric: must have resolvable endianness.
    if (enc != nullptr && enc->multiByte && !out.endianness.has_value() && !defaultEndianness.has_value()) {
        addError(errors, filePath, line, fieldPath + ".endianness",
                 QStringLiteral("multi-byte field '%1' requires endianness (set field-level or layout-level)")
                     .arg(out.name));
        ok = false;
    }

    // Bit fields validation for BitField encoding.
    if (enc != nullptr && enc->encoding == FieldEncoding::BitField) {
        const auto bf = fieldNode["bit_fields"];
        if (!bf || !bf.IsSequence() || bf.size() == 0) {
            addError(
                errors, filePath, line, fieldPath + ".bit_fields",
                QStringLiteral("'bit_fields' is required and must be a non-empty sequence for encoding 'bitfield'"));
            ok = false;
        } else {
            const int totalBits = out.sizeBytes * 8;
            // Build list with computed ranges, validate each.
            for (std::size_t i = 0; i < bf.size(); ++i) {
                const YAML::Node bn = bf[i];
                const QString bPath = QStringLiteral("%1.bit_fields[%2]").arg(fieldPath).arg(i);
                BitFieldDef b;
                bool bok = true;
                const int bline = nodeLine(bn);
                if (!bn.IsMap()) {
                    addError(errors, filePath, bline, bPath, QStringLiteral("bit_field entry must be a mapping"));
                    ok = false;
                    continue;
                }
                if (auto nn = bn["name"]; nn && nn.IsScalar()) {
                    b.name = qstr(nn.as<std::string>());
                    if (b.name.isEmpty()) {
                        addError(errors, filePath, nodeLine(nn), bPath + ".name",
                                 QStringLiteral("'name' must be a non-empty string"));
                        bok = false;
                    }
                } else {
                    addError(errors, filePath, bline, bPath + ".name", QStringLiteral("'name' is required"));
                    bok = false;
                }
                if (auto nn = bn["bit_start"]; nn && nn.IsScalar()) {
                    try {
                        b.bitStart = nn.as<int>();
                    } catch (const YAML::Exception&) {
                        addError(errors, filePath, nodeLine(nn), bPath + ".bit_start",
                                 QStringLiteral("'bit_start' must be a non-negative integer"));
                        bok = false;
                    }
                } else {
                    addError(errors, filePath, bline, bPath + ".bit_start", QStringLiteral("'bit_start' is required"));
                    bok = false;
                }
                if (auto nn = bn["bit_count"]; nn && nn.IsScalar()) {
                    try {
                        b.bitCount = nn.as<int>();
                    } catch (const YAML::Exception&) {
                        addError(errors, filePath, nodeLine(nn), bPath + ".bit_count",
                                 QStringLiteral("'bit_count' must be a positive integer"));
                        bok = false;
                    }
                    if (b.bitCount <= 0) {
                        addError(errors, filePath, nodeLine(nn), bPath + ".bit_count",
                                 QStringLiteral("'bit_count' must be > 0 (got %1)").arg(b.bitCount));
                        bok = false;
                    }
                } else {
                    addError(errors, filePath, bline, bPath + ".bit_count",
                             QStringLiteral("'bit_count' is required (>= 1)"));
                    bok = false;
                }
                if (auto nn = bn["description"]; nn && nn.IsScalar()) {
                    b.description = qstr(nn.as<std::string>());
                }
                if (bok && (b.bitStart < 0 || b.bitStart + b.bitCount > totalBits)) {
                    addError(errors, filePath, bline, bPath,
                             QStringLiteral("bit range [%1, %2) does not fit in %3-bit container")
                                 .arg(b.bitStart)
                                 .arg(b.bitStart + b.bitCount)
                                 .arg(totalBits));
                    bok = false;
                }
                if (bok) {
                    out.bitFields.push_back(std::move(b));
                } else {
                    ok = false;
                }
            }
            // Check overlap among accepted bit fields.
            std::vector<std::pair<int, int>> ranges;
            ranges.reserve(out.bitFields.size());
            for (const auto& b : out.bitFields) {
                ranges.emplace_back(b.bitStart, b.bitStart + b.bitCount);
            }
            std::sort(ranges.begin(), ranges.end());
            for (std::size_t i = 1; i < ranges.size(); ++i) {
                if (ranges[i].first < ranges[i - 1].second) {
                    addError(errors, filePath, line, fieldPath + ".bit_fields",
                             QStringLiteral("bit ranges overlap at bits [%1, %2) and [%3, %4)")
                                 .arg(ranges[i - 1].first)
                                 .arg(ranges[i - 1].second)
                                 .arg(ranges[i].first)
                                 .arg(ranges[i].second));
                    ok = false;
                    break;
                }
            }
            // Check for duplicate bit-field names.
            std::unordered_set<QString> seen;
            for (const auto& b : out.bitFields) {
                if (!seen.insert(b.name).second) {
                    addError(errors, filePath, line, fieldPath + ".bit_fields",
                             QStringLiteral("duplicate bit_field name '%1'").arg(b.name));
                    ok = false;
                }
            }
        }
    }

    return ok;
}

bool validateLayout(const YAML::Node& layoutNode, Layout& out, const QString& filePath, const QString& layoutPath,
                    std::vector<ValidationError>& errors) {
    bool ok = true;
    const int line = nodeLine(layoutNode);

    if (!layoutNode.IsMap()) {
        addError(errors, filePath, line, layoutPath, QStringLiteral("layout must be a mapping"));
        return false;
    }

    if (auto n = layoutNode["name"]; n && n.IsScalar()) {
        out.name = qstr(n.as<std::string>());
        if (out.name.isEmpty()) {
            addError(errors, filePath, nodeLine(n), layoutPath + ".name",
                     QStringLiteral("'name' must be a non-empty string"));
            ok = false;
        }
    } else {
        addError(errors, filePath, line, layoutPath + ".name", QStringLiteral("'name' is required"));
        ok = false;
    }

    std::optional<Endianness> layoutEndianness;
    if (auto n = layoutNode["endianness"]; n && n.IsScalar()) {
        const auto kw = n.as<std::string>();
        auto e = parseEndianness(kw);
        if (!e.has_value()) {
            addError(errors, filePath, nodeLine(n), layoutPath + ".endianness",
                     QStringLiteral("invalid endianness '%1' (must be 'little' or 'big')").arg(qstr(kw)));
            ok = false;
        } else {
            layoutEndianness = e;
            out.endianness = *e;
        }
    } else {
        addError(errors, filePath, line, layoutPath + ".endianness",
                 QStringLiteral("'endianness' is required at layout level (must be 'little' or 'big')"));
        ok = false;
    }

    if (auto n = layoutNode["match"]; n && n.IsMap()) {
        if (auto on = n["offset"]; on && on.IsScalar()) {
            try {
                out.match.offset = on.as<int>();
            } catch (const YAML::Exception&) {
                addError(errors, filePath, nodeLine(on), layoutPath + ".match.offset",
                         QStringLiteral("'match.offset' must be a non-negative integer"));
                ok = false;
            }
            if (out.match.offset < 0) {
                addError(errors, filePath, nodeLine(on), layoutPath + ".match.offset",
                         QStringLiteral("'match.offset' must be >= 0 (got %1)").arg(out.match.offset));
                ok = false;
            }
        } else {
            addError(errors, filePath, nodeLine(n), layoutPath + ".match.offset",
                     QStringLiteral("'match.offset' is required"));
            ok = false;
        }
        if (auto bn = n["bytes"]; bn && bn.IsSequence() && bn.size() > 0) {
            for (std::size_t i = 0; i < bn.size(); ++i) {
                const YAML::Node ben = bn[i];
                int b = -1;
                try {
                    b = ben.as<int>();
                } catch (const YAML::Exception&) {
                    addError(errors, filePath, nodeLine(ben),
                             QStringLiteral("%1.match.bytes[%2]").arg(layoutPath).arg(i),
                             QStringLiteral("byte must be an integer in [0, 255]"));
                    ok = false;
                    continue;
                }
                if (b < 0 || b > 255) {
                    addError(errors, filePath, nodeLine(ben),
                             QStringLiteral("%1.match.bytes[%2]").arg(layoutPath).arg(i),
                             QStringLiteral("byte (%1) must be in [0, 255]").arg(b));
                    ok = false;
                    continue;
                }
                out.match.bytes.push_back(static_cast<std::uint8_t>(b));
            }
        } else {
            addError(errors, filePath, nodeLine(n), layoutPath + ".match.bytes",
                     QStringLiteral("'match.bytes' is required and must be a non-empty sequence of bytes"));
            ok = false;
        }
    } else {
        addError(errors, filePath, line, layoutPath + ".match",
                 QStringLiteral("'match' is required (mapping with 'offset' and 'bytes')"));
        ok = false;
    }

    if (auto n = layoutNode["min_payload_bytes"]; n && n.IsScalar()) {
        try {
            out.minPayloadBytes = n.as<int>();
        } catch (const YAML::Exception&) {
            addError(errors, filePath, nodeLine(n), layoutPath + ".min_payload_bytes",
                     QStringLiteral("'min_payload_bytes' must be a non-negative integer"));
            ok = false;
        }
        if (out.minPayloadBytes < 0) {
            addError(errors, filePath, nodeLine(n), layoutPath + ".min_payload_bytes",
                     QStringLiteral("'min_payload_bytes' must be >= 0 (got %1)").arg(out.minPayloadBytes));
            ok = false;
        }
    } else {
        addError(errors, filePath, line, layoutPath + ".min_payload_bytes",
                 QStringLiteral("'min_payload_bytes' is required"));
        ok = false;
    }

    const auto fieldsNode = layoutNode["fields"];
    if (!fieldsNode || !fieldsNode.IsSequence() || fieldsNode.size() == 0) {
        addError(errors, filePath, line, layoutPath + ".fields",
                 QStringLiteral("'fields' is required and must be a non-empty sequence"));
        ok = false;
    } else {
        std::unordered_set<QString> fieldNames;
        for (std::size_t i = 0; i < fieldsNode.size(); ++i) {
            const YAML::Node fn = fieldsNode[i];
            const QString fPath = QStringLiteral("%1.fields[%2]").arg(layoutPath).arg(i);
            FieldDef field;
            const bool fOk = validateField(fn, field, layoutEndianness, filePath, fPath, errors);
            if (fOk && !field.name.isEmpty()) {
                if (!fieldNames.insert(field.name).second) {
                    addError(errors, filePath, nodeLine(fn), fPath + ".name",
                             QStringLiteral("duplicate field name '%1' within layout").arg(field.name));
                    ok = false;
                } else {
                    out.fields.push_back(std::move(field));
                }
            } else {
                ok = false;
            }
        }
    }

    return ok;
}

ValidationResult validateImpl(const YAML::Node& root, const QString& filePath) {
    std::vector<ValidationError> errors;
    Schema schema;
    schema.id = filePath;

    if (!root.IsMap()) {
        addError(errors, filePath, nodeLine(root), QStringLiteral("<root>"),
                 QStringLiteral("schema root must be a mapping"));
        return std::unexpected(std::move(errors));
    }

    if (auto n = root["schema_version"]; n && n.IsScalar()) {
        int v = -1;
        try {
            v = n.as<int>();
        } catch (const YAML::Exception&) {
            addError(errors, filePath, nodeLine(n), QStringLiteral("schema_version"),
                     QStringLiteral("'schema_version' must be an integer"));
        }
        if (v != 1) {
            addError(errors, filePath, nodeLine(n), QStringLiteral("schema_version"),
                     QStringLiteral("this codebase supports schema versions: [1]; file declares: %1").arg(v));
        } else {
            schema.schemaVersion = 1;
        }
    } else {
        addError(errors, filePath, nodeLine(root), QStringLiteral("schema_version"),
                 QStringLiteral("'schema_version' is required (must be 1)"));
    }

    if (auto n = root["description"]; n && n.IsScalar()) {
        schema.description = qstr(n.as<std::string>());
    }

    const auto layouts = root["layouts"];
    if (!layouts || !layouts.IsSequence() || layouts.size() == 0) {
        addError(errors, filePath, nodeLine(root), QStringLiteral("layouts"),
                 QStringLiteral("'layouts' is required and must be a non-empty sequence"));
    } else {
        std::unordered_set<QString> layoutNames;
        for (std::size_t i = 0; i < layouts.size(); ++i) {
            const YAML::Node ln = layouts[i];
            const QString lPath = QStringLiteral("layouts[%1]").arg(i);
            Layout layout;
            const bool lOk = validateLayout(ln, layout, filePath, lPath, errors);
            if (lOk && !layout.name.isEmpty()) {
                if (!layoutNames.insert(layout.name).second) {
                    addError(errors, filePath, nodeLine(ln), lPath + ".name",
                             QStringLiteral("duplicate layout name '%1'").arg(layout.name));
                } else {
                    schema.layouts.push_back(std::move(layout));
                }
            }
        }
    }

    if (!errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    return schema;
}

}  // namespace

ValidationResult SchemaValidator::validateFile(const QString& yamlPath) {
    const QFileInfo fi(yamlPath);
    const QString absolute = fi.absoluteFilePath();

    YAML::Node root;
    try {
        root = YAML::LoadFile(yamlPath.toStdString());
    } catch (const YAML::BadFile& e) {
        std::vector<ValidationError> errors;
        errors.push_back({absolute, -1, QStringLiteral("<file>"),
                          QStringLiteral("could not open file: %1").arg(QString::fromUtf8(e.what()))});
        return std::unexpected(std::move(errors));
    } catch (const YAML::ParserException& e) {
        std::vector<ValidationError> errors;
        errors.push_back({absolute, markLine(e.mark), QStringLiteral("<yaml>"),
                          QStringLiteral("yaml syntax error: %1").arg(QString::fromUtf8(e.msg.c_str()))});
        return std::unexpected(std::move(errors));
    } catch (const YAML::Exception& e) {
        std::vector<ValidationError> errors;
        errors.push_back({absolute, markLine(e.mark), QStringLiteral("<yaml>"),
                          QStringLiteral("yaml load error: %1").arg(QString::fromUtf8(e.msg.c_str()))});
        return std::unexpected(std::move(errors));
    }

    return validateImpl(root, absolute);
}

ValidationResult SchemaValidator::validateString(const QString& yamlContent, const QString& virtualPath) {
    YAML::Node root;
    try {
        root = YAML::Load(yamlContent.toStdString());
    } catch (const YAML::ParserException& e) {
        std::vector<ValidationError> errors;
        errors.push_back({virtualPath, markLine(e.mark), QStringLiteral("<yaml>"),
                          QStringLiteral("yaml syntax error: %1").arg(QString::fromUtf8(e.msg.c_str()))});
        return std::unexpected(std::move(errors));
    } catch (const YAML::Exception& e) {
        std::vector<ValidationError> errors;
        errors.push_back({virtualPath, markLine(e.mark), QStringLiteral("<yaml>"),
                          QStringLiteral("yaml load error: %1").arg(QString::fromUtf8(e.msg.c_str()))});
        return std::unexpected(std::move(errors));
    }

    return validateImpl(root, virtualPath);
}

}  // namespace signalforge::decoder
