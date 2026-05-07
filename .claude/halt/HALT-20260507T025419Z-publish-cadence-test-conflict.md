# HALT — M6 publish-cadence patch conflicts with concurrent-test consistency assertion

**Date**: 2026-05-07 02:54 UTC
**Branch**: `fix/m6-publish-cadence`
**Trigger**: HALT #4 (any M6/M7 test fails after patch → investigate root cause)
**Status**: Awaiting human direction on path forward

## What was done

1. Filed `docs/architecture/decisions/ADR-005-signal-buffer-publish-cadence.md`
   proposing adaptive publish cadence:
   `cadence = clamp(samplesRetained() / kPublishCadenceRatio,
   kPublishCadenceFloor, kPublishCadenceCap)` with
   floor=100, ratio=100, cap=5000.
2. Implemented the change in `src/buffer/signal_buffer.cpp` (no
   `.hpp` change; private `publishCadence_` field removed; cadence
   is computed inline at the publish-decision point).
3. Built `signalforge_buffer` clean, then ran the full release
   test suite.

## What broke

`tests/unit/buffer/signal_buffer_concurrent_test.cpp:34` —
**S9: 1 writer + 4 readers, 100k Double pushes, snapshot consistency**

```cpp
auto final = buf.queryLatest(1000);
REQUIRE(final.size() == 1000U);
REQUIRE(std::get<double>(final.back().value) == 99999.0);  // <- FAILS
```

Observed: `final.back().value == 99460.0` (539 samples behind).

## Root cause

The test asserts that immediately after the writer pushes
`kPushCount = 100 000` samples, `queryLatest(1000)` returns the
last 1000 samples with `back() == kPushCount - 1`. This assumes
**every push synchronously updates what readers can see**.

Under M6's original `cadence = 100` (constant), this was true to
within 100 samples — and at exactly N=100 000 samples, the last
publish landed at push 100 000 (since 100 000 mod 100 == 0), so
the segment did contain element 99999. The test passed.

Under the new adaptive cadence, at N=100 000 the cadence is 1 000.
The last publish in the trace landed at push 99 460
(`pushesSincePublish_` reached the then-current cadence of ~1 000
there; the next 540 pushes did not reach the new cadence again).

`queryRange`, `queryLatest`, and `queryLatestOne` all read from
the immutable published `Segment` — none of them consult the live
writer-side deque. So **all three reader APIs are now bounded-
stale** by `cadence - 1` samples. ADR-005's claim that
"`queryLatestOne` already bypasses the published Segment" was
**incorrect** — verified by reading
`LinearTypedBuffer::queryLatestOne` at `signal_buffer.cpp:598`.

This is a fundamental design tension between

