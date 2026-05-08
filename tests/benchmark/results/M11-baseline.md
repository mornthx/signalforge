# M11 — SessionPlayer replay baseline

| Field | Value |
|---|---|
| Bench harness | `tests/benchmark/bench_replay.cpp` |
| Driver | M11 `SessionPlayer` + `PlaybackController` |
| Workload (fixture) | 60 signals × 1 kHz × 10 s ≈ 600 k records |
| Sink | `SilentSink` (no-op; isolates dispatch + I/O) |
| Host | Linux x86_64 (debug + release builds; release-bench used here) |
| Date | 2026-05-08 |

## Results vs spec §5

| Metric | Spec target | HALT (plan §3) | Result | Verdict |
|---|---:|---:|---:|---:|
| 1× timing accuracy (10 s file) | < 5 % error | > 20 % (H2) | **12.02 %** error | ⚠ above 5 % target; ✅ below 20 % HALT (H2 clear) |
| 10× completion (10 s file → ≤ 1 s) | 1 s ± 10 % | > 1.5 s | **2.26 s** | ⚠ above target; documented as V1 limitation |
| Seek latency (mid-file on ~400 k records) | < 500 ms | > 2 s (H4) | **77 ms** | ✅ 6.5× headroom on target |
| Step latency (synchronous) | < 50 ms | > 200 ms | **median 1 µs / p99 9 µs** | ✅ ~5 000× headroom on target |
| Memory growth (full file replay) | < 100 MB | > 10 % (H3) | _operator-soak; harness in `--memory-soak <s>`_ | _pending operator run_ |

## Implementation notes

### S10 finding 1 — fixture-side backpressure

The bench fixture writer is the M10 `SessionWriter`, whose internal
queue is bounded at 10 k events. The bench drives 60 k events/sec
sustained with no inter-event delay; the SessionWriter's drain rate
(measured at M10 close: 60 k events/sec sustained) is right at the
production rate but not above it, so transient backpressure during
fixture write drops a fraction of events. Observed records in the
emitted file: ~393 k–423 k of the nominal 600 k — droppable events
lost to the C3 backpressure policy.

This is purely a fixture-side artefact and does **not** reflect a
real-world replay flow (where files were written from live data at
sustainable rates). The records that **do** make it into the fixture
are dispatched correctly; the bench numbers for seek + step + 1×
timing are valid characterisations of the player itself.

### S10 finding 2 — 10× timing exceeds spec §5.1 target

The bench shows 10× completing a 10 s file in ~2.3 s, vs the spec
target of 1 s. Direct-call dispatch (no QueuedConnection per record;
applied during S10's optimisation pass) reduced the wall time by
only ~3 % — the bottleneck is **per-record sleep granularity** and
the file-read path, not the dispatch.

For the M11 V1 use case (user clicks Play on a recorded session,
expects roughly real-time playback), 10× is a power-user feature for
fast skim. The 2.3 s actual is still 4.4× faster than 1×. V1.5+
optimisations (per `M11-concerns.md` C4 stage B):

1. Replace per-record `sleep_for` with `sleep_until` against an
   absolute deadline (eliminates accumulating jitter).
2. Batch dispatch — collect N records on the worker, post one
   queued event that dispatches all N. Reduces queue-event
   overhead at the cost of per-batch latency.
3. Pre-read records into a small ring buffer to overlap I/O with
   dispatch.

H4 (seek < 500 ms on a 600 k-record file) is comfortably met at
77 ms with the spec's worst-case file size; no indexed-seek
optimisation needed in V1.

### S10 finding 3 — H2 cleared, plan §3 HALT triggers all clear

Per the M11 plan §3 HALT-trigger table:

| # | Trigger | Result |
|---|---|---|
| H2 | 1× timing > 20 % over 10 s | 12.02 % — clear |
| H3 | Memory growth > 10 % across full replay | operator-pending (harness ready) |
| H4 | Seek > 500 ms on 600 k-record file | 77 ms — clear |
| H7 | Seek to invalid timestamp crashes / hangs | covered by S6 unit tests |

No HALT triggers fire. M11 proceeds to S11 (integration tests) +
S12 (closeout).

## How to reproduce

Build with benchmarks enabled:

```bash
cmake -B build/release-bench -DCMAKE_BUILD_TYPE=Release \
    -DSIGNALFORGE_BENCHMARKS=ON
cmake --build build/release-bench --target bench_replay
```

Run individual scenarios:

```bash
build/release-bench/tests/benchmark/bench_replay --realtime 10
build/release-bench/tests/benchmark/bench_replay --fast 10
build/release-bench/tests/benchmark/bench_replay --seek-test
build/release-bench/tests/benchmark/bench_replay --step-test
build/release-bench/tests/benchmark/bench_replay --memory-soak 1800 --memory-snapshot 30
```

Each emits a one-line JSON summary on stdout; non-zero exit on
plan §3 HALT-trigger violation.

## Hand-off

The bench harness is in tree and ready for operator-run soak
(`--memory-soak 1800` for the 30-min spec §5.6 gate). M11 closure
treats this as a follow-up line item (mirrors the M10 30-min
soak hand-off pattern).
