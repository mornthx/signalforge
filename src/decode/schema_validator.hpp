#pragma once

#include "decode/schema.hpp"

#include <QString>
#include <expected>
#include <vector>

namespace signalforge::decoder {

/// One validation problem found while loading a schema. Multiple errors may
/// be returned per file (the validator continues after a non-fatal field-
/// level problem so users can fix everything in one pass).
struct ValidationError {
    QString filePath;     ///< File path or virtual path supplied to validateString.
    int lineNumber = -1;  ///< 1-based; -1 when yaml-cpp's Mark() does not provide a position.
    QString fieldPath;    ///< Dotted path, e.g., "layouts[0].fields[3].bit_fields[1].bit_count".
    QString message;      ///< Human-readable, actionable description.
};

/// On success: the parsed Schema. On failure: at least one ValidationError.
using ValidationResult = std::expected<Schema, std::vector<ValidationError>>;

/// Loads + validates user-authored schema yaml files against schema v1.
///
/// Validation rules per M5 spec §4.4 sequence (1–5):
///   1. Yaml syntax errors are reported with line numbers.
///   2. Top-level required keys: `schema_version`, `layouts`.
///   3. Each layout: `name`, `endianness`, `match`, non-empty `fields`.
///   4. Each field: `name` non-empty + unique within layout, valid `encoding`,
///      consistent `size_bytes`, multi-byte fields require resolvable
///      endianness, `BitField` fields require non-empty/non-overlapping/
///      in-range `bit_fields`.
///   5. Returns a fully-populated Schema on success.
///
/// All public methods are thread-safe (stateless static functions).
class SchemaValidator {
public:
    /// Load a yaml file from disk, validate, and return either a Schema or a
    /// list of errors. The schema's `id` field is set to the absolute path.
    [[nodiscard]] static ValidationResult validateFile(const QString& yamlPath);

    /// Validate a yaml string. `virtualPath` is used for error reporting and
    /// becomes the schema's `id`; it does not need to exist on disk.
    [[nodiscard]] static ValidationResult validateString(const QString& yamlContent, const QString& virtualPath);
};

}  // namespace signalforge::decoder
