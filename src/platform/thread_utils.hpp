// src/platform/thread_utils.hpp
#pragma once

#include <QString>

namespace signalforge::platform {

/// Set the current thread's name. The name is visible in debuggers, `top`,
/// `htop`, `ps -T`, and Linux `/proc/<pid>/task/<tid>/comm`.
///
/// Linux constrains thread names to 15 bytes plus NUL. Inputs longer than
/// 15 UTF-8 bytes are truncated at the byte boundary and a warning is logged
/// via `SF_LOG_WARN`.
///
/// Thread safety: affects only the calling thread. Safe to call from any
/// thread.
void setCurrentThreadName(const QString& name);

/// Suggest CPU affinity for the current thread. Advisory only — the kernel
/// may schedule elsewhere, and some sandboxes reject the syscall entirely.
/// The caller tolerates a no-op.
///
/// `core` is a 0-based logical-CPU index. Out-of-range values (negative, or
/// greater than the detected CPU count) result in a no-op with an
/// `SF_LOG_WARN`; the function never throws.
///
/// Thread safety: affects only the calling thread. Safe from any thread.
void suggestCpuAffinity(int core);

}  // namespace signalforge::platform
