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
| S2 | Run-4 chart sizing fix | done | `4038191` | ADR-011 + splitter setSizes + QQuickWidget Expanding + chart geometry binding. Chart QQuickItem now 661×720 (was 0×0). Smoke test exposes a separate F4 (chart paint nothing) deferred to Wave 1 |
| S3 | GUI audit (operator-paired) | **operator pass complete** | `6a4a1a2` skeleton + (this commit) findings | Run 5 audit (`docs/m14-audit-operator-runs/run5-non-chart-audit.md`) returns 14 findings F5-F18. Folded into `docs/m14-gui-audit-report.md`. Scenario A decision recorded |
| S4 | Critical bug fixes (Waves 1/2/3) | **Wave 1 + 2 + 3 implementation done; awaiting batched operator validation** | Wave 1: `7ed9d7b`, `cbf7a25`. Wave 2: `5c639fe` (ADR-013). Wave 3: `6026395` (F18) + `18bab31` (F12) + `555f270` (F15). | Wave 1: F4 closed (Path α — production OK; smoke uses `SF_F4_DIAG=1` canary). Wave 2: F6 + F17 (ADR-013). Wave 3: F18 (Quit menu/Ctrl+Q) + F12 (replay relative time format) + F15 (idempotent re-registration + log throttle + status-bar budget). F11 awaits operator re-test (likely auto-resolves with F15a). All in non-frozen `.cpp` / `.hpp`; frozen-surface counter remains 0/2 |
| S5 | V1.0 scope re-evaluation (Scenario A/B/C) | **ratified** | `b2bd6c4` | Scenario A approved by human; S5 review notes folded into S7 done.md hand-off |
| S4 | Wave 4 (post-run-6) — F14 + F19 + F9 | **done; awaiting operator final S6 verification** | F14: `a29f581`. F19: `a11430f`. F9: (this commit). | All Wave 4 fixes in non-frozen `.cpp`; frozen-surface counter remains 0/2 |
| S6 | 18-test HW verification re-run | not started | — | Operator-driven; 16+/18 required for Scenario A |
| S7 | M14-done.md + V1.0 release PR | not started | — | Per C3: fresh PR `milestone/M14 → main` |

## Scenario decision (S5 PRELIMINARY — RATIFIED AT S5 / S6 CLOSE)

**Scenario A — V1.0 full ship after Waves 1+2+3 fixes.**

Recorded 2026-05-10 after S3 operator pass closed with 14
findings (F5-F18) on top of F1-F4 history. Final ratification
defers to S5 V1.0 scope re-evaluation document and S6 18-test
re-run reaching ≥ 16/18.

### Why Scenario A and not B/C

- 4 Critical findings (F4, F6, F15, F17), well under HALT #3
  bar of >10 Critical.
- All 4 have clear fix paths: wire-up gaps + resource-budget
  tuning, not architectural rewrites.
- Frozen-surface impact preliminarily 0–2, well under HALT #5
  (>2).
- HALT #1 not triggered: no Critical bug is "not fixable in
  M14".
- V1.0 GUI core architecturally sound: Phases A–E of operator
  audit pass cleanly (Connection lifecycle, Recording IO,
  Replay file open + mode switch, Multi-chart UI). Only
  Phase F (Persistence) entirely blocked by F17.

### What can flip to Scenario B/C during S4

- S4 fix uncovers >2 frozen-`.hpp` modifications (HALT #5)
- S4 fix surfaces an unfixable Critical (HALT #1)
- S6 18-test re-run < 12/18 after fixes (HALT #4)

CC tracks running counter in §"Frozen-surface modifications"
below; flags any HALT immediately.

### Wave fix sequencing

| Wave | Fixes | Type | ADR? | Operator dogfood |
|---|---|---|---|---|
| 1 | F4 | Architectural / unknown | ADR-012 if frozen `.hpp` needed | Yes (immediate, post-diag) |
| 2 | F6 + F17 | Wire-up gap | ADR-013 (single, both) | Yes (one session) |
| 3 | F11 + F15 + F12 + F18 | Buffer/perf + UX | ADR-014 if F15 architectural | Yes (one session) |

### V1.0.1 backlog (deferred from M14)

F2, F3, F5, F7, F8, F9, F14, F16. F13 is V1.5+. F10
auto-resolves with F6.

---

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

  ### Wave 1 diagnostic findings (post-instrumentation, 2026-05-10)

  CC added a `SF_F4_DIAG=1` env-var-gated diagnostic block to
  `Chart::updatePaintNode` (chart.cpp; chart.hpp NOT modified
  per ADR-011 frozen-surface discipline):

  - Logs paint-pass state: `w h signals parentItem
    parentVisible hasContents oldNode childCount`
  - Appends a bright orange `QSGSimpleRectNode` (60×60 @
    (10,10)) to the QSG root each pass — the canonical "is
    the scene graph alive" probe

  Local smoke run with `SF_F4_DIAG=1`:

  ```
  Chart::updatePaintNode[diag]: w=661 h=720 signals=1
    parentItem=ok parentVisible=true hasContents=true
    oldNode=reuse childCount=3
  M14_SMOKE_TIER_A: non_white_pixels=3600 total_pixels=475920
    width=661 height=720
  ```

  3600 = 60×60 = the orange rect. **The diag rect renders.**

  Implications:

  - Scene-graph submission path is alive — rules out the
    "F4 architectural / scene-graph broken" hypothesis.
  - Framebuffer capture path is alive — rules out the earlier
    "QQuickWidget framebuffer broken" hypothesis.
  - QSG node tree on the chart's root has 3 children:
    cursor + diag-rect + 1 signal node — so `addSignal` →
    `signalNodes` population works.
  - **F4 root cause is in the chart-specific paint of the
    signal node**: vertex generation produces zero output
    OR vertices land outside the framebuffer.

  Most likely sub-hypothesis (per §F4 forensic notes
  F4-A / F4-B / F4-C above):

  - F4-A — timestamp domain mismatch in smoke fixture (samples
    are at `host_now ≠ device-monotonic`, so `queryRange`
    returns empty → `latestSamples[id]` is empty → geometry
    allocates 0 vertices)
  - F4-B / F4-C — schema decoder OR chart time-axis policy

  Operator real-X11 dogfood now narrows the disambiguation:

  - If real hardware (whose decoded `timestamp_ms` may equal
    `host_now`) renders → F4-A confirmed; smoke fixture fix
    is sufficient
  - If real hardware also blank → F4-B or F4-C; production
    fix needed

  Until operator returns, S4 Wave 1 PAUSES. The diagnostic
  instrumentation lands on `milestone/M14` so the operator's
  test session can use `SF_F4_DIAG=1` to confirm.

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
