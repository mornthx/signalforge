// tools/crash_test/main.cpp
//
// Deliberate-crash tool for manual verification of sentry-native's
// crash reporting path. NOT integrated into unit tests.
//
// Usage:
//   ./crash_test null_deref      # dereference nullptr
//   ./crash_test abort           # std::abort()
//   ./crash_test throw           # unhandled std::runtime_error
//   ./crash_test stack_overflow  # infinite recursion
//
// After a crash, inspect the crashdump directory for a .dmp file. See
// README.md for the full verification procedure.

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sentry.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

std::filesystem::path resolveDatabaseDir() {
    if (const char* xdg = std::getenv("XDG_STATE_HOME"); xdg != nullptr && *xdg != '\0') {
        return std::filesystem::path{xdg} / "signalforge" / "crashdumps";
    }
    const char* home = std::getenv("HOME");
    const std::filesystem::path base = (home != nullptr && *home != '\0') ? std::filesystem::path{home} : "/tmp";
    return base / ".local" / "state" / "signalforge" / "crashdumps";
}

void initSentry() {
    const auto dbDir = resolveDatabaseDir();
    std::filesystem::create_directories(dbDir);

    sentry_options_t* options = sentry_options_new();
    sentry_options_set_database_path(options, dbDir.c_str());

    // The crashpad_handler binary is installed next to the crash_test
    // executable by the CMake build; resolve relative to the executable's
    // directory via /proc/self/exe.
    std::array<char, 4096> exePath{};
    const auto len = ::readlink("/proc/self/exe", exePath.data(), exePath.size() - 1);
    if (len > 0) {
        const std::filesystem::path exeDir =
            std::filesystem::path{std::string{exePath.data(), static_cast<std::size_t>(len)}}.parent_path();
        const auto handler = exeDir / "crashpad_handler";
        if (std::filesystem::exists(handler)) {
            sentry_options_set_handler_path(options, handler.c_str());
        }
    }

    sentry_options_set_release(options, "SignalForge@crash-test");
    // DSN intentionally unset: local minidumps only, no upload.

    if (sentry_init(options) != 0) {
        std::cerr << "crash_test: sentry_init failed; continuing without crash reporting\n";
    }
}

[[noreturn]] void nullDeref() {
    volatile int* p = nullptr;
    std::cerr << "crash_test: dereferencing nullptr...\n";
    *p = 42;       // intentional crash
    std::abort();  // unreachable but satisfies [[noreturn]]
}

[[noreturn]] void abortCrash() {
    std::cerr << "crash_test: calling std::abort...\n";
    std::abort();
}

[[noreturn]] void throwCrash() {
    std::cerr << "crash_test: throwing runtime_error (unhandled)...\n";
    throw std::runtime_error("crash_test deliberate throw");
}

[[noreturn]] void stackOverflow(int depth) {
    volatile char buf[4096];  // keep the frame non-tail-optimizable
    buf[0] = static_cast<char>(depth & 0xFF);
    stackOverflow(depth + 1);
    std::abort();  // unreachable
}

void usage() {
    std::cerr << "crash_test <mode>\n"
              << "  null_deref       — dereference nullptr\n"
              << "  abort            — std::abort()\n"
              << "  throw            — unhandled std::runtime_error\n"
              << "  stack_overflow   — infinite recursion\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 2;
    }
    initSentry();

    const std::string mode{argv[1]};
    if (mode == "null_deref") {
        nullDeref();
    }
    if (mode == "abort") {
        abortCrash();
    }
    if (mode == "throw") {
        throwCrash();
    }
    if (mode == "stack_overflow") {
        stackOverflow(0);
    }
    usage();
    sentry_close();
    return 2;
}
