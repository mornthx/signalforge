# M1 — Execution Plan

## Conventions

- Subtasks are numbered to match `docs/milestones/M1-qtquick-integration-spike.md` §4 (S1–S9).
- Each entry is a **subtask**, not an edit. A subtask may touch
  many files; a subtask ends with one commit.
- "Prereq" refers to other plan subtasks. Environmental preflight
  findings live in S1.
- Effort estimates are wall-clock CC time, not human review time.
- **Commit discipline**: this milestone's output is largely evidence
  files (PNGs, CSVs, logs). They go in `docs/spikes/M1-artifacts/`
  and commit with their check's subtask. Source-code commits still
  follow `[CM §Required-2]` build+test gating; docs-only commits
  use the §Required-2 exception for non-code files.

## Preflight before subtask 1

Already performed as a light probe for the understanding doc (not
formal S1). Key facts carried forward:

- Session: X11, `DISPLAY=:0`, AMD Cezanne iGPU.
- Missing at session start: `valgrind`, `radeontop`, `scrot`,
  `grim`, `import`, `xdotool`, `glxinfo`. Present: `xvfb-run`, Qt
  6.10.2 at `~/Qt/6.10.2/gcc_64/`.
- AppProtection preload active (M0 C2).

S1 below codifies these and requests one apt install batch up-front.

## Subtasks

### S1 — Environment preflight (Blocking)

**Action**:

