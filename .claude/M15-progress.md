# M15 — Progress

Live state for `milestone/M15` execution. Updated by CC after
each subtask close + on each operator interaction.

Source: `.claude/M15-understanding.md` + `.claude/M15-plan.md`
+ `.claude/M15-concerns.md`. Spec:
`docs/milestones/M15-vision-infrastructure.md`. Charter:
`docs/V0-series-charter.md`.

---

## Subtask state

| ID | Title | Status | Commits | Notes |
|---|---|---|---|---|
| S0 | Concerns C1-C7 + M15.2 local-only hybrid lock + empirical CC native test | done | `2fe5034` | Empirical Read-tool test PASSED on SignalForge PNG; M15.2 locked |
| S1 | Screenshot capture infrastructure (mechanism C in-process + B xvfb+xwd stub) | done | `d82d630` | `MainWindow::captureScreenshot` + `--capture-screenshot-*` CLI flags + `tests/visual/` Python harness + pixel-diff comparator + `scripts/accept-baseline.sh` + ctest `visual` label. Test 609 passes; M14 S1 + mechanical-18 still PASS |
| S2 | Vision-LLM integration | done | `ae2e453` (+ S2 fixes `f5db33f`, `1b7db01`, `671c865`) | Schema + validator + CC-native prompt template + optional MiMo API wrapper (urllib stdlib; never CI) + 3 visual tests (empty, with-connection, chart-with-signal). 611/611 ctest |
| S3 | Baseline coverage (Y-scope; 38 baselines per C3) | in progress | Round 1 `e94a656`, Round 2 `03d929d`, V0.3 hand-off `8266e4c`, Round 3 (this commit) | Round 1: 6 captures (00, 04, 05, 14, 15, 33). Round 2: 3 more (02 PASS, 12 PASS, 13 FLAKY 0.999 %). Multi-chart 01/36/37 deferred to V0.3 per operator decision A (HALT-20260510T172100Z; rebuild segfault is V1 production code, spec §2.2 #1 forbids UX fixes during M15). Round 3: 4 replay captures (17, 18, 19, 20) PASS via autoLoadReplaySession + autoReplayPlay/Pause/SeekPercent primitives + a bootstrap helper that records a 849 B session fixture from m14_smoke + udp_fixture_sender. **13 / 38 captured (12 PASS + 1 FLAKY)**, 10 still feasible via Round 4 (mech B for menus/dialogs), 12 operator-manual. Round 4 in flight. |
| S4 | Test framework integration: extend M14 S1 + mechanical-18 + visual-test suite | not started | — | Per C4 layout |
| S5 | CI integration (artifact upload + pixel-diff gate + accept-baseline.sh) | done | `b55203e` | `.github/workflows/ci.yml` uploads `tests/screenshots/**` as `visual-screenshots-<preset>` artifact (14 day retention) on every run via `if: always()`. `tests/visual/README.md` documents add-test workflow, baseline accept loop, local-only vision-LLM hybrid. Pixel-diff gate validated locally: absent baseline → matched, identical → 0%, regressions caught at strict thresholds. C7 verified: zero `secrets.*` references, no API key in CI. `accept-baseline.sh` already shipped in S1. CI run `25634337073` green (11m25s, all 3 presets). **Order-violation note**: S5 was executed before S3 post-compaction; per operator feedback (`feedback_plan_ordering.md`), plan ordering is the source of truth — re-read M15-plan before picking next subtask. |
| S6 | CC autonomy demonstration | not started | — | End-to-end self-test |
| S7 | M15-done.md + V0.3 hand-off | not started | — | Coverage map + V1 UX gap inventory + industrial refs |

---

## Phase 5 amendment carried forward (C7 reminder)

**Public-repo security: CI runs no vision LLM.**

- Zero `secrets.*` references in any
  `.github/workflows/*.yml` for vision-LLM purposes
- No API keys committed to the repo (`.env*` gitignored)
- Vision-LLM calls are local-only (CC's Read tool, optional
  MiMo via operator-local API key)
- CI gates: pixel-level baseline diff only (deterministic;
  no LLM verdict)
- **HALT trigger H7**: any accidental API key reference or
  vision-LLM call from CI → HALT immediately (security
  violation), revert + audit logs

CC self-checks every CI workflow change against this
constraint at S5.

---

## Quality > schedule discipline (V0 charter §4 + §8)

V0.2 has **no calendar commitment**. M15 closes when all 7
hard-stop criteria from `M15-understanding.md` §3 hold:

1. Screenshot capture works headless + locally
2. Vision-LLM returns reliable structured JSON; ≥ 3 visual
   tests use it
3. State-machine-complete baseline coverage captured;
   operator-approved one-time
4. M14 S1 + mechanical-18 extended with screenshots; GUI
   subset automated; M14-deferred items revisited
5. CI uploads screenshot artifacts; PR diff display +
   accept-baseline.sh workflow
6. CC autonomy end-to-end demonstration documented
7. M15-done.md published with V0.3 hand-off

Until all 7 hold, M15 stays open. No premature close.

---

## Frozen-surface modifications (C6 counter)

| Counter | Limit (HALT #5) | Status |
|---|---|---|
| **0 / 2** | > 2 → HALT | clean baseline |

Baseline reset at M15 S0 entry. Per V0 charter §3, M2-M12
frozen `.hpp` files (per `docs/v1.0-spec-list.md` §1) remain
frozen for V0.2 + V0.3. ADR + counter bump required for any
modification. `main_window.hpp` is **not** frozen (V1
integration point); M15 additions there are unrestricted.

| # | File | sha256 pre / post | Commit | ADR |
|---|---|---|---|---|

(empty — no M15 frozen-surface modifications yet)

---

## Empirical test artifact (S0 evidence)

`/tmp/m15-s0-empirical/chart.png` — 661×720 PNG (3 463 bytes)
captured from M14 S1 fixture. CC Read tool described
contents accurately:

- White background canvas (chart-pane framebuffer)
- Bright orange QSGSimpleRectNode ~60×60 at (10, 10)
  (the SF_F4_DIAG diagnostic rect from ADR-010)
- No chart line (xvfb + software-RHI rasterization
  limitation per ADR-010 Path α)

Confirms M15.2 lock: CC native multimodal Read tool is
sufficient for SignalForge GUI screenshot description in V0.2.

---

## §S3 — 38-baseline checklist

Round 1 (this commit) captures the 6 currently-feasible
mechanism-C states. Round 2+ will add MainWindow primitives
+ CLI flags + fixtures to lift the remaining 32 from
`manual` to mechanism-C / mechanism-B captures.

Capture orchestrator: `tests/visual/scripts/capture_baselines.py`.
Output: `tests/screenshots/baseline-candidate/<state>.png`
(gitignored; published as `visual-screenshots-<preset>` CI
artifact for operator review per S5).

Stability gate: each PASS state captured 3× under software
RHI; consecutive-run pixel diff < 0.5 % at 4-channel
tolerance.

| # | State | Mechanism | Round | Status | Diff |
|---|---|---|---|---|---|
| 00 | empty-launch | C | 1 | PASS | 0.002 % |
| 01 | empty-with-chart | C → manual | (HALT) | DEFERRED | Round 2 added `autoAddCharts` primitive + `--auto-add-charts` flag, but `rebuildChartWidgets()` segfaults under headless tight-loop chart-add timing (HALT-20260510T172100Z). 3 fix attempts (sync, defer 0/400 ms) all crashed. Tracked as pre-existing GUI rebuild fragility for V0.3 fix. |
| 02 | conn-udp-idle | C | 2 | PASS | 0.000 % — `autoLoadFixtureNoConnect` primitive (Round 2) |
| 03 | conn-udp-connecting | manual | — | SKIP | UDP bind ≈ instant; transient state not observable under headless capture. Operator-manual. |
| 04 | conn-udp-connected | C | 1 | PASS | 0.000 % |
| 05 | conn-udp-with-signal | C | 1 | PASS | 0.002 % |
| 06 | conn-udp-disconnecting | manual | — | SKIP | Transient; same constraint as 03. |
| 07 | conn-udp-error | manual | — | SKIP | Needs intentional bind-fail (port already bound) or driver fault injection. Operator-manual or Round 4 fault-injection fixture. |
| 08 | conn-serial-idle | manual | — | SKIP | Hardware required (or socat virtual pty extension). Out-of-scope for headless CI; operator-manual. |
| 09 | conn-serial-connected | manual | — | SKIP | Same constraint as 08. |
| 10 | conn-tcp-idle | manual | — | SKIP | Needs TCP-server fixture coordinator (not yet built). |
| 11 | conn-tcp-connected | manual | — | SKIP | Same constraint as 10. |
| 12 | multi-2-drivers | C | 2 | PASS | 0.002 % — `m15_multi_2.yaml` fixture (Round 2) |
| 13 | multi-5-drivers | C | 2 | FLAKY | 0.999 % — `m15_multi_5.yaml` fixture (Round 2). Operator review: signal-selector layout under software-RHI varies slightly run-to-run; flagged for tighter capture timing or layout-stabilisation pass. |
| 14 | recording-active | C | 1 | PASS | 0.002 % |
| 15 | recording-stopped | C | 1 | PASS | 0.002 % |
| 16 | replay-open-dialog | B | 4 | PENDING | modal QFileDialog; needs full-screen grab + slot trigger |
| 17 | replay-loaded | C | 3 | PASS | 0.000 % — `autoLoadReplaySession` primitive + bootstrapped fixture (`m15-replay-fixture.sfreplay`, 849 B) |
| 18 | replay-playing | C | 3 | PASS | 0.000 % — `autoReplayPlay` primitive; play fires 500 ms post-load |
| 19 | replay-scrubber-mid | C | 3 | PASS | 0.002 % — `autoReplaySeekPercent(50)` primitive |
| 20 | replay-end | C | 3 | PASS | 0.002 % — `autoReplaySeekPercent(100)` primitive |
| 21 | replay-speed-5x | B | 3+4 | PENDING | needs replay-load + speed flag + B-mech for combo dropdown |
| 22 | mode-live-to-replay | B | 4 | PENDING | modal confirm; needs sequence trigger |
| 23 | mode-replay-to-live | B | 4 | PENDING | modal 3-option; needs replay-loaded prerequisite + sequence trigger |
| 24 | dialog-add-serial | B | 4 | PENDING | modal; needs `--auto-open-add-conn-dialog=serial` flag |
| 25 | dialog-add-udp | B | 4 | PENDING | modal; needs `--auto-open-add-conn-dialog=udp` flag |
| 26 | dialog-edit | B | 4 | PENDING | modal; needs `--auto-open-edit-conn-dialog=<id>` flag |
| 27 | dialog-quit-recording | manual | — | SKIP | Triggered by closeEvent during recording; Qt close-event injection is brittle headlessly. Operator-manual. |
| 28 | dialog-recording-error | manual | — | SKIP | Needs SessionWriter::start fault injection. Operator-manual or Round 4 fault-injection. |
| 29 | dialog-replay-error | manual | — | SKIP | Needs malformed session-file injection. Operator-manual or Round 4. |
| 30 | menu-file-open | B | 4 | PENDING | menu popup is separate top-level window; needs full-screen grab + slot-trigger to open menu |
| 31 | menu-connections-open | B | 4 | PENDING | same as 30 |
| 32 | menu-session-open | B | 4 | PENDING | same as 30 |
| 33 | status-buffer-normal | C | 1 | PASS | 0.002 % |
| 34 | status-buffer-warn | manual | — | SKIP | timing-dependent; needs traffic-flooding fixture. Operator-manual or Round 4 traffic-gen fixture. |
| 35 | status-buffer-full | manual | — | SKIP | same as 34 plus drop-overflow timing. |
| 36 | multi-chart-2 | C → manual | (HALT) | DEFERRED | same `rebuildChartWidgets()` segfault as 01; HALT-20260510T172100Z |
| 37 | multi-chart-5 | C → manual | (HALT) | DEFERRED | same `rebuildChartWidgets()` segfault as 01 / 36 |

**Round 3 summary** (cumulative): PASS 12 / FLAKY 1 / DEFERRED-V0.3 3 / PENDING-rounds 10 / SKIP-operator-manual 12 of 38.

(Round 2 alone: PASS 8 / FLAKY 1 / DEFERRED-V0.3 3 / PENDING-rounds 14 / SKIP-operator-manual 12. Round 1 alone: PASS 6 / SKIP-pending-rounds 21 / SKIP-operator-manual 11.)

### S3 HALT — multi-chart capture deferred to V0.3

3 baseline states deferred to V0.3 due to V1 GUI rebuild fragility:

- `01-empty-with-chart`
- `36-multi-chart-2`
- `37-multi-chart-5`

**Root cause**: `MainWindow::rebuildChartWidgets()` (`src/app/main_window.cpp:561`) tears down existing chart `QQuickWidget` host(s) via `deleteLater()` and synchronously constructs new hosts + re-parents the existing `Chart` `QQuickItem*` children. Under headless tight-loop chart-add timing (`--auto-add-charts <n>` invoking `autoAddCharts`), the deleted host's QML scene-graph teardown races against the new host's `setSource()` init on the same thread, terminating the process with SIGSEGV before the screenshot QTimer fires. Cf. ADR-010 §"Implementation lesson" — chart QQuickWidget hosting was identified as fragile early in M13.

**Three fix attempts** (all crashed identically):

1. Synchronous invocation post `window.show()`, before `app.exec()`.
2. `QTimer::singleShot(0, ...)` (event loop fires immediately).
3. `QTimer::singleShot(400, ...)` after a refactored bulk `createChart()` loop + single `rebuildChartWidgets()` call.

**Reproducer for V0.3**:

```sh
cmake --build --preset release
xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" \
    timeout 8 ./build/release/src/app/signalforge \
        --auto-add-charts 1 --exit-after-ms 4000
# expected: clean exit at ~4s with 2 chart panes
# actual:   SIGSEGV before exit-after-ms, no screenshot saved
```

**Why deferred not fixed**: spec §2.2 #1 forbids UX fixes during M15. The crash is in V1 production GUI rebuild logic; real-user click cadence does not trigger it (M0–M14 operator usage + M14 audit run-1..run-7 dogfood — no chart-rebuild crash reported). V0.2 vision infrastructure correctly surfaced the V1 reliability gap that V0.2 cannot fix per scope; this is exactly the M15-charter intent (infrastructure exposes UX/reliability gaps; V0.3 fixes them).

**V0.3 hand-off**: V0.3 chart layout redesign (M17 widget rebuild per preliminary roadmap) will refactor or replace `rebuildChartWidgets()` to be safe under any chart-add timing. Recommended approach for V0.3 (CC's read; not binding):

- Append-only multi-chart: never tear down existing host widgets; create a new `QQuickWidget` per newly-added `Chart`, leave existing host/Chart pairs untouched.
- OR migrate to QML-driven multi-chart layout where `Chart` items are children of a single QML repeater bound to `chartManager_->chartIds()`, eliminating the C++-side teardown loop entirely.

After V0.3 chart redesign lands, capture states 01 / 36 / 37 by running `tests/visual/scripts/capture_baselines.py 01 / 36 / 37` (no script change needed — the manual mark in `capture_baselines.py:specs_phase_b_skipped()` is the only gate).

**Infrastructure preserved for V0.3** (committed in `03d929d`):

- `MainWindow::autoAddCharts(int extra)` primitive (`main_window.cpp:autoAddCharts`).
- `--auto-add-charts <n>` CLI flag (`main.cpp` `autoAddChartsArg` block).
- Capture mechanism C tested + working for single-chart + connection states.

These are no-harm to production paths (`--auto-add-charts` is a test-only flag; the production toolbar `+ Chart` action remains unchanged at `onAddChart()`). V0.3 chart redesign re-tests the same flag once the rebuild logic is safe.



**Operator-manual count**: 11 (states 03, 06, 07, 08, 09, 10, 11, 27, 28, 29, 34, 35 — though 12 are listed, one is borderline). Above the < 5 target from session prompt; the gap is tracked as Round 4 / V0.3 work.

**Per-round plan**:
- Round 2: chart-add primitive + no-connect fixture mode + multi-driver fixtures → 6 states (01-as-conceptually-distinct, 02, 12, 13, 36, 37). _Note: 01 may collapse into 00 unless V0.3 disambiguates._
- Round 3: replay infrastructure (session-file fixture + 4 replay-control primitives) → 5 states (17, 18, 19, 20; 21 needs B too).
- Round 4: mechanism B (full-screen grab via `QApplication::primaryScreen()->grabWindow(0)`) + dialog/menu slot triggers → 9–11 states (16, 21–26, 30–32; possibly 22, 23, 27–29 with fault injection).
- Genuinely operator-manual: 03, 06, 07, 08, 09, 10, 11, 34, 35 (transient timing / hardware / extreme-pressure timing).

After Round 4 the operator reviews ≥ 27/38 candidate baselines + commits the operator-manual residual as a one-time hand-capture session.

---

## HALT log

- `HALT-20260510T172100Z-m15-s3-rebuildcharts-segfault.md` — M15 / S3 Round 2 multi-chart capture (states 01, 36, 37). Trigger #2 (3 fix attempts). `rebuildChartWidgets()` segfaults under headless tight-loop chart-add timing; pre-existing GUI rebuild fragility surfaced. Awaiting operator decision on A (defer V0.3) / B (in-scope rebuild redesign — would violate spec §2.2 #1) / C (timing workaround — brittle).

---

## Cross-references

- Spec: `docs/milestones/M15-vision-infrastructure.md`
- Understanding: `.claude/M15-understanding.md`
- Plan: `.claude/M15-plan.md`
- Concerns: `.claude/M15-concerns.md`
- V0 charter: `docs/V0-series-charter.md`
- V0.1 status: `docs/v0.1-status-summary.md`
- M14 done: `.claude/M14-done.md`
- M14 mechanical-18 (extension target):
  `tests/integration/gui/run_mechanical_18.sh`
- M14 S1 smoke (extension target):
  `tests/integration/gui/release_binary_smoke.sh`
