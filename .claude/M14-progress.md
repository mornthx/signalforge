# M14 — Progress

Live state for `milestone/M14` execution. Updated by CC after
each subtask close + on each operator audit-section pass.
Operator findings posted as numbered lists under §"Audit
findings (S3)".

Source: `.claude/M14-understanding.md` + `.claude/M14-plan.md`
+ `.claude/M14-concerns.md`. Spec:
`docs/milestones/M14-gui-audit.md`.

---

## Subtask state

| ID | Title | Status | Commits | Notes |
|---|---|---|---|---|
| S0 | Concerns C1-C6 + PR #24 closure | done | `8515137` | M14-concerns.md, M14-progress.md scaffold, PR #24 closed-with-supersede |
| S1 | CI release-binary smoke test (Tier A + Tier B) + framework | done | `536ff91` | Harness catches run-4 0×0 + the new F4 paint-visibility bug. Regression-protect verification still deferred (now to S4-close: needs F4 fixed to establish a passing baseline) |
| S2 | Run-4 chart sizing fix | **CC-side done; smoke still WILL_FAIL** | (this commit) | ADR-011 + splitter setSizes + QQuickWidget Expanding + chart geometry binding. Chart QQuickItem now 661×720 (was 0×0). Smoke test exposes a separate F4: chart paints no visible content even with correct geometry — deferred to S4 |
| S3 | GUI audit (operator-paired) | not started | — | Per C2: daily ping-pong by spec §3.2 section |
| S4 | Critical bug fixes | not started | — | Per-bug commit (M14.3 P); ADR-011+ for architectural changes |
| S5 | V1.0 scope re-evaluation (Scenario A/B/C) | not started | — | Collaborative; CC drafts, human finalizes |
| S6 | 18-test HW verification re-run | not started | — | Operator-driven; 16+/18 required for Scenario A |
| S7 | M14-done.md + V1.0 release PR | not started | — | Per C3: fresh PR `milestone/M14 → main` |

## Scenario decision discipline (C5 reminder)

Per spec M14.5 X, V1.0 ships in one of three forms after
audit. **CC must NOT pre-commit to Scenario A.**

The decision is made post-audit, written collaboratively
with the human, finalized in `docs/v1.0-scope-evaluation.md`
at S5.

Triggers that should bias toward Scenario B / C:

- Architectural fix requires modifying > 2 frozen `.hpp`
  files (HALT H5)
- `> 10` Critical bugs in S3 audit (HALT H3)
- 18-test HW verification `< 12/18` (HALT H4)
- Audit reveals fundamental unfixable issues (HALT H1)

