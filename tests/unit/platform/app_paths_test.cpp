// tests/unit/platform/app_paths_test.cpp
#include "observability/logging.hpp"
#include "platform/app_paths.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace signalforge::platform;

namespace {

class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value) : name_(name) {
        const char* prev = std::getenv(name);
        if (prev != nullptr) {
            had_prev_ = true;
            prev_.assign(prev);
        }
        if (value == nullptr) {
            ::unsetenv(name);
        } else {
            ::setenv(name, value, 1);
        }
    }
    ~ScopedEnv() {
        if (had_prev_) {
            ::setenv(name_, prev_.c_str(), 1);
        } else {
            ::unsetenv(name_);
        }
    }
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    const char* name_;
    std::string prev_;
    bool had_prev_ = false;
};

std::filesystem::path uniqueTmpRoot(const std::string& suffix) {
    auto base = std::filesystem::temp_directory_path() /
                ("signalforge_m2_apppaths_" + suffix + "_" + std::to_string(::getpid()));
    std::filesystem::remove_all(base);
    return base;
}

}  // namespace

TEST_CASE("app_paths: logDirectory honors SIGNALFORGE_LOG_DIR override", "[platform][app_paths]") {
    signalforge::observability::init_logging();
    const auto tmp = uniqueTmpRoot("log_override");
    ScopedEnv guard{"SIGNALFORGE_LOG_DIR", tmp.c_str()};

    const auto got = logDirectory();
    REQUIRE(!got.isEmpty());
    REQUIRE(std::filesystem::exists(tmp));
    REQUIRE(got.toStdString() == tmp.string());

    std::filesystem::remove_all(tmp);
}

TEST_CASE("app_paths: logDirectory falls back to XDG_STATE_HOME/signalforge/logs", "[platform][app_paths]") {
    signalforge::observability::init_logging();
    const auto tmp = uniqueTmpRoot("log_xdg");
    ScopedEnv unsetOverride{"SIGNALFORGE_LOG_DIR", nullptr};
    ScopedEnv xdg{"XDG_STATE_HOME", tmp.c_str()};

    const auto got = logDirectory();
    REQUIRE(!got.isEmpty());
    const auto expected = (tmp / "signalforge" / "logs").string();
    REQUIRE(got.toStdString() == expected);
    REQUIRE(std::filesystem::exists(expected));

    std::filesystem::remove_all(tmp);
}

TEST_CASE("app_paths: logDirectory falls back to HOME/.local/state when XDG and override unset",
          "[platform][app_paths]") {
    signalforge::observability::init_logging();
    const auto tmp = uniqueTmpRoot("log_home");
    ScopedEnv unsetOverride{"SIGNALFORGE_LOG_DIR", nullptr};
    ScopedEnv unsetXdg{"XDG_STATE_HOME", nullptr};
    ScopedEnv homeEnv{"HOME", tmp.c_str()};

    const auto got = logDirectory();
    const auto expected = (tmp / ".local" / "state" / "signalforge" / "logs").string();
    REQUIRE(got.toStdString() == expected);
    REQUIRE(std::filesystem::exists(expected));

    std::filesystem::remove_all(tmp);
}

TEST_CASE("app_paths: crashDumpDirectory honors XDG_STATE_HOME", "[platform][app_paths]") {
    signalforge::observability::init_logging();
    const auto tmp = uniqueTmpRoot("crash_xdg");
    ScopedEnv xdg{"XDG_STATE_HOME", tmp.c_str()};

    const auto got = crashDumpDirectory();
    const auto expected = (tmp / "signalforge" / "crashdumps").string();
    REQUIRE(got.toStdString() == expected);
    REQUIRE(std::filesystem::exists(expected));

    std::filesystem::remove_all(tmp);
}

TEST_CASE("app_paths: crashDumpDirectory falls back to HOME/.local/state", "[platform][app_paths]") {
    signalforge::observability::init_logging();
    const auto tmp = uniqueTmpRoot("crash_home");
    ScopedEnv unsetXdg{"XDG_STATE_HOME", nullptr};
    ScopedEnv homeEnv{"HOME", tmp.c_str()};

    const auto got = crashDumpDirectory();
    const auto expected = (tmp / ".local" / "state" / "signalforge" / "crashdumps").string();
    REQUIRE(got.toStdString() == expected);
    REQUIRE(std::filesystem::exists(expected));

    std::filesystem::remove_all(tmp);
}

TEST_CASE("app_paths: configDirectory honors XDG_CONFIG_HOME", "[platform][app_paths]") {
    signalforge::observability::init_logging();
    const auto tmp = uniqueTmpRoot("config_xdg");
    ScopedEnv xdg{"XDG_CONFIG_HOME", tmp.c_str()};

    const auto got = configDirectory();
    const auto expected = (tmp / "signalforge").string();
    REQUIRE(got.toStdString() == expected);
    REQUIRE(std::filesystem::exists(expected));

    std::filesystem::remove_all(tmp);
}

TEST_CASE("app_paths: configDirectory falls back to HOME/.config", "[platform][app_paths]") {
    signalforge::observability::init_logging();
    const auto tmp = uniqueTmpRoot("config_home");
    ScopedEnv unsetXdg{"XDG_CONFIG_HOME", nullptr};
    ScopedEnv homeEnv{"HOME", tmp.c_str()};

    const auto got = configDirectory();
    const auto expected = (tmp / ".config" / "signalforge").string();
    REQUIRE(got.toStdString() == expected);
    REQUIRE(std::filesystem::exists(expected));

    std::filesystem::remove_all(tmp);
}
