# M25 — Buffer slow-signal publish latency (C1) — understanding

## Source

Fixes concern **C1** (surfaced in M21, `.claude/M21-concerns.md`): the M6 `SignalBuffer`
publishes to readers only every `kDefaultPublishCadence` = 100 samples, with the time-based
flush an explicit deferred TODO. So a slow signal (e.g. 1 Hz) takes ~100 s before its value
appears in any panel (Numeric/State/Table/Bar/Gauge/Plot) — bad for exactly the slow scalars
those panels target. Continues the dashboard work; branched off `milestone/M24`.

## What M25 delivers

A **time-based publish flush** in the buffer's internal push/publish path: a segment publishes
when **either** 100 samples have accumulated **or** ≥ `kPublishFlushInterval` (200 ms) of
*sample-time* has elapsed since the last publish.

## Freeze scope

The M6 `SignalBuffer` **public API is frozen**, but the spec (§6.2, restated in
`signal_buffer.hpp`) explicitly puts **`TypedBuffer` polymorphism / publish internals OUTSIDE
the freeze surface** ("may evolve"). The change is internal (push/publish behavior); no public
signature or schema changes. Not a HALT #4 trigger.

## Key design points

- Interval is measured from **sample timestamps** (`push(t, …)`), not wall clock — monotonic
  with data, resume-safe, and avoids `Date.now`-style nondeterminism.
- **Lazy anchor:** on the first push the publish-time anchor is set to `t` *without* publishing.
  So the existing buffer tests (which push 100 samples microseconds apart) still publish
  strictly on the 100-sample cadence — the time flush never fires for clustered fast pushes.
- 200 ms interval chosen to sit above the existing tests' densest spacing (the Int64 query
  test's 1 ms × 200 spans 200 ms and already publishes on cadence at 100), so no existing
  publish-cadence test changes.

## Effect

Slow signal at 1 Hz: value visible after the **2nd** sample (~1 s) instead of the 100th
(~100 s). Fast signals (≥ ~500 Hz): cadence still dominates — unchanged behavior + perf.

## DoD

New `signal_buffer_time_flush_test.cpp` proving slow-signal visibility + fast-cadence
preserved; all existing buffer tests still green; Debug+Release build & ctest green;
clang-format clean; Doxygen on any touched decls; progress current; local commits (no push).
