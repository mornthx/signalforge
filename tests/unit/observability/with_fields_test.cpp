// tests/unit/observability/with_fields_test.cpp
#include "observability/logging.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

// Set XDG_STATE_HOME before init_logging's std::call_once can fire.
// The logger's resolve_log_dir uses XDG_STATE_HOME/signalforge/logs. Static
// initializer runs before main() so any in-process call to init_logging()
// picks up this directory.
std::filesystem::path setupLogDir() {
    auto xdg = std::filesystem::temp_directory_path() / ("signalforge_m2_wf_xdg_" + std::to_string(::getpid()));
    std::filesystem::remove_all(xdg);
    std::filesystem::create_directories(xdg);
    ::setenv("XDG_STATE_HOME", xdg.c_str(), 1);
    return xdg / "signalforge" / "logs";
}
const std::filesystem::path g_log_dir = setupLogDir();

std::string readLogFile() {
    // Flush first to make the file up to date on disk.
    if (auto logger = spdlog::default_logger(); logger) {
        logger->flush();
    }
    std::ifstream f(g_log_dir / "signalforge.log");
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Find the single line that contains the marker and return it.
std::string findLine(const std::string& haystack, const std::string& marker) {
    const auto idx = haystack.find(marker);
    if (idx == std::string::npos) {
        return {};
    }
    const auto line_start = haystack.rfind('\n', idx);
    const auto a = (line_start == std::string::npos) ? 0 : line_start + 1;
    const auto line_end = haystack.find('\n', idx);
    const auto b = (line_end == std::string::npos) ? haystack.size() : line_end;
    return haystack.substr(a, b - a);
}

}  // namespace

TEST_CASE("with_fields: empty slot when no call precedes log line", "[observability][with_fields]") {
    signalforge::observability::init_logging();
    signalforge::observability::clear_fields();

    SPDLOG_INFO("wf_marker_no_fields");
    const auto line = findLine(readLogFile(), "wf_marker_no_fields");
    REQUIRE(!line.empty());
    REQUIRE(line.find(R"("fields":{})") != std::string::npos);
}

TEST_CASE("with_fields: populated fields appear in next line", "[observability][with_fields]") {
    signalforge::observability::init_logging();
    signalforge::observability::clear_fields();

    signalforge::observability::with_fields({
        {"device_id", "dev0"},
        {"queue", "rx"},
    });
    SPDLOG_WARN("wf_marker_with_fields");
    const auto line = findLine(readLogFile(), "wf_marker_with_fields");
    REQUIRE(!line.empty());
    REQUIRE(line.find(R"("device_id":"dev0")") != std::string::npos);
    REQUIRE(line.find(R"("queue":"rx")") != std::string::npos);
}

TEST_CASE("with_fields: consumed after one line; subsequent lines empty", "[observability][with_fields]") {
    signalforge::observability::init_logging();
    signalforge::observability::clear_fields();

    signalforge::observability::with_fields({{"tag", "first"}});
    SPDLOG_INFO("wf_marker_line_a");
    SPDLOG_INFO("wf_marker_line_b");

    const auto content = readLogFile();
    const auto line_a = findLine(content, "wf_marker_line_a");
    const auto line_b = findLine(content, "wf_marker_line_b");
    REQUIRE(!line_a.empty());
    REQUIRE(!line_b.empty());
    REQUIRE(line_a.find(R"("tag":"first")") != std::string::npos);
    REQUIRE(line_b.find(R"("fields":{})") != std::string::npos);
}

TEST_CASE("with_fields: per-thread state is isolated", "[observability][with_fields]") {
    signalforge::observability::init_logging();
    signalforge::observability::clear_fields();

    // Worker thread sets its own fields but never logs. Worker's TLS is
    // destroyed at thread exit. Main thread's TLS is unaffected.
    std::thread worker([] { signalforge::observability::with_fields({{"who", "worker"}}); });
    worker.join();

    signalforge::observability::clear_fields();
    SPDLOG_INFO("wf_marker_main_isolation");
    const auto line = findLine(readLogFile(), "wf_marker_main_isolation");
    REQUIRE(!line.empty());
    REQUIRE(line.find(R"("fields":{})") != std::string::npos);
    REQUIRE(line.find(R"("who")") == std::string::npos);
}

TEST_CASE("with_fields: second call replaces not appends", "[observability][with_fields]") {
    signalforge::observability::init_logging();
    signalforge::observability::clear_fields();

    signalforge::observability::with_fields({{"a", "1"}});
    signalforge::observability::with_fields({{"b", "2"}});
    SPDLOG_INFO("wf_marker_replaced");
    const auto line = findLine(readLogFile(), "wf_marker_replaced");
    REQUIRE(!line.empty());
    REQUIRE(line.find(R"("b":"2")") != std::string::npos);
    REQUIRE(line.find(R"("a":"1")") == std::string::npos);
}

TEST_CASE("with_fields: JSON special chars escaped", "[observability][with_fields]") {
    signalforge::observability::init_logging();
    signalforge::observability::clear_fields();

    signalforge::observability::with_fields({
        {"quoted", R"(value with "quotes")"},
        {"path", R"(C:\path)"},
    });
    SPDLOG_INFO("wf_marker_escaped");
    const auto line = findLine(readLogFile(), "wf_marker_escaped");
    REQUIRE(!line.empty());
    REQUIRE(line.find(R"("quoted":"value with \"quotes\"")") != std::string::npos);
    REQUIRE(line.find(R"("path":"C:\\path")") != std::string::npos);
}

TEST_CASE("with_fields: clear_fields is a no-op on empty state", "[observability][with_fields]") {
    signalforge::observability::init_logging();
    signalforge::observability::clear_fields();
    REQUIRE_NOTHROW(signalforge::observability::clear_fields());
}
