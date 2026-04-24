// tests/unit/platform/thread_utils_test.cpp
#include "observability/logging.hpp"
#include "platform/thread_utils.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <pthread.h>
#include <sys/prctl.h>
#include <thread>

using namespace signalforge::platform;

namespace {

std::string readThreadComm() {
    std::array<char, 32> buf{};
    (void)prctl(PR_GET_NAME, buf.data(), 0, 0, 0);
    return std::string{buf.data()};
}

}  // namespace

TEST_CASE("thread_utils: setCurrentThreadName applies short name", "[platform][thread_utils]") {
    signalforge::observability::init_logging();
    setCurrentThreadName("sf_short");
    REQUIRE(readThreadComm() == "sf_short");
}

TEST_CASE("thread_utils: setCurrentThreadName truncates long names to 15 bytes", "[platform][thread_utils]") {
    signalforge::observability::init_logging();
    setCurrentThreadName("abcdefghijklmnopqrstuvwxyz");  // 26 bytes
    const auto got = readThreadComm();
    REQUIRE(got.size() <= 15);
    REQUIRE(got == "abcdefghijklmno");
}

TEST_CASE("thread_utils: setCurrentThreadName handles empty input safely", "[platform][thread_utils]") {
    signalforge::observability::init_logging();
    setCurrentThreadName("");
    // prctl(PR_SET_NAME, "") empties the comm to "" — accept either empty or
    // pre-existing value; the important guarantee is "no crash".
    const auto got = readThreadComm();
    (void)got;
    SUCCEED();
}

TEST_CASE("thread_utils: setCurrentThreadName is thread-local", "[platform][thread_utils]") {
    signalforge::observability::init_logging();
    setCurrentThreadName("main_name");

    std::string otherName;
    std::thread t([&] {
        setCurrentThreadName("worker");
        otherName = readThreadComm();
    });
    t.join();

    REQUIRE(otherName == "worker");
    REQUIRE(readThreadComm() == "main_name");
}

TEST_CASE("thread_utils: suggestCpuAffinity accepts valid core", "[platform][thread_utils]") {
    signalforge::observability::init_logging();
    // Core 0 must exist on any machine that runs the test.
    REQUIRE_NOTHROW(suggestCpuAffinity(0));
}

TEST_CASE("thread_utils: suggestCpuAffinity no-ops on out-of-range core", "[platform][thread_utils]") {
    signalforge::observability::init_logging();
    REQUIRE_NOTHROW(suggestCpuAffinity(-1));
    REQUIRE_NOTHROW(suggestCpuAffinity(10'000));
}
