# M2 — Concerns

This file records concerns, deviations, and locally-blocked verifications
raised during M2 execution.

## Local debug-asan host block

**Status**: known, pre-existing from M0/M1.

The development host has `/etc/ld.so.preload` configured such that the
AddressSanitizer runtime cannot be loaded first. Running the `debug-asan`
preset's test binaries locally produces:

```
==NNN==ASan runtime does not come first in initial library list; you
should either link runtime to your application or manually preload it
with LD_PRELOAD.
```

Per CLAUDE.md §Required-2:

> AddressSanitizer and UBSan violations: verified on the `debug-asan`
> preset when the local host permits; otherwise CI is the authoritative
> gate (document the local block in `.claude/M<n>-concerns.md`).

**Decision**: the `debug-asan` **build** passes locally (all M2 code
compiles clean under `-fsanitize=address,undefined`). Only the **runtime
test execution** is blocked. CI runs the `debug-asan` test pass and is
authoritative for ASan/UBSan violations.

Applies to every M2 subtask commit. Will not be re-reported per-subtask.

---

## Spec §2.2-4 "patch 5" errata — resolved by 2026-04-23 clarification

Superseded by the nine M2 spec clarifications (commit 24e089b) merged into
milestone/M2 at 6e1a2b5. Clarification 9 replaced "patch 5" with "§4.6".

---

## Logger synchronous, not async

**Background**: architecture §14.1 lists logging as "spdlog (async,
rotating file sink)". M2 S9 requires `with_fields(...)` to attach
structured fields to the next `SF_LOG_*` line on the same thread.

**Technical conflict**: spdlog's async mode runs the pattern formatter
on a background thread. A formatter that reads `thread_local` state
set by the caller thread cannot see that state in async mode — each
thread has its own TLS. spdlog's MDC header at
`_deps/spdlog-src/include/spdlog/mdc.h` explicitly says "Not supported
in async mode." The only supported alternatives within spdlog 1.14
would either embed fields into the message payload (forbidden by spec
§4.6.1 as "string-concatenated fields") or require a newer spdlog
version with async MDC support (none available as of 1.14.1, the
pinned version).

**Decision**: switched the logger in `src/observability/logging.cpp`
to synchronous (`spdlog::logger` instead of `spdlog::async_logger`).
The custom `%J` pattern flag formatter runs on the caller thread,
reads `thread_local` fields, and emits them into the `fields` JSON
slot. Pattern integrity is preserved.

**Impact**: each `SF_LOG_*` call now blocks on disk write (rotating
sink's mutex + write syscall). V1 use cases log sparsely — at most a
few hundred lines/minute during normal operation. For high-volume
streams (e.g., per-frame trace), the caller would typically use trace
level with SPDLOG_ACTIVE_LEVEL stripping, not per-frame INFO. If
future milestones show logging as a hotspot, a re-evaluation with a
newer spdlog (MDC-async support) or a custom queue design would be
warranted.

**Spec compatibility**: architecture v0.8 would ideally re-state §14.1
to drop "async". This is a minor deviation; no ADR is required unless
the human requests one at M2 close.

---

