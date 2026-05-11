# M15 Done — V0.2 Closure (AI Vision Infrastructure)

| Field | Value |
|---|---|
| Status | **V0.2 vision infrastructure landed; AI perception loop closed** |
| Authority | `docs/V0-series-charter.md` |
| Closing PR | `milestone/M15` → `main` (this PR) |
| Tag (Phase 3) | `v0.2.0` (internal milestone; no GitHub Release publish per charter §5) |
| Successor | M16+ (V0.3 — industrial UI/UX rebuild) |
| Date | 2026-05-11 |

This document is the M15 milestone hand-off + V0.3 unlock
checklist. Per V0 charter §2.2: "V0.2 reached at M15 close.
AI sees the GUI. V0.3 design pass can proceed against measured
baselines, not operator visual dogfood. Tag `v0.2.0` (internal
milestone; no GitHub Release publish). Audience: internal
developer (operator) + CC sessions."

---

## 1. Outcome

**M15 closes as V0.2 vision infrastructure.** The V0 charter §1
perception loop ("AI sees the GUI") closes end-to-end:

1. **CC captures GUI**: `MainWindow::captureScreenshot` (mech C
   window grab) + `captureFullScreen` (mech B QScreen grab)
   + `tests/visual/lib/capture.py` orchestrator + per-state
   primitives (`autoLoadFixtureNoConnect`, `autoAddCharts`,
   `autoLoadReplaySession`, `autoReplayPlay/Pause/SeekPercent`,
   `autoOpenMenu`, `autoShow{Add,Edit}ConnectionDialog`,
   `autoReplaySpeedComboPopup`).
2. **CC describes GUI**: multimodal Read tool (primary) +
   optional MiMo API benchmark (operator-local; never in CI).
3. **CC diagnoses regression**: S6 demo proved end-to-end on a
   deliberately-introduced `tr("&Quit")` → `tr("&Exit")`
   regression — pixel-diff at the 5 % gate produced a 0.019 %
   diff (false-PASS); Read tool caught the semantic regression;
   CC proposed + verified the fix.
4. **CC proposes fix**: documented in
   `docs/m15-cc-autonomy-demo.md` with reproduction recipe.

V0.3 redesign now has measured baselines (12 production-fidelity
PNGs at `tests/visual/baselines/`) to A/B against — operator
visual dogfood no longer the gating mechanism for design-pass
iteration.

### Closure gates

