// src/platform/app_paths.cpp
#include "platform/app_paths.hpp"

#include "observability/logging.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace signalforge::platform {

namespace {

std::filesystem::path envPath(const char* var) {
    const char* raw = std::getenv(var);
    if (raw == nullptr || *raw == '\0') {
        return {};
    }
    return std::filesystem::path{raw};
}

std::filesystem::path home() {
    const char* raw = std::getenv("HOME");
    if (raw != nullptr && *raw != '\0') {
        return std::filesystem::path{raw};
    }
    return std::filesystem::current_path();
}

QString ensureDir(const std::filesystem::path& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        SF_LOG_WARN("app_paths: failed to create directory '{}': {}", dir.string(), ec.message());
        return {};
    }
    return QString::fromStdString(dir.string());
}

}  // namespace

QString logDirectory() {
    if (auto custom = envPath("SIGNALFORGE_LOG_DIR"); !custom.empty()) {
        return ensureDir(custom);
    }
    if (auto xdgState = envPath("XDG_STATE_HOME"); !xdgState.empty()) {
        return ensureDir(xdgState / "signalforge" / "logs");
    }
    return ensureDir(home() / ".local" / "state" / "signalforge" / "logs");
}

QString crashDumpDirectory() {
    if (auto xdgState = envPath("XDG_STATE_HOME"); !xdgState.empty()) {
        return ensureDir(xdgState / "signalforge" / "crashdumps");
    }
    return ensureDir(home() / ".local" / "state" / "signalforge" / "crashdumps");
}

QString configDirectory() {
    if (auto xdgConfig = envPath("XDG_CONFIG_HOME"); !xdgConfig.empty()) {
        return ensureDir(xdgConfig / "signalforge");
    }
    return ensureDir(home() / ".config" / "signalforge");
}

}  // namespace signalforge::platform
