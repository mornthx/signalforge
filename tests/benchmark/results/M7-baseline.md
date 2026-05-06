# M7 baseline

> Acceptance gate: tick wall time **p50 = 2.14 ms / p95 = 2.22 ms /
> p99 = 2.39 ms** (mean of 3 independent runs) on host shuai-Laptop.
> All three targets (`p50 < 5 ms`, `p95 < 8 ms`, `p99 < 10 ms`) met
> with 2-4× margin. **All scenarios pass.** HALT trigger #3
> (`p99 > 15 ms after one optimization pass`) not fired — the first
> measurement was already within budget, so no optimization pass was
> needed.

## Run conditions

- Branch: `milestone/M7` at the S9 commit (`2bb539d`).
- Preset: `release-bench` (`-DSIGNALFORGE_BENCHMARKS=ON`,
  `CMAKE_BUILD_TYPE=Release`, GCC 13.3, C++23, `-O2`, no ASan).
- Host: shuai-Laptop, x86_64 Linux 6.8, glibc 2.39.
- Binary: `build/release-bench/tests/benchmark/bench_expression_engine`.
- 3 independent runs.

## Scenario — 30 Hz tick wall time (spec §5.4 / plan §S10)

500 base signals (all `double`) registered with a custom-budget
`SignalBufferRegistry` (2 GB to fit 500 1-second 1 kHz buffers ×
~8 KB each at the default sizing). 100 derived expressions, each
referencing 5 random base signals via the formula
`b_a + b_b * b_c - b_d + b_e`. 1000 ticks; per-tick wall time
captured via `std::chrono::steady_clock` immediately around the
`onTick()` invocation.

| Run | p50 (ms) | p95 (ms) | p99 (ms) | max (ms) | mean (ms) |
|---:|---:|---:|---:|---:|---:|
| 1 | 2.1351 | 2.2175 | 2.4426 | 2.8720 | 2.1455 |
| 2 | 2.1343 | 2.2032 | 2.3207 | 2.8489 | 2.1422 |
| 3 | 2.1367 | 2.2334 | 2.3994 | 2.8632 | 2.1500 |
| **mean** | **2.135** | **2.218** | **2.387** | **2.861** | **2.146** |

Run-to-run variance: p50 0.002 ms (0.1%), p95 0.030 ms (1.4%),
p99 0.122 ms (5.1%). The p99 dispersion is driven by occasional
allocator / scheduler noise; even the worst observed p99 (2.443 ms)
clears the 10 ms target by 4×.

| Statistic | Value | Target | Headroom |
|---|---:|---:|---:|
| p50 | 2.14 ms | < 5 ms | 2.3× |
| p95 | 2.22 ms | < 8 ms | 3.6× |
| p99 | 2.39 ms | < 10 ms | 4.2× |

Per-tick budget breakdown (back-of-envelope from the 2.14 ms p50):

- 100 expressions × 5 sources = 500 source lookups (`bufferFor` +
  `queryLatestOne`) per tick. At ~2 µs/lookup this contributes
  ~1.0 ms.
- 100 expressions × 1 exprtk evaluation each. exprtk's compiled
  expression executes ~5-10 ops in <1 µs typical, so ~0.1 ms total.
- 100 derived `registry.onSignal` pushes (incl. metric counter
  updates). ~5 µs each → ~0.5 ms.
- Allocations (per-tick `sources` vector reuse: zero-alloc on the
  hot path), per-expression metric updates, and snapshot publish
  cadence at 100-tick boundaries account for the remainder.

## HALT trigger #3 status

**Not fired.** The very first build hit `p99 = 2.4 ms`, well below
the 10 ms target and far below the 15 ms HALT threshold. No
optimization pass was needed; the spec's per-spec §7-3 "one
optimization pass" allowance remains unused.

## Acceptance verdict

✅ **All scenarios pass.** The 30 Hz tick has comfortable headroom
on the spec's 33.33 ms per-tick budget — the engine consumes
~6.4% of the 30 Hz wall budget at this load (100 expressions, 500
base signals).

## Hand-off notes

- The scenario builds expressions via `ExpressionValidator` to match
  the production registration flow; the validator reports zero
  errors and the topological sort produces a valid evaluation order
  every run.
- `evaluations_total = 100 000` (= 100 × 1000) and
  `evaluation_errors = 0` confirms every expression evaluated
  successfully every tick — no NaN propagation, no exprtk runtime
  errors.
- The 5-source-per-expression formula was kept simple
  (`+`, `*`, `-`) by design; spec §5.4 permits this since the goal
  is to measure the engine's per-tick orchestration cost, not
  pathological-formula pricing.
