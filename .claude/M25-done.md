# M25 — C1 fix (closure report)

Branch: `milestone/M25` (local, off `milestone/M24`; **not pushed**). Fixes concern C1 (slow-signal
publish latency) raised in M21.

## What shipped

A **time-based publish flush** in the M6 `SignalBuffer`'s internal push/publish path: a segment
publishes when **either** 100 samples accumulate **or** ≥ 200 ms of *sample-time* has elapsed
since the last publish. Lazy-anchored on the first push, so clustered fast pushes (the existing
tests, microseconds apart) still publish strictly on the 100-sample cadence.

| File | Change |
|---|---|
| `src/buffer/signal_buffer.cpp` | `kPublishFlushInterval` (200 ms); `lastPublishTime_` + `publishAnchorValid_`; publish trigger now `cadenceHit || timeHit` |
| `tests/unit/buffer/signal_buffer_time_flush_test.cpp` | slow-signal visibility + fast-cadence-preserved (2 cases) |

Commits: `(plan)` → `<S1>` → (this) close-out.

## Effect

- A 1 Hz signal's value appears after the **2nd** sample (~1 s) instead of the 100th (~100 s).
  Benefits every panel that reads `queryLatestOne` / `queryRange` (Numeric, State, Table, Bar,
  Gauge, Plot).
- Fast signals (≥ ~500 Hz): cadence still dominates — unchanged behavior + performance.

## Verification

- New test green (6 assertions). **All 48 buffer/publish/query tests still pass** — no cadence
  regression (lazy anchor preserves the contract). Debug + Release build green; full ctest green.
- clang-format clean.

## Freeze note

The M6 `SignalBuffer` **public API is untouched**. The change is to `TypedBuffer` publish
internals, which the spec (§6.2 / `signal_buffer.hpp`) explicitly places **outside the M6 freeze
surface**. Not a HALT #4 trigger.

## Status

C1 resolved on `milestone/M25` (local, unpushed). Chain: `main → M21 → M22 → M23 → M24 → M25`.
With this, the P2/P3/C1 sequence is complete; the remaining DR-001 items are the menu-bar (#4)
and status-bar (#5) IA rewrite.