Until S5: do **not** phrase commits / progress updates /
done.md drafts as if Scenario A is the outcome. Use neutral
language ("the V1.0 scope decision in S5 will determine
…").

## Frozen-surface modifications (C6 counter)

| Counter | Limit (HALT H5) | Status |
|---|---|---|
| **0 / 2** | > 2 → HALT | OK — clean baseline |

Baseline reset at M14 S0. ADR-008's additive method on
M5-frozen `decoder_registrar.hpp` is part of `milestone/M13`
ancestry already merged into the M14 base branch and is **not
counted** against the M14 budget. Any new frozen-`.hpp`
modification by M14 commits must be appended to the table
below before commit; if appending would push count to `> 2`,
HALT #5 fires.

| # | File | sha256 pre / post | Commit | ADR |
|---|---|---|---|---|

(empty — no M14 frozen-surface modifications yet)

## Audit findings (S3)

Operator posts findings here, one section per pass. CC folds
findings into `docs/m14-gui-audit-report.md` and into S4 fix
commits as they land.

### Pass 1 — (date / section TBD)

(awaiting first operator pass; S3 begins after S1 + S2 close)

### Incidental findings during S1+S2 harness development (2026-05-09)

These were observed while building the S1 smoke harness and the
S2 chart-sizing fix. Logged here so they are not lost; severity +
S4 fix priority will be assigned during the S3 audit pass.

- **F1 (Critical) — RESOLVED in S2** (`<S2 commit>`). Chart
  QQuickWidget framebuffer was 0×0 because the splitter pane,
  the QQuickWidget size policy, and the Chart QQuickItem
  geometry binding all collapsed to 0 width. Fixed by
  ADR-011: splitter `setSizes({256, 1024})` + QQuickWidget
  `setSizePolicy(Expanding,Expanding)` + chart syncSize lambda
  bound to host root's `widthChanged`/`heightChanged` signals.
  Smoke now reports `width=661 height=720` consistently.
- **F2 (Serious?)** — Segfault during shutdown after
  `--exit-after-dump` drives `QApplication::quit()`. The dump
  line is logged successfully and `SignalForge exiting, rc=0` is
  logged before the crash. Stderr shows offscreen-platform-
  specific `QPainter::begin: Paint device returned engine == 0,
  type: 3` plus warnings about `propagateSizeHints` / `raise()`.
  Root cause TBD; severity to be decided in S3 (does not block
  CI smoke; the harness's checks all run off the log file
  before the crash).
- **F3 (Serious?)** — `UdpDriver destroyed in non-Idle state;
  callers should close() first` warning on shutdown. Connection
  lifecycle is not closing the driver before destruction. May
  be related to F2.
- **F4 (Critical, NEW)** — Chart paints no visible content even
  when correctly sized. Surfaced by S2: with the chart at
  661×720, with the temperature signal added, with 200 UDP
  frames decoded into the SignalBufferRegistry, with the chart's
  redraw timer firing 120+ times, the QQuickWidget framebuffer
  comes back entirely white. Diagnostic confirmed:
  - `signals=1` on the chart
  - `SignalBufferRegistry has 9 signal id(s)` including
    `udp:m14-smoke-udp/temperature`
  - Chart `pos=(0,0) visible=true enabled=true opacity=1`
  - QQuickWidget background-clear-color test (240,240,240)
    DID register in the framebuffer → grab path works

  ### F4 forensic notes (post-S2 chart.cpp read; NO fix
  attempted)

  Read `src/chart/chart.cpp` `updatePaintNode` + `onTick`
  per the user's S2 instruction. Observations:

  - `updatePaintNode` is the standard QQuickItem render path:
    builds a `QSGNode` tree of `QSGGeometryNode`s (one per
    visible signal) with `QSGFlatColorMaterial`. `setFlag(
    ItemHasContents, true)` is set in the ctor (line 141), so
    the QSG render thread WILL call `updatePaintNode`. Each
    signal's geometry is filled from `impl_->latestSamples`.
  - **If `latestSamples[signalId]` is empty, the node's
    geometry is allocated to 0 vertices** (line 406) —
    rendering nothing visible. This is the silent path to
    "chart sized 661×720 but pixels all white".
  - `onTick` (the 30 Hz redraw timer slot) populates
    `latestSamples` by calling
    `registry_->bufferFor(signalId)->queryRange(axisStart,
    axisEnd, pixelWidth)`.
  - `axisStart` / `axisEnd` come from `timeAxis_->visible
    Start() / visibleEnd()`. In live mode the time axis is
    anchored to the host's monotonic clock (~10 s window
    ending at "now").
  - **Likely F4 root cause**: timestamp domain mismatch
    between decoded sample timestamps and the chart's time
    axis. The smoke fixture's `udp_fixture_sender.py` sends
    `temperature_sensor` frames whose `timestamp_ms` field
    starts at 0 (`seq * 20`). If the SchemaDecoder uses the
    decoded `timestamp_ms` directly as the sample timestamp,
    samples land at "epoch + 0…4000 ms" while the chart's
    visible window is at "host_now − 10 s … host_now"
    (~e.g. 1762 e9 ns). `queryRange` returns empty →
    `latestSamples[signalId]` is empty → `updatePaintNode`
    paints nothing.

  ### Three candidate interpretations (resolved by S3
  operator pass)

  1. **F4-A — smoke fixture bug**. SchemaDecoder is correct
     (uses decoded `timestamp_ms`); the smoke harness is
     wrong (should send timestamps near `host_now`). Fix:
     update `udp_fixture_sender.py` to anchor `timestamp_ms`
     at `time.time_ns() / 1e6 - frame_count * 20`. No
     production-code change. Severity: **Serious** (smoke
     test only; doesn't block V1.0).
  2. **F4-B — SchemaDecoder timestamp policy bug**.
     SchemaDecoder should use host-receive-time (not
     decoded `timestamp_ms`) so live charts work without
     requiring device clocks to match host. Production fix
     in schema decoder. Severity: **Critical** for V1.0.
  3. **F4-C — chart time-axis policy bug**. Chart should
     auto-align its visible window to recent samples
     (rather than to host_now) so any monotonic device
     timeline renders. Production fix in `TimeAxisManager`
     or `Chart::onTick`. Severity: **Critical** for V1.0.

  S3 operator pass disambiguates: if real hardware
  connection (operator's actual board) renders fine → F4-A
  is the answer (M9 hardware happens to have device clock
  ≈ host clock, or the operator's device sends host-time).
  If real hardware also shows blank chart → F4-B or F4-C.

  Until S3 confirms, S1 smoke remains `WILL_FAIL TRUE` and
  CC does NOT attempt a fix.

## S1 deliverable evidence

Harness output against `milestone/M14` HEAD `aae2f4b` (= run-4 unfixed):

```
=== M14 S1 release-binary smoke ===
Tier A line: M14_SMOKE_TIER_A: non_white_pixels=0 total_pixels=0
              width=0 height=0
FAIL Tier A: chart framebuffer is entirely clear-color
  total_pixels=0 non_white_pixels=0
```

ctest treatment:
- `tests/integration/gui/CMakeLists.txt` registers the smoke with
  `set_tests_properties(... PROPERTIES WILL_FAIL TRUE)` so the
  expected Tier-A failure does NOT break ctest. Verified via
  `ctest --show-only=json-v1` showing `WILL_FAIL: True` on the test.
- 608/608 ctest pass on Debug + Release after S1 (was 607/607
  pre-S1; +1 new smoke case running in WILL_FAIL mode).
- debug-asan: build clean; local ctest blocked by the host
  `/etc/ld.so.preload` ASan-conflict (documented constraint;
  CI is authoritative).

S1 regression-protect verification (revert each ADR + confirm smoke
fails) is **deferred to S2 close** — without the run-4 fix the smoke
already fails on baseline, so the per-ADR-revert experiment has no
"green baseline" to revert from. After S2 lands, the verification
runs as part of S2 close before the WILL_FAIL property is removed.

## Frozen-surface modifications (S1 + S2 audit)

S1 added one public method group to `src/app/main_window.hpp`
(`autoLoadTestFixture`, `autoSelectSignal`, `grabChartImage`).
`main_window.hpp` is **not** in the M2-M12 frozen list per
`docs/v1.0-spec-list.md` §1 (it's the V1 integration point;
intentionally unfrozen).

S2 (ADR-011) modifies `main_window.cpp` only — `chart.hpp`
(M8-frozen) is intentionally untouched per ADR-011's "rejected
alternative B" (overriding `Chart::itemChange` would have
modified the M8 freeze). Counter remains **0 / 2**.

## HALT log

Track any HALT trigger fires here, with timestamp + cause +
resolution path. Empty so far.

## Cross-references

- Spec: `docs/milestones/M14-gui-audit.md`
- Understanding: `.claude/M14-understanding.md`
- Plan: `.claude/M14-plan.md`
- Concerns: `.claude/M14-concerns.md`
- M13 closure: `.claude/M13-done.md` §"M13 not release-ready
  — escalating to M14"
- Closed PR #24 — superseded by `milestone/M14`
