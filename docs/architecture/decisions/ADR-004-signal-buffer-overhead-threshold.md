# ADR-004 — SignalBuffer end-to-end overhead threshold revised based on measurement

**Status**: Accepted
**Date**: 2026-05-06
**Context**: M6 S11/S11.5 — measured end-to-end overhead exceeded the spec-stated thresholds; root-cause analysis showed the gap is structural, not algorithmic.

## Context

M6 spec §5.4 set the end-to-end overhead target at "≤ 5%" with a
HALT trigger at ">10%" (§7-4). These thresholds were established
during M6 spec authoring without architectural prototyping or
measurement.

S11 benchmark on `milestone/M6` revealed the actual overhead is
**33.22%** at first measurement. After one optimization pass (S11
metric update reductions), overhead remained ~33%. After Option A
(per-decoder buffer-pointer cache, S11.5), overhead landed at
**26.12%**.

Source decomposition (per HALT report at
`.claude/halt/HALT-20260506T081040Z-m6-e2e-overhead.md` and the
post-S11.5 update at
`.claude/halt/HALT-20260506T084448Z-m6-e2e-overhead-after-cache.md`):

- `SignalBufferRegistry` path overhead: ~10-15 ns/signal (mutex +
  map find, partially mitigated by S11.5 cache).
- `SignalBuffer::push` body: ~50-80 ns/signal (variant unpack +
  atomic counter + cadence check + LOD update — structural, hard
  to reduce without invasive refactor).
- Total: ~60-95 ns per signal vs counter baseline.

## Decision

Revise §7-4 HALT trigger from `>10%` to `>35%`. Revise §5.4
acceptance target from `≤ 5%` to `≤ 30%`. The 26.12% S11.5
measurement passes the new threshold with 4 percentage points of
margin.

The 30% / 35% values reflect:

- Realistic floor for the "per-event `SignalValueSink` + variant +
  LOD pyramid" architecture.
- 4 percentage points of margin above the measured value (host
  variance + future drift).
- 5 percentage points of HALT buffer above acceptance.

## Consequences

- **M6 acceptance gate passes** with the current S11.5 implementation.
  Closure proceeds.
- **M12 (Performance Optimization)** inherits a `SignalBuffer`
  overhead reduction goal. Profiler-driven optimization may target
  `SignalBuffer::push` body, the registry path, or the per-event
  `SignalValueSink` interface (potentially batched).
- **Real V1 workload performance is unaffected**: M6 throughput at
  ~1.5 M signals/sec leaves ~150× headroom over the expected 10 k
  signals/sec workload (10 drivers × 100 Hz × 10 signals/frame).
- **Future overhead measurements above 35% will trigger HALT** per
  the revised threshold.

## Alternatives considered

- **Option C (deeper refactor for sub-10% overhead)**: rejected.
  1-3 days of work for an uncertain outcome; not cost-effective when
  the real workload has 150× headroom and M12 already exists in the
  roadmap as the structurally correct home for cross-milestone
  perf debt.
- **New M6.1 remediation milestone**: rejected. M12 (Performance
  Optimization) already exists in the milestone roadmap and is the
  appropriate home for cross-milestone perf debt.
- **Accept HALT and stop M6**: rejected. M6 is functionally complete
  with 320 / 320 tests passing; blocking M7/M8/M10 indefinitely on a
  non-blocking perf gap is the wrong tradeoff.

## Precedent

This ADR is a measurement-driven correction analogous to ADR-002
(Crashpad → sentry-native after measuring build complexity). It is
**not** a "spec gate trip → relax threshold" workaround: the
revision is grounded in (a) reproducible measurement, (b) source
decomposition showing the gap is structural, and (c) impact
analysis showing real-workload performance is unaffected.

## Cross-references

- M6 spec §5.4 / §7-4 / §10 (amended in the same commit set as
  this ADR per the human's S11.6 authorization).
- HALT reports:
  - `.claude/halt/HALT-20260506T081040Z-m6-e2e-overhead.md` (first
    HALT, pre-cache, 33.22%).
  - `.claude/halt/HALT-20260506T084448Z-m6-e2e-overhead-after-cache.md`
    (second HALT, post-cache, 26.12%).
- Benchmark results: `tests/benchmark/results/M6-baseline.md`.
