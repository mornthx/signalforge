// src/platform/thread_utils.cpp
#include "platform/thread_utils.hpp"

#include "observability/logging.hpp"

#include <array>
#include <cstring>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <sys/prctl.h>
#include <thread>
#include <unistd.h>

namespace signalforge::platform {

namespace {

constexpr std::size_t kMaxThreadNameBytes = 15;  // Linux kernel limit (plus NUL)

std::string truncateUtf8ToByteLimit(const std::string& s, std::size_t limit) {
    if (s.size() <= limit) {
        return s;
    }
    // Walk back until we find the start byte of a UTF-8 code point so we do
    // not split a multi-byte sequence. A start byte is either 0xxxxxxx or
    // 11xxxxxx; continuation bytes are 10xxxxxx.
    std::size_t cut = limit;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    return s.substr(0, cut);
}

}  // namespace

void setCurrentThreadName(const QString& name) {
    const std::string utf8 = name.toStdString();
    const std::string truncated = truncateUtf8ToByteLimit(utf8, kMaxThreadNameBytes);
    const bool didTruncate = truncated.size() != utf8.size();

    std::array<char, kMaxThreadNameBytes + 1> buf{};
    std::memcpy(buf.data(), truncated.data(), truncated.size());
    buf[truncated.size()] = '\0';

    // prctl is preferred for htop/top/ps visibility; pthread_setname_np also
    // updates Linux thread comm but is limited to the same 15-byte window.
    (void)prctl(PR_SET_NAME, buf.data(), 0, 0, 0);
    (void)pthread_setname_np(pthread_self(), buf.data());

    if (didTruncate) {
        SF_LOG_WARN("thread_utils: thread name truncated to '{}' ({} bytes > {} limit)", buf.data(), utf8.size(),
                    kMaxThreadNameBytes);
    }
}

void suggestCpuAffinity(int core) {
    const long cpuCount = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (core < 0 || (cpuCount > 0 && core >= cpuCount)) {
        SF_LOG_WARN("thread_utils: CPU core {} out of range (detected {} online); ignoring affinity hint", core,
                    cpuCount);
        return;
    }

    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(static_cast<std::size_t>(core), &mask);

    const int rc = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
    if (rc != 0) {
        SF_LOG_WARN("thread_utils: pthread_setaffinity_np(core={}) failed rc={}; treating as no-op", core, rc);
    }
}

}  // namespace signalforge::platform
