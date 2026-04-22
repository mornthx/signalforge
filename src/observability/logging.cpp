#include "observability/logging.hpp"

#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <string>
#include <string_view>

namespace signalforge::observability {

namespace {

constexpr std::string_view kLoggerName = "signalforge";
constexpr std::string_view kLogFileName = "signalforge.log";
constexpr std::size_t kMaxFileBytes = 10 * 1024 * 1024;  // 10 MB
constexpr std::size_t kMaxFiles = 10;
constexpr std::size_t kThreadPoolQueue = 8192;
constexpr std::size_t kThreadPoolThreads = 1;
constexpr std::string_view kJsonPattern =
    R"({"ts":"%Y-%m-%dT%H:%M:%S.%eZ","level":"%l","thread":%t,"module":"%n","event":"%v","fields":{}})";

std::once_flag g_init_flag;

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

        spdlog::init_thread_pool(kThreadPoolQueue, kThreadPoolThreads);

        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file.string(), kMaxFileBytes, kMaxFiles);
        sink->set_pattern(std::string{kJsonPattern});

        auto logger = std::make_shared<spdlog::async_logger>(std::string{kLoggerName}, sink, spdlog::thread_pool(),
                                                             spdlog::async_overflow_policy::block);

        const auto level = resolve_level();
        logger->set_level(level);
        logger->flush_on(spdlog::level::info);

        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
        spdlog::set_level(level);
    });
}

}  // namespace signalforge::observability