- **HALT #4**: existing tests assume a strong consistency
  invariant ("after N synchronous pushes, queryLatest reflects
  push N").
- **HALT #5**: 1 M-sample push throughput ≥ 500 k samples/sec
  is unachievable under cadence=100 because per-publish O(N) copy
  becomes O(M²/100) total over an M-sample run.

The two requirements can't both be satisfied with the current
**lock-free, copy-on-publish, single-Segment** snapshot
architecture. We need either:

- (A) accept bounded staleness on `queryLatest` /
  `queryLatestOne` and update the consistency test to validate
  the new contract, **or**
- (B) change the storage architecture so publish is cheap (then
  cadence can stay at 100 and the test invariant holds), **or**
- (C) add a separate read path for live-state queries (e.g., a
  small mutex protecting access to the live tail) so
  `queryLatestOne` stays fresh while `queryRange` keeps the
  lock-free Segment path.

## Three options for the human to choose

### Option A — Accept staleness, update the test

- Code change: keep the cadence patch as is (already implemented
  on this branch, ~10 lines of change in `signal_buffer.cpp`).
- Test change: `signal_buffer_concurrent_test.cpp` — assert
  `final.back().value` is **within `cadence(N=kPushCount)` of
  `kPushCount - 1`** rather than exact equality. This validates
  the new ADR-005 contract.
- Update ADR-005 to correct the `queryLatestOne` claim.
- Pros: smallest diff. Hits 500 k samples/sec at 1 M (must verify
  with bench).
- Cons: changes a documented M6 contract that downstream consumers
  may rely on. CLAUDE.md anti-pattern flag: "Fixing tests by
  loosening assertions" — this would be the closest thing to it,
  even though it's actually validating a redesigned contract.
- **Estimate: 30 minutes.**

### Option B — Architectural fix (chunked storage)

- Replace `std::deque<T> values_` and `std::deque<timestamp>
  timestamps_` in `LinearTypedBuffer` with chunked storage:
  - `std::vector<std::shared_ptr<const std::vector<T>>> sealedChunks_;`
  - `std::vector<T> tailValues_;` (mutable, fills up to
    `kChunkSize` then seals into a `shared_ptr<const>`)
  - Equivalent for `timestamps_`.
- `publishSegment` then copies only the chunks vector
  (`O(chunkCount) = O(N/chunkSize)`) plus a snapshot of the tail
  (`O(chunkSize)`). At chunkSize=4096 and N=1 M, that's ~4 KB of
  copy per publish, regardless of N. Cadence can stay at 100.
- Update all 9 sites that touch `values_` directly (LOD bin
  emission's `values_[i]` indexing becomes a chunk lookup).
- Update eviction (front-trim chunks once fully evicted).
- BoolTypedBuffer (separate code path for bit-packed storage)
  may or may not need analogous treatment.
- Pros: preserves all existing test invariants. No staleness
  trade-off. Fundamentally faster (`O(M)` total for inserting
  M samples regardless of N).
- Cons: significant refactor (~300-500 LoC change in
  `signal_buffer.cpp`). Risk of subtle bugs in chunk math
  (off-by-one in eviction, LOD-vs-chunk index translation, etc.).
  Per CLAUDE.md, would be a structural rather than measurement-
  driven change.
- **Estimate: 4-6 hours focused work.**

### Option C — Add a live-state read path

- Keep cadence patch as is.
- Add a small mutex (or shared_mutex) protecting access to the
  live `values_` / `timestamps_` deques.
- Refactor `queryLatestOne` and `queryLatest` to acquire the
  shared lock and read directly from the live deques, bypassing
  the Segment. `queryRange` continues to use the immutable
  Segment for lock-free historical queries.
- Push acquires the exclusive lock briefly while modifying the
  deques.
- Pros: smaller change than Option B. Preserves all test
  invariants.
- Cons: breaks M6's documented "lock-free reads" promise (spec
  §3.6). Adds per-push lock acquire/release (~25 ns
  uncontended). The S9 concurrent test would now exercise actual
  contention between writer and 4 reader threads, and the
  per-push lock might noticeably affect throughput at 1 kHz +
  rates.
- **Estimate: 2-3 hours, including bench validation.**

## Recommendation

Option B is the architecturally correct answer. Option A is the
pragmatic-but-risky shortcut. Option C is a middle ground but
gives up the lock-free reads property that M6's design explicitly
called out.

If the human wants the patch done within 8 hours total (HALT
trigger #6), Option C is the lowest-risk fast path. Option B is
the right long-term answer but might bump up against the 8-hour
budget if surprises hit.

If the human prefers to defer the architectural fix to a future
M8 milestone (where chart-subsystem authoring will exercise the
buffer at scale and the chunked-storage redesign is a natural
companion to chart development), Option A's "document the
staleness in ADR-005, update the consistency test to match" is
the sensible interim.

## Repository state at HALT

- Branch: `fix/m6-publish-cadence` (local only; not pushed)
- Commits: none yet (changes are uncommitted local edits)
- Modified files:
  - `src/buffer/signal_buffer.cpp` (cadence formula change)
  - `docs/architecture/decisions/ADR-005-signal-buffer-publish-cadence.md` (new)
  - This HALT report
- Tests passing: 319 / 320 (only the S9 concurrent test fails)
- Frozen `.hpp` files: untouched (verified by `git diff` against
  M6 freeze list; HALT trigger #1 NOT fired)
- Reader API contracts: queryRange / queryLatest / queryLatestOne
  return-value SHAPES are unchanged; only the staleness of the
  values returned changes (HALT trigger #2 — value freshness is
  arguably part of the API contract; this is the contested point)

## Awaiting

Human direction on which option (A / B / C) to pursue, or
whether to roll back the cadence change entirely and document the
issue as a known M8 follow-up.
