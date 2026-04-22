// src/observability/logging.hpp
#pragma once
#include <spdlog/spdlog.h>

namespace signalforge::observability {

/// Initialize the async rotating-file logger. Idempotent.
/// Log directory: $XDG_STATE_HOME/signalforge/logs/, falling back to
/// ~/.local/state/signalforge/logs/ when XDG_STATE_HOME is unset.
/// Format: JSON lines. Fields: ts, level, thread, module, event, fields.
/// Rotation: 10 MB per file, 10 files retained.
/// Default level: info. Override via the SIGNALFORGE_LOG_LEVEL env var.
void init_logging();

}  // namespace signalforge::observability

#define SF_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define SF_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define SF_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define SF_LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define SF_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
