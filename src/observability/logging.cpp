#include "observability/logging.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace signalforge::observability {

namespace {

constexpr std::string_view kLoggerName = "signalforge";
constexpr std::string_view kLogFileName = "signalforge.log";
constexpr std::size_t kMaxFileBytes = 10 * 1024 * 1024;  // 10 MB
constexpr std::size_t kMaxFiles = 10;

// The pattern uses `%J` (custom flag) for the fields slot. FieldsFlag
// writes `{}` when no fields have been attached and `{"k":"v",...}` when
// `with_fields` has populated thread-local state.
constexpr std::string_view kJsonPattern =
    R"({"ts":"%Y-%m-%dT%H:%M:%S.%eZ","level":"%l","thread":%t,"module":"%n","event":"%v","fields":%J})";

std::once_flag g_init_flag;

// Per-thread field state. with_fields() populates; the FieldsFlag
// formatter consumes and clears.
thread_local std::vector<std::pair<std::string, std::string>> g_thread_fields;

void appendJsonEscaped(spdlog::memory_buf_t& dest, std::string_view value) {
    for (char c : value) {
        switch (c) {
        case '"':
            dest.append(std::string_view{"\\\""});
            break;
        case '\\':
            dest.append(std::string_view{"\\\\"});
            break;
        case '\n':
            dest.append(std::string_view{"\\n"});
            break;
        case '\r':
            dest.append(std::string_view{"\\r"});
            break;
        case '\t':
            dest.append(std::string_view{"\\t"});
            break;
        default:
            dest.push_back(c);
            break;
        }
    }
}

// Custom flag formatter bound to `%J` in kJsonPattern. Emits the
// thread-local fields as a JSON object and clears them after use.
class FieldsFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& /*msg*/, const std::tm& /*tm_time*/,
                spdlog::memory_buf_t& dest) override {
        if (g_thread_fields.empty()) {
            dest.push_back('{');
            dest.push_back('}');
            return;
        }
        dest.push_back('{');
        bool first = true;
        for (const auto& [k, v] : g_thread_fields) {
            if (!first) {
                dest.push_back(',');
            }
            first = false;
            dest.push_back('"');
            appendJsonEscaped(dest, k);
            dest.append(std::string_view{"\":\""});
            appendJsonEscaped(dest, v);
            dest.push_back('"');
        }
        dest.push_back('}');
        g_thread_fields.clear();
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<FieldsFlag>();
    }
};

std::filesystem::path resolve_log_dir() {
    if (const char* xdg = std::getenv("XDG_STATE_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path{xdg} / "signalforge" / "logs";
    }
    const char* home = std::getenv("HOME");
    const std::filesystem::path base =
        (home != nullptr && *home != '\0') ? std::filesystem::path{home} : std::filesystem::current_path();
    return base / ".local" / "state" / "signalforge" / "logs";
}

spdlog::level::level_enum resolve_level() {
    const char* raw = std::getenv("SIGNALFORGE_LOG_LEVEL");
    if (raw == nullptr || *raw == '\0') {
        return spdlog::level::info;
    }
    const std::string_view value{raw};
    if (value == "trace")
        return spdlog::level::trace;
    if (value == "debug")
        return spdlog::level::debug;
    if (value == "info")
        return spdlog::level::info;
    if (value == "warn")
        return spdlog::level::warn;
    if (value == "error")
        return spdlog::level::err;
    return spdlog::level::info;
}

}  // namespace

void init_logging() {
    std::call_once(g_init_flag, []() {
        const std::filesystem::path dir = resolve_log_dir();
        std::filesystem::create_directories(dir);
        const std::filesystem::path file = dir / kLogFileName;

        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file.string(), kMaxFileBytes, kMaxFiles);

        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<FieldsFlag>('J').set_pattern(std::string{kJsonPattern});
        sink->set_formatter(std::move(formatter));

        // Synchronous logger: the FieldsFlag formatter reads thread_local
        // field state, which requires the formatter to run on the caller's
        // thread. spdlog's async mode does not carry thread_local through
        // the queue (MDC header: "Not supported in async mode"). M2-concerns
        // documents the deviation from architecture §14.1's "async" hint.
        auto logger = std::make_shared<spdlog::logger>(std::string{kLoggerName}, sink);

        const auto level = resolve_level();
        logger->set_level(level);
        logger->flush_on(spdlog::level::info);

        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
        spdlog::set_level(level);
    });
}

void with_fields(std::initializer_list<std::pair<std::string_view, std::string_view>> fields) {
    g_thread_fields.clear();
    g_thread_fields.reserve(fields.size());
    for (const auto& [k, v] : fields) {
        g_thread_fields.emplace_back(std::string{k}, std::string{v});
    }
}

void clear_fields() {
    g_thread_fields.clear();
}

}  // namespace signalforge::observability