1. Probe `QT_QPA_PLATFORM` paths (document that `xcb` is used; `wayland` is not applicable on this X11 session).
2. Confirm Qt 6.10.2 at `$SIGNALFORGE_QT_PATH` or default. Already confirmed present.
3. Verify `scrot`, `radeontop`, `valgrind` are all present via `which`. HALT if any missing — pre-probe expected them installed per approval update #1.
4. Document `QT_SCALE_FACTOR` behavior from Qt docs (no test run yet).
5. Sanity-check `/sys/class/drm/card*/device/mem_info_vram_used` is readable (for Check 5 VRAM sampling per approval update #3).
6. Note AppProtection preload is still active — flagged for S6 valgrind attempt.
7. Write findings to `.claude/M1-progress.md` under "S1 preflight".

**Files written**:
- `.claude/M1-progress.md` (new)

**Prereq**: understanding+plan approval.
**Classification**: Blocking.
**Effort**: ~15 min (mostly waiting on the human for apt decision).
**Commit point**: 🟢 yes — docs-only. Message: `spike: M1 preflight findings`.

### S2 — Spike program skeleton (Blocking)

**Action**: create the five files under `tools/spike/qquick_dock_test/`.

**Files written**:
- `tools/spike/qquick_dock_test/CMakeLists.txt` — standalone; `cmake_minimum_required(VERSION 3.22)`; `project(qquick_dock_test CXX)`; `find_package(Qt6 6.10 REQUIRED COMPONENTS Core Widgets Quick QuickWidgets)`; one `qquick_dock_test` executable; no link against `src/*`.
- `tools/spike/qquick_dock_test/main.cpp` — `QApplication` + command-line flag parsing (`--auto-check N` optional, also `--short`). When no flag, interactive mode.
- `tools/spike/qquick_dock_test/main_window.hpp` — `class MainWindow : public QMainWindow` in `namespace signalforge::spike`. Three `QDockWidget*` members, each with a `QQuickWidget*`.
- `tools/spike/qquick_dock_test/main_window.cpp` — constructs docks/widgets, loads `qrc:/qml/DockContent.qml`, wires menu bar ("Hide Dock 1" / "Show Dock 1" / "Quit"), wires the auto-check dispatch. Defines a helper to capture screenshots via `QWidget::grab()` at a given path.
- `tools/spike/qquick_dock_test/qml/DockContent.qml` — root `Item`, colored `Rectangle`, `Canvas` animated by a 33 ms `Timer` (sine wave), label showing dock id and state, `MouseArea` emitting `contextMenuRequested(point)` on right-click.
- `tools/spike/qquick_dock_test/resources.qrc` (if needed for QML; alternative: `Q_INIT_RESOURCE` or direct filesystem path).
- `tools/spike/qquick_dock_test/README.md` — how to build & run; explicit note: *"This spike uses `qDebug` for logging as an exception to CLAUDE.md §Forbidden-6, per M1 spec §S2 and §9. Production code under `src/` uses `SF_LOG_*`."*

**Build verification**:
```
cd tools/spike/qquick_dock_test
cmake -B build -S . -G Ninja \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.2/gcc_64 \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
Zero warnings. Then:
```
xvfb-run --auto-servernum ./build/qquick_dock_test &
sleep 3; kill %1
```
Must not immediately crash. Log lines captured to confirm QML loaded and render context was created.

**Prereq**: S1.
**Classification**: Blocking.
**Effort**: ~60 min (plus first-time CMake Qt-find round-trip).
**Commit point**: 🟢 yes. Message: `spike: scaffold qquick_dock_test with three docked QQuickWidgets`.

### S3 — Check 1: Floating / re-docking (Independent)

**Action**: add `--auto-check 1` handling. Sequence:
1. Load all three docks.
2. For cycle i in 1..5: `dock->setFloating(true)`, `QTest::qWait(1000)`, programmatically move the floating window 200 px, `QTest::qWait(500)`, `dock->setFloating(false)`, `QTest::qWait(1000)`.
3. Capture end-state screenshot of the main window via `QWidget::grab()` → PNG.
4. Parse stderr for `QQuickWidget` / `QSG` warnings; exit 0 on clean run, non-zero on any warning.

**Files written**:
- Edits to `main.cpp` / `main_window.cpp` (auto-check 1 branch).
- `docs/spikes/M1-artifacts/check1-log.txt` — stderr output.
- `docs/spikes/M1-artifacts/check1-end-state.png` — screenshot.

**Local execution**: yes, under `xvfb-run` (auto-servernum). X11 session also available for sanity re-run.
**CI execution**: yes (S8 covers Check 1 headless).

**Prereq**: S2.
**Classification**: Independent.
**Effort**: ~30 min.
**Commit point**: 🟢 yes. Message: `spike: add check 1 — floating/re-docking cycles`.

### S4 — Check 2: HiDPI at 125/150/175/200% (Independent)

**Action**: add `--auto-check 2` handling (runs the current process at the current scale, settles for 2 s, then spawns `scrot <outfile>.png` to capture the real compositor output — per approval update #2, `QWidget::grab()` is NOT used for this check because it walks a software path that cannot evidence real-compositor HiDPI rendering). Driver logic is a shell loop that re-launches the spike four times with `QT_SCALE_FACTOR=<v>` set, captures a screenshot per scale via scrot, and logs startup stderr per scale.

**Files written**:
- `docs/spikes/M1-artifacts/check2-scale-1.25.png`
- `docs/spikes/M1-artifacts/check2-scale-1.50.png`
- `docs/spikes/M1-artifacts/check2-scale-1.75.png`
- `docs/spikes/M1-artifacts/check2-scale-2.00.png`
- `docs/spikes/M1-artifacts/check2-log.txt` (startup stderr for each scale, concatenated with scale markers).
- Small helper shell snippet under `tools/spike/qquick_dock_test/run-check2.sh` (optional; may keep it inline in the report if the snippet is trivial).

**Local execution**: yes — requires real X11 display (`DISPLAY=:0`), not xvfb, because the whole point of Check 2 is real-compositor rendering fidelity. `scrot` now present (verified at S1).
**CI execution**: spec §S8 **skips** Check 2 in CI (no real display). This check's Final verdict will typically be "Pass (local on real display) + human visual verification pending" — CC records pass/fail of the machinery; human judges crispness.

**Prereq**: S2 (S3 not strictly required; can run independent of S3 changes).
**Classification**: Independent.
**Effort**: ~30 min (four launches + bookkeeping).
**Commit point**: 🟢 yes. Message: `spike: add check 2 — HiDPI at 125/150/175/200%`.

### S5 — Check 3: Context-menu propagation (Independent)

**Action**: add QML `MouseArea` emitting `contextMenuRequested(point position)` on right-click. C++ connects the signal to a slot that constructs a `QMenu` with "Action A" / "Action B" and calls `menu->exec(globalPos)`. `--auto-check 3` programmatically sends right-click to each of the three docks via `QTest::mouseClick(..., Qt::RightButton)`, polls for `QApplication::activePopupWidget()` within 500 ms, triggers "Action A" by signal, confirms close and action dispatch.

**Files written**:
- Edits to `main_window.cpp` (menu slot) and `DockContent.qml` (MouseArea + signal).
- `docs/spikes/M1-artifacts/check3-log.txt`.
- `docs/spikes/M1-artifacts/check3-menu-screenshot.png` — captured during one of the menu-open states via `QWidget::grab()` on the menu's top-level widget (if `activePopupWidget()` exposes it).

**Local execution**: yes under X11 and xvfb.
**CI execution**: yes (S8 covers Check 3 headless).

**Prereq**: S2.
**Classification**: Independent.
**Effort**: ~45 min (QML signal wiring + three-dock verification).
**Commit point**: 🟢 yes. Message: `spike: add check 3 — context menu across widgets/quick boundary`.

### S6 — Check 4: Hide/show lifecycle (Independent)

**Action**: add `--auto-check 4` (20 cycles) and `--auto-check 4 --short` (5 cycles). Each cycle: `dock1->hide(); qWait(500); dock1->show(); qWait(500)`. Between cycles, sample `/proc/self/status` VmRSS and `ls /proc/self/fd | wc -l`. Emit one CSV row per cycle.

Optionally (if S1 apt-install approved and valgrind + AppProtection allow it): run the same binary under `valgrind --leak-check=full --error-exitcode=2` on the `--short` variant. If AppProtection interferes (recursive malloc init, same as M0 C2 but for valgrind), record verbatim error to `check4-valgrind-blocked.txt` and skip the valgrind arm. Max one attempt to unblock (consistent with spec §3's "do NOT attempt more than one local workaround").

**Files written**:
- Edits to main.cpp / main_window.cpp (auto-check 4 branch + sampler).
- `docs/spikes/M1-artifacts/check4-memory-trace.csv` (20-cycle run, if successful).
- `docs/spikes/M1-artifacts/check4-valgrind.txt` OR
  `docs/spikes/M1-artifacts/check4-valgrind-blocked.txt`.

**Local execution**: yes for the VmRSS/FD arm; valgrind likely Partial on this host.
**CI execution**: yes for the `--short` VmRSS arm (§S8). Valgrind explicitly not attempted in CI ("CI doesn't have the minutes budget").

**Partial-results handling**: if valgrind blocked locally AND CI didn't cover it either, report Check 4 local verdict as "Partial" with reason "valgrind blocked by AppProtection preload, non-valgrind sampling shows <result>", and Final verdict as either "Pass (CI partial + local partial non-valgrind)" or "Partial — valgrind verdict absent, human may re-run on a non-AppProtection host if strict leak evidence is required."

**Prereq**: S2.
**Classification**: Independent.
**Effort**: ~45 min.
**Commit point**: 🟢 yes. Message: `spike: add check 4 — hide/show lifecycle with VmRSS+FD sampling`.

### S7 — Check 5: Multi-instance GPU (Independent)

**Action**: add `--auto-check 5` with 30 s sampling at 500 ms cadence. Starts all three docks rendering (their `Timer`-driven `Canvas` is always on), then samples:
- CPU % via reading `/proc/self/stat` deltas (no `top` dependency — avoids adding a tool).
- Spike process memory via `/proc/self/status` VmRSS and VmSize.
- GPU%: spawn `radeontop -d - -l 60` as a child process and parse its plain-text output (no `--json` — radeontop has no JSON mode; per approval update #3, direct text parsing only).
- VRAM: read `/sys/class/drm/card*/device/mem_info_vram_used` directly on each sample. Path verified readable at S1. This is independent of radeontop and survives if radeontop's text parsing hits a format surprise.

**Files written**:
- Edits to main.cpp / main_window.cpp (auto-check 5 branch, CPU sampler, optional `radeontop` subprocess, CSV writer).
- `docs/spikes/M1-artifacts/check5-gpu-trace.csv` — (time, cpu_percent, rss_mb, vsize_mb, gpu_percent or NULL, vram_mb or NULL).
- `docs/spikes/M1-artifacts/check5-summary.md` — min / max / mean of each column, with "N/A (no GPU telemetry)" if applicable.

**Local execution**: CPU/memory yes; GPU conditional on S1 apt-install.
**CI execution**: **skipped** per §S8 (no discrete GPU on runner). If local is also Partial, Check 5 Final verdict is "Partial — no GPU telemetry available in either environment. Human needs to run the spike on a host with a working GPU tool to close Check 5 before M6."

**Prereq**: S2.
**Classification**: Independent.
**Effort**: ~45 min.
**Commit point**: 🟢 yes. Message: `spike: add check 5 — multi-instance GPU sampling`.

### S8 — CI headless subset (Blocking)

**Action**: add `.github/workflows/m1-spike.yml` with the structure from §S8 of the spec. Single job matrix over the three check IDs (1, 3, 4-short). Runs on `push` to `milestone/M1` only. Installs Qt 6.10.2 via `jurplel/install-qt-action@v4` with cache. Builds the spike (isolated from top-level CMake). Runs `xvfb-run --auto-servernum ./build/qquick_dock_test --auto-check N`. Uploads `docs/spikes/M1-artifacts/check*-log.txt` as artifacts on every run.

**Files written**:
- `.github/workflows/m1-spike.yml`

**Syntax validation**: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/m1-spike.yml'))"`.

**Local execution**: N/A (it's a GitHub Actions workflow). The workflow's first real execution happens when S8's commit is pushed.
**CI execution**: yes — that's literally its purpose. Wait for the first run to go green before S9.

**Prereq**: S3, S5, S6 (needs all three headless-capable auto-checks to exist).
**Classification**: Blocking.
**Effort**: ~30 min + CI wait time.
**Commit point**: 🟢 yes. Message: `ci: add M1 spike headless verification workflow`. Push immediately; then watch the run.

### S9 — Report generation (Blocking)

**Action**: produce `docs/spikes/M1-qtquick-integration.md` matching the template in §S9 of the spec. Structure:
1. Executive verdict matrix (filled).
2. Environment (OS, kernel, Qt, display server, GPU+driver, CPU, AppProtection status, valgrind status).
3. Per-check sections (method, local result, CI result, artifacts links, issues, fallback impact from §S3–S7 copied verbatim).
4. Headless vs. local discrepancies (if any).
5. Blocked items (consolidated from `.claude/M1-partial-results.md`).
6. "Data for the human's decision" (facts only, no recommendation). When all checks Pass, this section additionally includes an **evidence-strength summary per check** (per approval update #4) — metadata, not recommendation. Example format: "Check 1 — Pass on local + CI, high confidence"; "Check 2 — Pass on local real display only, no CI alternative by design, medium-high confidence pending human visual crispness verification"; "Check 5 — Pass on local only, no CI alternative by design, medium confidence on a single AMD iGPU host". The summary classifies evidence breadth (two environments vs one) and remaining human-verification gaps, not a go/no-go.

**Files written**:
- `docs/spikes/M1-qtquick-integration.md` — the report.
- `.claude/M1-partial-results.md` if any Check hit Partial (may have been created earlier during S3–S7; this step consolidates).

**Prereq**: S8 green on CI (so the CI-column cells are real).
**Classification**: Blocking.
**Effort**: ~45 min.
**Commit point**: 🟢 yes. Message: `docs: M1 Qt Quick integration spike report`.

### S10 — Completion report (not in spec numbering; required per [EM §3.3])

After S9, per `[EM §3.3]` step 8: produce `.claude/M1-done.md` using the `[EM §6.2]` template plus the per-milestone customization from `[M1 spec §7]`. Include the verdict matrix inline as required by `[M1 spec §7.3]`. Explicit "Freezes established: none" line (per M1 spec §7.4).

**Files written**:
- `.claude/M1-done.md`
- Final update to `.claude/M1-progress.md`.

**Classification**: Blocking.
**Effort**: ~30 min.
**Commit point**: 🟢 yes. Message: `docs: file M1 completion report`.

### S11 — Hand-off to human (no commit)

Post a closing report in chat covering:
- Branch state (milestone/M1, N commits ahead of main).
- CI state (workflow run URLs for the M1 spike).
- Verdict matrix summary.
- Explicit list of items requiring human visual verification (Check 2 crispness, any Partials).
- Human actions per M1 spec §8: read report, eyeball the spike locally, make the rendering-approach decision, record it in arch §8.5 or an ADR, PR milestone/M1 → main, tag v0.0.2-alpha.1.

## Commit layout summary

Expected commits on `milestone/M1` after M1 closes (below the existing
`afe87aa docs: add M1 Qt Quick integration spike spec` on `main`):

1. `chore: record M1 understanding and plan` (plan approval — analogous to M0's `0a117e3`)
2. `spike: M1 preflight findings` (S1)
3. `spike: scaffold qquick_dock_test with three docked QQuickWidgets` (S2)
4. `spike: add check 1 — floating/re-docking cycles` (S3)
5. `spike: add check 2 — HiDPI at 125/150/175/200%` (S4)
6. `spike: add check 3 — context menu across widgets/quick boundary` (S5)
7. `spike: add check 4 — hide/show lifecycle with VmRSS+FD sampling` (S6)
8. `spike: add check 5 — multi-instance GPU sampling` (S7)
9. `ci: add M1 spike headless verification workflow` (S8)
10. `docs: M1 Qt Quick integration spike report` (S9)
11. `docs: file M1 completion report` (S10)

Total: 11 commits. If any check fix requires a follow-up, one extra
`fix: ...` commit may land before S9.

## Local vs. CI execution table

| Check | Local (this host) | CI (M1 workflow) | Evidence in report |
|---|---|---|---|
| 1. Floating / re-docking | ✅ X11 + xvfb | ✅ xvfb | Both |
| 2. HiDPI 125–200% | ✅ X11 screenshots via `QWidget::grab()` | ❌ skipped (no display) | Local + human visual verification note |
| 3. Context-menu propagation | ✅ X11 + xvfb | ✅ xvfb | Both |
| 4-full. Hide/show 20 cycles | ✅ VmRSS+FD. ⚠️ valgrind arm at-risk (AppProtection) | ❌ 20 cycles too long for CI | Local VmRSS+FD + either local valgrind or note |
| 4-short. Hide/show 5 cycles | ✅ | ✅ | CI primary, local confirms |
| 5. Multi-instance GPU | ⚠️ needs `radeontop` from S1 apt-install | ❌ skipped (no GPU) | Local only, or Partial if no install |

## Total effort estimate

~5 hours of CC wall-clock time plus CI wait + human review gates.
M1 spec's 5 person-day estimate absorbs human review and any
HALT-resolution or manual verification time.

## Partial-results policy (per approval update #5)

Expected Partial count at start: **0** (tools are installed).
Any Partial that arises during execution is **reported immediately
in chat**, not accumulated toward the soft-HALT threshold of 3.
The only plausible Partial-trigger during execution is S6's
valgrind × AppProtection interaction; if that fires, I pause and
surface it before continuing to S7.

## Out-of-band interventions I may request

- **At S1**: HALT if any of `scrot`, `radeontop`, `valgrind` is
  missing (pre-probe expected them installed; absence now is a
  state-reconciliation failure worth stopping for).
- **At S6 valgrind attempt**: one workaround attempt max. If
  AppProtection recurses with valgrind malloc, record the block
  and continue on the non-valgrind arm — but surface the event
  in chat first.
- **At S8 watch**: if CI fails for a non-environmental reason (Qt
  install action update, runner-image compiler drift), raise the
  failure and stop — M1 §6-4 HALT.
- **At S9 report-writing**: if any Partial landed, flag before
  writing the report so the human knows what the matrix will say.
