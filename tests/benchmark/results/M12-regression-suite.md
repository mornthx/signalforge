# M12 — Regression suite

| Field | Value |
|---|---|
| Script | `tests/benchmark/m12_regression_suite.sh` |
| Checker | `tools/profile/check_regression.py` |
| Threshold | 5 % per spec §3.4 J / plan §3 H1 |
| Baselines | M11 closure (hard-coded in `check_regression.py`) |
| Strict mode | `--strict` exits non-zero (code 3) on regression |
| Wall-time | ~30 s for the M11 bench-replay subset (full M0-M11 sweep is ~30 min when soak modes are included) |

## What runs

The script delegates to existing JSON-emitting bench tools under
`tests/benchmark/`. Currently:

- `bench_replay --realtime 10` — M11 1× timing accuracy
- `bench_replay --seek-test` — M11 seek latency on ~600 k records
- `bench_replay --step-test` — M11 step latency p99

`bench_session_writer` (default mode) is **excluded** from the
per-commit cycle because its 10-second sustained-write workload
adds significant wall-time. Operator-run when soak validation
is needed (M10 30-min soak + M11 30-min soak hand-offs).

`bench_signal_buffer` integration with the harness is added at
S4 when the M6 push-wrapper optimisation lands (see
M12-progress.md S4).

## Regression-cycle discipline

Per M12 plan §0 + concerns C3:

| Commit | Run regression suite? |
|---|---|
| Pre-S0 understanding+plan | no (docs-only) |
| S0 concerns | no (docs-only) |
| S1 profile harness | yes (validates M11 baselines hold) |
| S2 profile report | no (docs-only) |
| **S3 optimisation 1 (C4 Stage B)** | **yes — must show ≥ 10 % primary metric improvement AND all baselines within 5 %** |
| **S4 optimisation 2 (M6 push)** | **yes — same as S3** |
| S5 (deferred) | n/a |
| S6 final baseline + integration tests | yes (full sweep) |
| S7 done.md | no (docs-only) |

Net regression cycles for M12: **3-4** (S1, S3, S4, S6).

## Exit codes

| Code | Meaning |
|---|---|
| 0 | All matched metrics within threshold |
| 3 | At least one matched metric regressed > threshold (strict mode) |
| 64 | Build dir not found |
| 65 | Current output file empty / malformed |

## How to run

```bash
# From repo root, after building bench targets:
./tests/benchmark/m12_regression_suite.sh build/release-bench --strict
```

`--strict` is recommended for per-commit gating; without it,
regressions report but the script exits 0 (informational mode,
useful while iterating on an optimisation in S3 / S4).

## Hand-off

This suite is the M12 regression gate. After M12 closes,
M13 (Packaging) should keep this script in tree as the V1.0
performance-regression entry point. V1.5+ may extend the
metric set (currently scoped to the M11-closure-known
baselines hard-coded in `check_regression.py`).
