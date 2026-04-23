# M1 Completion Report

## Timing

- Started (session opening per `[EM §9]`): 2026-04-23 (understanding
  + plan written as `47924d3`, executed after approval).
- Completed: 2026-04-23 (this file committed).
- Active CC wall-clock time: approximately 5 hours including
  preflight, skeleton, five checks, one real-display Check 5 run on
  DISPLAY=:0, five runs of Check 4 to characterize variance, two CI
  pushes (one to fix the artifact-upload glob), the spike report,
  and this completion report. CI wait time ≈ 12 minutes across the
  two M1-spike workflow runs.

## Verdict matrix (copied inline per `[M1 §7.3]`)

| Check | Local | CI | Final verdict |
|---|---|---|---|
| 1. Floating / re-docking | Pass (5 cycles, screenshot, 0 warnings) | Pass | **Pass** (high confidence) |
| 2. HiDPI 125/150/175/200 % | Partial (objective machinery Pass; xvfb-rendered screenshots; real-compositor crispness unverified) | N/A (no real display) | **Partial — human visual verification owed** |
| 3. Context-menu propagation | Pass (3 docks × menu trigger × clear) | Pass | **Pass** (high confidence) |
| 4. Hide / show lifecycle | Mixed (VmRSS growth 7–17 MB across 5 runs vs spec < 10 MB; FD stable; valgrind clean for spike code) | Pass (short variant) | **Partial / nuanced** |
| 5. Multi-instance GPU | Nuanced (real-display HW OpenGL: GPU% 47.8% Pass; CPU% 48.7% for 3 widgets Fail vs < 30% single-core; per-process VRAM unattributable) | N/A (no GPU) | **Nuanced** |

Full report: `docs/spikes/M1-qtquick-integration.md`.

## Deliverables checklist — `[M1 §2.1]` + `[§7.1]`

| Spec item | Status | Notes |
|---|---|---|
| 1. Standalone spike at `tools/spike/qquick_dock_test/` NOT in top-level CMakeLists | ✅ | Isolated build; top-level CMakeLists untouched |
| 2. Three `QQuickWidget`s in three `QDockWidget`s | ✅ | Left / Right / Bottom dock areas |
| 3. QML scene with animated Canvas (30 Hz) | ✅ | Sine-wave Canvas + state label + right-click MouseArea |
| 4. Five integration checks | ✅ | Implemented as `--auto-check 1..5` |
| 5. Report at `docs/spikes/M1-qtquick-integration.md` | ✅ | Includes verdict matrix, env, per-check sections, human-decision data |
| 6. Screenshots + data under `docs/spikes/M1-artifacts/` | ✅ | Check 1 end-state + Check 2 × 4 scales + Check 3 menu + Check 4 CSV + rss-runs + valgrind + Check 5 trace + summary + QSG info + env snapshot |
| 7. `.claude/M1-partial-results.md` | N/A | No check in hard-Partial ("couldn't run") state. S4 and S5 nuances are documented in the main report; S6 nuance likewise |
| Build cleanly with zero warnings | ✅ | Every commit built cleanly; verified via cmake --build each round |
| Spike runs without immediate crash | ✅ | Both xvfb and DISPLAY=:0 runs initialize, run, and exit cleanly |
| CI workflow with one successful run | ✅ | `.github/workflows/m1-spike.yml` — CI run 24810079353 green |

Acceptance-criteria items from `[M1 §7]`:
- §7.1 artifacts: ✅ all present.
- §7.2 report quality: ✅ matrix complete, environment documented,
  per-check sections complete, no recommendation, ambiguous items
  flagged for human.
- §7.3 process: ✅ commits conform to `<module>: <verb> <object>`;
  two subtasks classified as Partial/Mixed/Nuanced — inside the
  soft-HALT budget that update #5 collapsed to 0 expected but
  report-immediately-if-any (both surfaced in chat at their
  moments).
- §7.4 freezes established: **None**. M1 is a spike; no interface or
  schema is committed or frozen. Recorded here per the template.

## Acceptance self-check — `[EM §5]`

