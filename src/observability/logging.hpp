// src/observability/logging.hpp
#pragma once

#include <initializer_list>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>

namespace signalforge::observability {

/// Initialize the async rotating-file logger. Idempotent.
/// Log directory: $XDG_STATE_HOME/signalforge/logs/, falling back to
/// ~/.local/state/signalforge/logs/ when XDG_STATE_HOME is unset.
/// Format: JSON lines. Fields: ts, level, thread, module, event, fields.
/// Rotation: 10 MB per file, 10 files retained.
/// Default level: info. Override via the SIGNALFORGE_LOG_LEVEL env var.
void init_logging();

/// Attach structured fields to the next log line emitted on the same
/// thread. After that line is formatted, the fields are cleared.
///
/// Usage:
///   signalforge::observability::with_fields({
///       {"device_id", "dev0"},
///       {"queue", "rx"},
///   });
///   SF_LOG_WARN("backpressure triggered");
///
/// The JSON-lines output includes the fields in the `"fields"` slot:
///   {"ts":"...","level":"warn",...,"fields":{"device_id":"dev0","queue":"rx"}}
///
/// Thread safety: each thread has its own field state; calls on one
/// thread do not affect another. Intended for one-off correlation, not
/// high-volume logging. Subsequent `SF_LOG_*` calls without a preceding
/// `with_fields` emit `"fields":{}`.
///
/// Calling `with_fields` twice without an intervening log line replaces
/// (not appends) the previous fields.
///
/// The field values are JSON-escaped for `"` and `\`; other control
/// characters are emitted verbatim (caller should not include them).
void with_fields(std::initializer_list<std::pair<std::string_view, std::string_view>> fields);

/// Clear any fields attached to the current thread. Rarely needed — the
/// formatter consumes fields after the next log line. Exposed for test
/// teardown.
void clear_fields();

}  // namespace signalforge::observability

#define SF_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define SF_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define SF_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define SF_LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define SF_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