- [x] M15 implementation pushed on `milestone/M15`
- [ ] CI green on `milestone/M15` (blocked by cross-environment
  font/style drift between operator's local-xvfb baselines and
  CI's Ubuntu-24.04-xvfb; remediation path documented below)
- [x] Operator's R7 production-fidelity reclassification accepted
- [x] V0 charter governs (`docs/V0-series-charter.md`)
- [x] CC autonomy demo recorded
  (`docs/m15-cc-autonomy-demo.md`)
- [ ] Human Phase 2 approval of this PR
- [ ] Phase 3 (CC autonomous): merge PR + tag `v0.2.0`

---

## 2. Subtask deliverables (S0 → S7)

| ID | Title | Status | Closing commits |
|---|---|---|---|
| S0 | Concerns C1-C7 + M15.2 vision-LLM lock + empirical CC native test | done | `2fe5034` |
| S1 | Screenshot capture infrastructure | done | `d82d630` |
| S2 | Vision-LLM integration | done | `ae2e453` (+ CI fixes `f5db33f`, `1b7db01`, `671c865`) |
| S3 | Baseline coverage (12 production-fidelity of 38) | done | Rounds 1–6: `e94a656`, `03d929d`, `8266e4c`, `f797918`, `9e7ec30`, `f9be28d`; fidelity audit + Round 6 fixes: `8d3d0e4`; R7 reclassification: `2fa842c`; accept-baseline path fix: `692477b`; operator's 12-baseline acceptance: `1f4524b` |
| S4 | Test framework integration | done | `6da6c61` |
| S5 | CI integration (artifact upload + accept-baseline.sh) | done | `b55203e` |
| S6 | CC vision-driven self-test demonstration | done | `1e8a20b` (local-only at M15 close; queued for pre-PR push once S3 baselines are CI-canonical) |
| S7 | This M15-done.md + V0.3 hand-off | in progress | (this commit) |

---

## 3. Visual baseline coverage map

Per `M15-concerns.md` C3, V0.2 ships **Y-scope** state-machine-
complete coverage. 38 states catalogued; **12 production-
fidelity baselines committed** at `1f4524b`:

| # | State | Mechanism | Status |
|---|---|---|---|
| 00 | empty-launch | C window grab | PASS |
| 02 | conn-udp-idle | C; `autoLoadFixtureNoConnect` | PASS |
| 04 | conn-udp-connected | C; m14_smoke fixture | PASS |
| 12 | multi-2-drivers | C; m15_multi_2.yaml fixture | PASS |
| 13 | multi-5-drivers | C; m15_multi_5.yaml fixture | PASS-FLAKY (7.5 % tolerance) |
| 24 | dialog-add-serial | C-fullscreen; `autoShowAddConnectionDialog("serial")` | PASS |
| 25 | dialog-add-udp | C-fullscreen; `autoShowAddConnectionDialog("udp")` | PASS |
| 26 | dialog-edit | C-fullscreen; `autoShowEditConnectionDialog` | PASS |
| 30 | menu-file-open | C-fullscreen; `autoOpenMenu("File")` | PASS |
| 31 | menu-connections-open | C-fullscreen; `autoOpenMenu("Connections")` | PASS |
| 32 | menu-session-open | C-fullscreen; `autoOpenMenu("Session")` | PASS |
| 33 | status-buffer-normal | C; `--auto-select-signal udp:m14-smoke-udp/temperature` | PASS |

V0.3 hand-off backlog (26 states): see §5 below.

---

## 4. V1 UX gap inventory (V0.3 design input)

Surfaced during S3 R7 fidelity audit, S6 demo, and S7 R9
cross-environment measurement-coupling investigation. 11
entries catalogued (1–10 already in `M15-progress.md` §S3;
#11 added at S7):

1. **`buffer 0%%` / `seek N %%` double-percent encoding** — trivial fix (`%1%%` → `%1 %`).
2. **Buffer pressure not color-coded** — V0.3: green / yellow / red thresholds + visual indicator.
3. **Play button does not toggle on `PlaybackState`** — V0.3: toggle label/icon Loaded → Paused → Playing → Ended.
4. **`Auto-connect on startup` combo "currently has no effect"** but still editable — V0.3: remove or honour.
5. **Replay seek shows `0 / 0 records`** for catalog-only sessions — V0.3: hide records suffix when count is 0.
6. **Replay seek slider lags on programmatic `seek()`** — `PlaybackController::seek()` doesn't emit `positionChanged`. V0.3: emit on success.
7. **Default Driver type = Serial** regardless of UDP-heavy embedded workflows — V0.3: most-recently-used or detection-based.
8. **`Auto-connect commands` UI is busy-by-default** — V0.3: collapse behind "Advanced" disclosure.
9. **Recording-active status lacks live progress without traffic** — V0.3: heartbeat indicator + elapsed-time independent of byte count.
10. **Production replay seek emits no UI feedback** — V0.3: status label + slider both reflect seek target.
11. **Visual identity not owned by application** — SignalForge V1 defers all Qt styling to the OS cascade (Qt theme + fonts + display config + desktop session state). Same SignalForge binary on operator dev (Ubuntu + Yaru theme + Cantarell fonts inherited from the desktop session) vs CI runner (xvfb + Fusion fallback + base fonts, no desktop session) produces 14–33 % pixel diff with semantically identical content. This makes pixel-diff regression detection unreliable across environments and means SignalForge has no consistent visual identity — compare LabVIEW / MATLAB / Tektronix / Saleae / NI / Yokogawa, which all own their look cross-platform regardless of host OS theme.

    **V0.3 required deliverable**: take ownership of visual identity via `QT_STYLE_OVERRIDE` + custom `QPalette` + bundled fonts (`QFontDatabase::addApplicationFont`) + stylesheets (QSS or Qt-themes). Same binary, any OS, identical render. **First M16 priority** — without it, all subsequent V0.3 redesign work has environment-dependent fragility and any pixel-diff regression check is meaningless outside an exactly-reproduced CI environment.

---

## 5. V0.3 hand-off backlog (26 of 38 deferred from S3)

Per R7 production-fidelity classification:

| Class | Count | States | V0.3 work |
|---|---:|---|---|
| (b) V1 GUI gap | 1 | 18-replay-playing | Play button toggle on PlaybackState (UX gap #3) |
| (c) capture-mechanism | 1 | 05-conn-udp-with-signal | Hardware-RHI baseline pass (ADR-010 limitation; software-RHI cannot rasterize 1-px line strips) |
| (d) fixture-mock | 6 | 14, 15, 17, 19, 20, 21 | Either (i) production code emits the state naturally (UX gaps #6, #9, #10) or (ii) xdotool / Qt-QTest event injection drives real-user paths |
| Multi-chart segfault | 3 | 01, 36, 37 | `rebuildChartWidgets()` redesign (append-only OR QML repeater) per `HALT-20260510T172100Z` Option A |
| Operator-manual | 15 | 03, 06, 07 (transient ConnectionState); 08, 09, 10, 11 (hardware Serial+TCP); 16, 22, 23, 27, 28, 29 (specialised modal flows); 34, 35 (extreme buffer-pressure) | Mix of hardware fixtures, fault-injection CLI flags, real-X11 dogfood; classify each in V0.3 bootstrap |

---

## 6. Industrial software references (opportunistic collection)

V0.3 design pass benchmarks against established industrial-
software UIs. References collected during M15:

- **LabVIEW** (NI) — graphical block-diagram + chart-heavy UX; reference for connection-list panel density.
- **MATLAB Instrument Control Toolbox** — fixture-edit-dialog density + driver-type combo; reference for `Connection-add` dialog.
- **Tektronix Oscilloscope UIs** (TBS / MSO series) — chart-pane toolbar density; reference for `+ Chart` / `Live` / time-preset arrangement.
- **Saleae Logic** — modern data-acquisition UX; reference for signal-selector tree + chart-pane interaction.
- **NI VeriStand** — real-time multi-channel monitoring; reference for buffer-pressure visualisation + multi-driver layout.
- **Yokogawa IS-Series** — DAQ + chart workbench; reference for status-bar layout + record/replay toolbar grouping.

These are collected references, not endorsements. V0.3 picks
the design-token + widget pattern that best fits the
SignalForge embedded-bring-up workflow.

---

## 7. V0.3 spec-writing scope (M16+)

Per `docs/V0-series-charter.md` §2.3, V0.3 = industrial UI/UX
rebuild. Provisional M16+ scope:

- **M16 — Design tokens + Visual identity ownership**

  First V0.3 milestone. Takes SignalForge from "OS-styled" to "self-styled". Without it, M17–M20+ baseline regression is environment-coupled (per UX gap #11 / R9 lesson at V0.2 close).

  Deliverables:

  1. **Qt rendering pipeline ownership** — `QT_STYLE_OVERRIDE=Fusion` (or a custom `QStyle` subclass); `QApplication::setPalette(...)` driven by the M16 color tokens; evaluate QSS vs Qt-themes for stylesheet authoring.
  2. **Font management** — bundle a fixed set of fonts via CMake installer payload + `QFontDatabase::addApplicationFont` at startup, so the same SignalForge binary on any OS picks the same fonts.
  3. **Color tokens** — light + dark palettes targeting industrial aesthetics (high-contrast, low-saturation, accessible for instrument-panel viewing contexts).
  4. **Typography tokens** — sans-serif system + monospace system + sizes / weights / line-heights.
  5. **Spacing + layout tokens** — 4 px / 8 px / 16 px grid; consistent panel padding, control padding, dialog margins.
  6. **Cross-environment baseline verification** — the same binary must produce identical render across dev (operator's Ubuntu + Yaru) + CI (Ubuntu-24.04 xvfb + Fusion fallback) + arbitrary user-install OS variations. M16 close gate: re-capture all V0.2 baselines under M16 tokens; pixel-diff < 1 % across all three environments.

  Industrial software references (LabVIEW / MATLAB Instrument Control / Tektronix / Saleae Logic / NI VeriStand / Yokogawa IS-Series) all do this; V0.3 closes the gap. Without M16, V0.3 design pass cannot use pixel-diff as a regression gate, and SignalForge cannot be evaluated as "industrial-software-grade visual quality" in any objective sense.
- **M17 — Core widget rebuild**: rebuild ChartManager + Chart QQuickWidget hosting to fix `rebuildChartWidgets()` segfault (M15 §S3 §HALT) AND lift Play-button-toggle gap (UX gap #3); make `PlaybackController::seek()` emit `positionChanged` (UX gap #6); production seek UI feedback (UX gap #10).
- **M18 — Workflow rebuild**: connection-add / connection-edit dialog redesign (UX gaps #4, #7, #8); status-bar redesign (UX gaps #1, #2, #5, #9); replay-toolbar overhaul (UX gaps #3, #6); buffer-pressure visualisation (#2).
- **M19+ — Hardware fixture suite**: Serial (socat virtual pty), TCP server fixture, UDP traffic flood fixture, replay session file generators; closes 8+ operator-manual S3 states.
- **M20+ — Operator interactive states**: xdotool / Qt-QTest event injection for the 3 specialised-modal-flow operator-manual states (mode transitions, replay file picker).

V0.3 will additionally use the M15 perception loop (S6 pattern)
as the iteration cadence: each design change → CC visual-diff
+ vision-LLM describe → operator dogfood only on semantic
deltas, not pixel-level.

---

## 8. Known issues at M15 close

### 8.1 Cross-environment baseline-rendering drift (operator-blocking for CI green)

CI run `25647474336` (operator's `1f4524b` baseline-acceptance
push) and `25647961120` (my `6da6c61` S4 push) both failed
visual tests with 14–33 % pixel diff. Root cause: the local-
xvfb environment that produced the operator-accepted captures
renders Qt widgets with a stripped-down style (mostly white,
no inter-component fills) while CI's Ubuntu-24.04 xvfb renders
the canonical Qt-default style (with subtle background fills).

Per operator's S3 R8 diagnostic: "两组截图体现的功能都是没有问题的，
pixel diff 来源于ui显示，ci的ui显示是正常qt风格" — both captures
show the same semantic state; CI is the more-canonical Qt
rendering.

CI captures from S4 run `25647961120` are staged at
`tests/screenshots/baseline-candidate/` (gitignored) for the
operator's per-state review via:

```bash
for state in 00-empty-launch 02-conn-udp-idle 04-conn-udp-connected \
             12-multi-2-drivers 13-multi-5-drivers \
             24-dialog-add-serial 25-dialog-add-udp 26-dialog-edit \
             30-menu-file-open 31-menu-connections-open \
             32-menu-session-open 33-status-buffer-normal; do
    # operator reviews each via Read tool / image viewer …
    scripts/accept-baseline.sh "$state"
done

git status                         # 12 modified baselines
git commit -m "fix: M15 S3 — re-baseline against CI xvfb Qt rendering

Cross-environment style drift surfaced on CI runs 25647474336
(operator) and 25647961120 (S4). Local-xvfb's stripped-down
rendering accepted at 1f4524b; CI's canonical Qt-default
rendering is now the committed reference. Operator confirmed
semantic equivalence in M15 S3 R8 diagnostic."
git push origin milestone/M15
```

After this remediation lands + S6 demo commit (`1e8a20b`
local-only) pushes on top, CI should go green.

**Long-term fix (V0.3)**: M16 design-tokens pass pins Qt style
+ fontconfig in both local and CI to eliminate the drift entirely.

### 8.2 Multi-chart segfault (catalogued V0.3)

`HALT-20260510T172100Z`. Operator decision A: defer to V0.3
chart-host redesign. `autoAddCharts` primitive + `--auto-add-charts`
CLI flag retained as V0.3-ready infrastructure.

### 8.3 Software-RHI 1-px line-strip rasterization (catalogued V0.3)

ADR-010 §"Implementation lesson". Affects state 05 baseline +
all chart-line-bearing replay/recording captures. V0.3
hardware-RHI baseline pass closes this.

---

## 9. Frozen-surface counter (CLAUDE.md HALT trigger #5)

| Counter | Limit | Status |
|---|---|---|
| **0 / 2** | > 2 → HALT | clean |

No frozen-surface modifications during M15. All work on
non-frozen surfaces (`main_window.{hpp,cpp}`, `main.cpp`,
test infrastructure, new fixtures, documentation).

---

## 10. CC + operator collaboration retrospective

M15 surfaced three governance lessons worth carrying into V0.3:

1. **Plan ordering is source of truth** (operator R5
   correction; memorialised in `feedback_plan_ordering.md`).
   Post-compaction CC re-read M15-plan.md before picking next
   subtask. S5 jumped ahead of S3 once; corrected.
2. **Production-fidelity > pixel-stability** (operator R7
   reclassification). 6 captures (14, 15, 17, 19, 20, 21)
   that originally passed S3 pixel-diff were reclassified as
   (d) fixture-mock when the operator surfaced the fixture-
   state-vs-production-state distinction. V0.3 inherits this
   as the canonical acceptance criterion.
3. **Pixel-diff alone is insufficient** (S6 demo proved end-
   to-end). 4-letter text swap at the 5 % gate registers 0.019 %.
   Vision-LLM verdict closes the semantic gap; pixel-diff
   regression-protects layout / theme / chrome.

Per-baseline review authority sits with the operator (R8
correction — bulk-overwrite-without-review was rejected even
when CI semantically matched local). V0.3 inherits this:
baseline acceptance remains explicit, per-state, operator-
driven; CC stages candidates, operator accepts.

**R9 — Cross-environment measurement coupling (V0.2 close).**
Pixel-diff vs CI failed for the 12 operator-approved baselines:
14–33 % diff with semantically identical content. Investigation
revealed SignalForge has no owned visual identity — Qt defaults
inherited from the OS desktop session (operator dev: Ubuntu +
Yaru theme + Cantarell fonts; CI: xvfb + Fusion fallback +
base fonts).

Path A (re-baseline against CI captures) accepted for V0.2
close because the V0.2 charter §1 promise (vision
infrastructure) is delivered; visual identity is out of V0.2
scope and rolls into V0.3 M16 (UX gap #11).

Implication: V0.2 baselines are CI-environment-specific
snapshots. Will fail when operator re-runs visual tests
locally — same binary, different rendering. Operator must
either accept noise on local runs (CI is authoritative gate)
or pin local environment to match CI manually until M16
closes the gap.

V0.3 M16 must close this gap. Otherwise pixel-diff is
meaningless for V0.3+ regression detection in any setting
other than CI exact reproduction. **V0.2's vision-loop
demo (S6) still works cross-environment** because the
loop is CC-Read-tool-based semantic comparison, not
pixel-level; the lesson R9 carries forward is specifically
about pixel-diff's cross-environment limit, not about the
V0.2 perception loop itself.

---

## 11. PR + merge state

- PR number: (to be filed at S7 close — open PR
  `milestone/M15` → `main` once CI is green per §8.1 remediation)
- Merge SHA: TBD (Phase 3 after operator approval)
- Tag plan: `v0.2.0` (internal milestone; no GitHub Release
  publish per charter §5)
- Bootstrap successor: `milestone/M16` (V0.3 — see §7)

---

## 12. Cross-references

- M15 spec: `docs/milestones/M15-vision-infrastructure.md`
- V0 charter: `docs/V0-series-charter.md`
- M14 done (predecessor): `.claude/M14-done.md`
- M15 progress: `.claude/M15-progress.md`
- M15 concerns: `.claude/M15-concerns.md`
- CC autonomy demo: `docs/m15-cc-autonomy-demo.md`
- HALT log: `.claude/halt/HALT-20260510T172100Z-m15-s3-rebuildcharts-segfault.md`
- Visual test suite README: `tests/visual/README.md`
- Accept-baseline workflow: `scripts/accept-baseline.sh`

---

## 13. Hand-off to next session

When the operator returns to merge M15:

1. **Resolve §8.1 baseline drift**: review the 12 CI captures
   in `tests/screenshots/baseline-candidate/` (already staged
   by CC); run the bash loop from §8.1 to promote + commit;
   push.
2. **Push S6**: `git push origin milestone/M15` to send the
   queued `1e8a20b` commit (S6 demo doc).
3. **Watch CI**: both pushes should produce green CI runs.
4. **Open PR**: `gh pr create --base main --head milestone/M15`
   with this M15-done.md as the body (or a summary thereof).
5. **Phase 2 approval**: operator reviews PR + says
   "approved, merge M15 and begin M16 bootstrap" (per
   CLAUDE.md authorization phrase).
6. **Phase 3 (CC autonomous)**: merge PR, tag `v0.2.0`, push
   tag, bootstrap `milestone/M16`.

---

## 14. V1.0 ship-gate framing (cross-environment visual determinism)

The V0.2 → V0.3 transition surfaces a structural V1.0 ship-
gate requirement not previously documented in `docs/V0-series-charter.md`:

**SignalForge V1.0 ship requires deterministic Qt rendering
across supported OS / Qt version / display config.** The
operator's dev machine, CI runners, and arbitrary end-user
installs must produce identical pixel output for the same
application state. Without this:

- pixel-diff regression detection is meaningless beyond
  exact-CI-reproduction (R9 lesson);
- bug reports referencing visual state cannot be reproduced
  across environments (operator sees X, user sees Y, CC sees Z
  on a third);
- documentation screenshots become environment-specific
  (the M15 baselines are already split across two distinct
  renderings);
- SignalForge cannot claim "production-ready visual quality"
  in the industrial software sense — LabVIEW / MATLAB /
  Tektronix / Saleae / NI / Yokogawa all guarantee
  cross-platform pixel-determinism.

V0.3 M16 (UX gap #11) is the path to closing this gap.
After M16: same SignalForge binary, any supported OS, any
desktop session state → identical pixel output for the same
application state. That deterministic-rendering invariant
becomes a V1.0 ship-gate prerequisite alongside the existing
charter §1 functional gates.

Documentation hand-off to V0 charter authors: when the
operator updates `docs/V0-series-charter.md` between V0.2
close and V0.3 bootstrap, the V1.0 readiness section should
note this prerequisite explicitly (or §14 here is the
authoritative reference until then).