### §5.1 Code
- [x] All promised modules / files exist
- [x] Build clean, zero warnings (spike's own CMake invocation)
- N/A — Spike has no unit tests by design (§2.2 item 1: no
  production chart code; §2.3 optional items do not include unit
  tests)
- [x] ASan cleanliness: not applicable to the spike binary (M1 does
  not set up a debug-asan preset for it; M0's debug-asan gate covers
  the production tree and remains green)
- [x] clang-format passes on spike sources (src tree unchanged so
  its format discipline is unaffected)
- [x] No stray `TODO`s added
- [x] No commented-out code
- [x] No `std::cout` / `printf`; `qDebug` is used **only** inside
  the spike per the `[M1 §9]` exception, documented in the spike's
  `README.md`

### §5.2 Tests
- N/A — Spike is evidence-gathering, not a tested module

### §5.3 Documentation
- [x] Spike's `README.md` explains build / run / logging exception
- [x] `docs/spikes/M1-qtquick-integration.md` is the milestone
  deliverable

### §5.4 Process
- [x] One meaningful commit per subtask plus a fix-up commit for
  the CI artifact upload; no `WIP` residue
- [x] Commit messages follow `<module>: <verb> <object>`
- [x] No merge conflicts
- [x] No HALT reports — zero HALTs fired this milestone

### §5.5 Spec conformance — see `[M1 §7]` section above

## Test results

Unit: N/A (none required for a spike).

Integration: the spike's own `--auto-check` sequence is the
"integration" surface.

| Check | Local | CI |
|---|---|---|
| 1 | Pass | Pass |
| 2 | Partial (objective) | N/A |
| 3 | Pass | Pass |
| 4 short | Pass | Pass |
| 4 full | Partial / nuanced | N/A |
| 5 | Nuanced (HW run) | N/A |

CI run (canonical green): https://github.com/mornthx/signalforge/actions/runs/24810079353

## HALTs raised during this milestone

**None.** All checks ran to completion. Three findings required
immediate in-chat escalation per approval update #5 but none
triggered a formal HALT:

1. S4 first-attempt scrot captured root window instead of spike →
   remediated with xvfb fallback (spec-permitted); real-compositor
   crispness flagged for human.
2. S6 VmRSS growth sits right at spec threshold → characterized
   across 5 runs; report describes the nuance.
3. S7 xvfb GPU numbers were not representative → user-directed
   re-run on DISPLAY=:0 with `QSG_INFO=1`; primary evidence is the
   real-display run.

## Deviations and concerns

(Nothing ended up in a separate `.claude/M1-concerns.md` — the
concerns are embedded in the spike report's "Data for the human's
decision" section.)

**Two open questions the report explicitly leaves for the human**:

- **Check 2 real-compositor crispness** at 125 / 150 / 175 /
  200 %: CC's captures are from Xvfb (software renderer). Human
  opens the spike on the physical monitor at each scale and judges.
- **Check 5 per-process VRAM attribution**: free-tier AMD telemetry
  cannot isolate per-process VRAM on a shared iGPU. The ~77 MB
  incremental during the spike is the best available attribution.

**One data point that warrants M6 early-benchmarking attention**:

- Check 5 CPU% was 48.7% (3 widgets) on hardware-accelerated
  OpenGL. The spec says "< 30% single-core". Per-widget ≈ 16%.
  M6's chart pipeline is architecturally different (Scene Graph
  custom node + downsampling), so linear extrapolation to 20
  charts is not reliable — but the 3-widget result is a signal
  the rendering approach decision can weigh.

## Freezes established in this milestone

**None.** M1 is a spike milestone; no interface or schema is
committed or frozen. Recorded here per the `[M1 §7.4]` requirement
to explicitly state absence of freezes.

## Open issues carried forward

1. Human visual verification of Check 2 HiDPI crispness on the
   physical display at the four scale factors.
2. Human interpretation of Check 4's "9–17 MB over 20 cycles"
   against the spec's "< 10 MB (i.e., not unbounded)" language.
3. Human decision on the rendering approach per `[M1 §8]`:
   QQuickWidget / QWindow container / QPainter+OpenGL.
4. `glxinfo` is not installed on the dev host (`mesa-utils` absent);
   `QSG_INFO=1` was used as a fallback for RHI backend confirmation.
   Not blocking; mention for completeness.

## Commits on `milestone/M1`

Below the branch-point commit `afe87aa docs: add M1 Qt Quick
integration spike spec` on `main`:

| Hash | Subject |
|---|---|
| `47924d3` | chore: record M1 understanding and plan |
| `c815125` | spike: M1 preflight findings |
| `c1c79d4` | spike: scaffold qquick_dock_test with three docked QQuickWidgets |
| `e140acf` | spike: add check 1 — floating/re-docking cycles |
| `1fce8b0` | spike: add check 2 — HiDPI at 125/150/175/200% |
| `ba9b27e` | spike: add check 3 — context menu across widgets/quick boundary |
| `3c8c35f` | spike: add check 4 — hide/show lifecycle with VmRSS+FD sampling |
| `0c32be1` | spike: add check 5 — multi-instance GPU sampling |
| `9e76ae8` | ci: add M1 spike headless verification workflow |
| `e46115c` | ci: fix M1 spike artifact upload glob |
| `ed0e03b` | docs: M1 Qt Quick integration spike report |
| *(this)*  | docs: file M1 completion report |

That's 12 net commits on milestone/M1.

## Hand-off checklist for the human — `[M1 §8]`

1. **Read** `docs/spikes/M1-qtquick-integration.md` in full.
2. **Eyeball** Check 2 on the physical monitor at each of the four
   scale factors:
   ```
   QT_SCALE_FACTOR=1.25 QT_AUTO_SCREEN_SCALE_FACTOR=0 \
       ./tools/spike/qquick_dock_test/build/qquick_dock_test
   ```
   (repeat for 1.50, 1.75, 2.00). Judge crispness of text and the
   Canvas sine-wave.
3. **Decide** the rendering approach: Go (QQuickWidget) /
   Downgrade (QWindow container) / Aggressive downgrade (QPainter +
   OpenGL). The decision material is in the report's "Data for the
   human's decision" section.
4. **Record** the decision per `[M1 §8.4]`: either amend
   `docs/architecture/architecture.md §8.5`, or file a new ADR
   under `docs/architecture/decisions/`. A one-line note if Go; a
   reasoned explanation if a downgrade.
5. **Merge** `milestone/M1` → `main` (the override OV-1 means no
   protection gate; do this deliberately after acceptance), and
   tag `v0.0.2-alpha.1`.

## Suggestions for M2

None substantive. M2 is the Platform + Core Abstractions milestone
per `[MR]`; nothing from M1's spike blocks its start. The only
soft carryover: if the human's M1 decision is "downgrade to QWindow
container", M6's estimate needs updating and M1 §8.4's architecture
amendment is a prereq for M2 / M6 convergence.
