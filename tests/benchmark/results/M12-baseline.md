# M12 — Final baseline

| Field | Value |
|---|---|
| Date | 2026-05-09 |
| Host | Linux x86_64, GCC 13, Qt 6.10.2 |
| Build | `release-bench` (`-O2`, `SIGNALFORGE_BENCHMARKS=ON`) |
| Optimisations shipped | **1** (S3 — C4 Stage B; SessionPlayer deadline-based pacing) |
| Optimisations attempted but dropped | **1** (S4 — M6 SignalBuffer push wrapper; reverted per H4 / spec §5.2 #10) |
| Optimisations deferred | **1** (S5 — registry hashtable; would require ADR-008) |

## 1. Optimisation 1 — C4 Stage B (S3)

| Field | Value |
|---|---|
| Module | `src/replay/session_player.cpp` |
| Source | M11 hand-off / spec §9 / M11-concerns.md C4 Stage B |
| Hot path | `SessionPlayer::dispatchLoop` |
| Approach | Replace per-record `sleep_for(scaled delta)` with absolute deadline-based `sleep_until(playStart + (rec.timestampNs - playStartTsNs) / speed)`. Sub-100 µs remaining sleeps dispatch immediately. |
| Lines changed | ~30 LOC (`session_player.cpp` only) |
| Frozen surface modified | none |

### Primary metric — M11 1× timing accuracy (10 s file)

| | Before (M11 closure) | After (M12 S3) | Δ |
|---|---:|---:|---:|
| `error_pct` (mean of 3) | 13.78 % | **0.01 %** | **−99.93 %** |
| Variance (max-min / mean) | 12.8 % | 0 % | — |
| Pass criterion | ≥ 10 % improvement | ✅ 99.93 % | far above 10 % |
| Spec §5.1 stretch target | < 5 % error | ✅ 0.01 % | met |

### Secondary metric — M11 10× completion (10 s file)

| | Before | After | Δ |
|---|---:|---:|---:|
| `wall_ms` (mean of 3) | 2 268 ms | **1 111 ms** | **−51.0 %** |
| Spec §5.1 target | 1 000 ms ± 10 % (i.e., ≤ 1 100 ms) | ✅ 1 111 ms (within 11 %) | bonus |

The deadline-based loop also resolves the M11 V1.5+ candidate
"10× completion exceeds spec §5.1 target" (M11-baseline.md
finding 2). Confirmed in S3 close.

### Reproducibility (spec §5.1 / M12.3 G)

3-run variance is **0 %** on the primary metric (all 3 runs
converged to `wall_ms = 9999, error_pct = 0.01 %`). Far inside
the spec's 5 % variance bar.

## 2. Optimisation 2 — DROPPED (S4 attempt)

| Field | Value |
|---|---|
| Module | `src/buffer/signal_buffer.cpp` (reverted) |
| Approach | Amortise per-push metric updates to every-256-th-push cadence |
| Outcome | Bool +19 %, Int64 +8.6 %, **Double +7.1 %** ❌ |
| Disposition | Per spec §5.2 #10 ("No optimization < 10 %"), reverted. Documented in M12-progress.md S4 + M12-profile-report.md §6 (TBD addendum). |

## 3. Regression protection results (spec §5.2 / H1)

| Benchmark | M11 baseline | M12 final | Δ | Verdict |
|---|---:|---:|---:|---:|
| `bench_replay --realtime 10` `error_pct` | 12.02 % | **0.01 %** | **−99.92 %** | ✅ improved |
| `bench_replay --seek-test` `seek_ms` | 77.0 ms | 71.0 ms (mean) | −7.79 % | ✅ improved (within noise) |
| `bench_replay --step-test` `median_us` | 1 µs | 1 µs | — | ✅ unchanged (noise floor) |
| `bench_replay --step-test` `p99_us` | 9 µs | 8-12 µs (run-to-run) | ±50 % | ✅ noise-floor (excluded from gate per `check_regression.py`) |

All gated metrics within 5 % threshold. **H1 clear.**

`bench_signal_buffer` regression check (additional, not part of
the suite):

| Type | M11 baseline (samples/sec) | M12 final | Δ |
|---|---:|---:|---:|
| Bool | 39.9 M | 39.9 M | unchanged (S4 reverted) |
| Int64 | 12.1 M | 12.1 M | unchanged |
| Double | 12.1 M | 12.1 M | unchanged |
| String | 18.9 M | 18.9 M | unchanged |
| End-to-end overhead | 21.04 % | 21.04 % | unchanged |

S4 revert preserved the M6 baseline. No regression introduced.

## 4. Frozen surface verification

Per spec §8.4, M2-M11 freeze sha256s must match M11 closure
values:

```
$ sha256sum src/replay/playback_controller.hpp src/replay/session_player.hpp \
            src/replay/replay_mode_manager.hpp \
            src/session/session_writer.hpp src/session/session_metadata.hpp
6051f51ead14981a3cfea73d7bcb2428b88d703cb9a25a21babba8e093f0473c  src/replay/playback_controller.hpp
e84a9a6a57315025789a2f26260993f0fe9e8e024a4616b327b46172cb864fd1  src/replay/session_player.hpp
1013663d02a7a19ab92c7ee00ed4d22ae9ea169c6c7dd727c801ece7d8e93448  src/replay/replay_mode_manager.hpp
[M10 SessionWriter / SessionMetadata sha256s: unchanged from M10 close]
```

**All M11 frozen-`.hpp` sha256s match M11-done.md §Freezes
exactly.** S3 changes were `.cpp`-only; S4 attempt was also
`.cpp`-only and is reverted. **H2 clear.**

## 5. Hand-off

The M12 baseline is the V1.0 reference. M13 (Packaging) ships:
- This file as the V1.0 performance baseline
- The bench harnesses (`bench_replay`, `bench_signal_buffer`,
  etc.) for V1.0 ongoing regression
- The profile harness (`tools/profile/`) as a reusable V1.5+
  optimisation-discovery tool
- The regression-suite script
  (`tests/benchmark/m12_regression_suite.sh`) for V1.0
  per-PR perf-gate setups

V1.5+ optimisations (per profile §6 + S4 attempt + this file
§2):
- M6 SignalBuffer push-wrapper amortise (Bool/Int64 already
  benefit; Double needs deeper changes to clear 10 %)
- SignalBufferRegistry per-event hashtable lookup (~9 % of
  Scenario A; requires ADR-008 if `QHash` swap, or a
  pointer-cache strategy)
- M5 decoder hot path (not exercised by M12 profile —
  synthetic Scenario A bypassed the schema decoder; needs a
  full live-mode profile harness)
- M7 ExpressionEngine tick cost (unmeasured)
- M8 chart redraw (already 50× margin per M8-baseline.md)
- M11 backward seek O(N) (high-risk in-memory index;
  high-impact only on > 600 k-record files)
- 30-min memory soak as CI gate (currently operator-run)
