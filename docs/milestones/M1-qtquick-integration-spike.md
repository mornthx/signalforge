# M1 — Qt Quick Integration Spike

**Milestone ID**: M1
**Sprint**: 1–2 (bridging week)
**Estimated effort**: 5 person-days
**Prerequisites**: M0 closed (main at v0.0.1-alpha.1 baseline)
**Next milestone**: M2
**Hard-stop type**: Technical decision (human picks rendering approach)
**Branch**: `milestone/M1`

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR M<n>]` — Milestone Roadmap, entry for milestone `M<n>`
- `[CM §X]` — CLAUDE.md, section X

---

## 1. Goal

Produce enough evidence for the human to decide, with confidence, whether `QQuickWidget` can carry the V1 chart workload — or whether a fallback (QWindow container, or QPainter/OpenGL self-render) must be adopted before M6 starts.

This milestone does not implement any chart. It only exercises Qt Quick in dock-panel scenarios that mirror what M6 will do, and reports what happens with numbers and screenshots.

---

## 2. Scope

### 2.1 Must deliver

1. A standalone spike program at `tools/spike/qquick_dock_test/` with its own `CMakeLists.txt`. **This directory is NOT added to the top-level `CMakeLists.txt`** — it builds independently using the same Qt install and presets.
2. The spike program embeds **three** `QQuickWidget` instances in a `QMainWindow` with three `QDockWidget` containers.
3. Each `QQuickWidget` renders a simple QML scene: a colored `Rectangle` with a rotating geometry (`Canvas` with sine-wave or similar) to ensure the render thread is active.
4. Five integration checks covering the five concerns in `[Arch §8.5]`:
   - **Check 1** — Floating / re-docking / cross-monitor drag survival
   - **Check 2** — HiDPI rendering at 125%, 150%, 175%, 200%
   - **Check 3** — Context-menu event propagation across the Qt Widgets ↔ Qt Quick boundary
   - **Check 4** — Hide / show render-thread lifecycle (no leaks)
   - **Check 5** — Multi-instance GPU resource usage
5. `docs/spikes/M1-qtquick-integration.md` — a written report with:
   - A verdict matrix (§2.2 below)
   - Per-check methodology, measurements, and screenshots (where applicable)
   - A machine state summary (OS, kernel, GPU, Qt install path, whether AppProtection is active)
   - A "fallback impact if Check K fails" note for each check
6. Screenshots and benchmark data under `docs/spikes/M1-artifacts/`.
7. `.claude/M1-partial-results.md` if any check could not be executed locally (see §5 soft-HALT rules).

### 2.2 The verdict matrix

The report's central deliverable is this table. Each cell is **Pass / Partial / Fail / Blocked**, with evidence cited.

| Check | Local (dev host) | CI (GitHub Actions headless) | Final verdict |
|---|---|---|---|
| 1. Floating / re-docking | | | |
| 2. HiDPI scaling | | N/A (headless) | |
| 3. Context-menu propagation | | | |
| 4. Hide/show lifecycle | | | |
| 5. Multi-instance GPU | | | |

"Final verdict" is CC's synthesized call per check, with rationale. The human reads the whole table and makes the go/no-go decision — CC does **not** recommend a final direction.

### 2.3 Must not do

1. **No production chart code.** M6 owns that.
2. **No modification to `src/`**. The spike is isolated in `tools/spike/`.
3. **No addition to top-level `CMakeLists.txt`**. The spike builds via its own CMake invocation.
4. **No new top-level dependencies**. Use only what's already in `[Arch §4.1]`.
5. **No performance benchmarking beyond what each check requires.** Frame rate, latency, and throughput numbers belong to M6.
6. **No recommendation in the report on whether to proceed with QQuickWidget.** CC's job is evidence; the human's job is decision.

### 2.4 Optional

1. A short `tools/spike/qquick_dock_test/README.md` explaining how to run the spike manually.
2. A CI job (separate from the main `build` matrix) that runs the headless subset on each push to `milestone/M1`. This is nice to have; do not block M1 completion on it.

---

## 3. Subtask classification (for soft-HALT semantics)

This milestone introduces a new semantic: each subtask is marked `blocking` or `independent`.

- **Blocking**: failure triggers a hard HALT per `[CM §HALT]`.
- **Independent**: failure is recorded in `.claude/M1-partial-results.md`, execution continues with the remaining subtasks. The final report explicitly lists which checks are partial and why.

| Subtask | Classification | Rationale |
|---|---|---|
| S1. Environment preflight | Blocking | Without Qt Quick loadable, the spike has no point |
| S2. Spike program skeleton | Blocking | All checks depend on this |
| S3. Check 1 — Floating | Independent | Each check stands alone |
| S4. Check 2 — HiDPI | Independent | Screen-dependent; may be blocked by environment |
| S5. Check 3 — Context menu | Independent | Event-system-specific |
| S6. Check 4 — Hide/show | Independent | Valgrind may be blocked by AppProtection (like M0's C2) |
| S7. Check 5 — Multi-instance GPU | Independent | GPU telemetry depends on hardware |
| S8. CI headless subset | Blocking | Needed as fallback evidence for any check blocked locally |
| S9. Report generation | Blocking | The milestone's actual deliverable |

**Rule for independent subtasks**: if a subtask fails for environmental reasons (tooling unavailable, security-software interference, display server limitation), record it in `.claude/M1-partial-results.md` with exact error output and continue. Do NOT attempt more than one local workaround before moving on — escalation to CI is the designed fallback path.

**Rule for blocking subtasks**: standard HALT semantics apply.

---

## 4. Per-subtask specifications

### S1 — Environment preflight (Blocking)

Before writing any spike code:

- Verify Qt 6.10.2 is at the expected location (per `$SIGNALFORGE_QT_PATH` or default).
- Verify `QT_QPA_PLATFORM` can be set to `xcb` (Linux X11 session) or `wayland`.
- Verify `QT_SCALE_FACTOR` environment variable is respected (confirm by reading docs, not by running a full test yet).
- Verify one GPU telemetry tool is available: `intel_gpu_top`, `nvidia-smi`, `radeontop`, or `glxinfo`. If none, record which is the best available and proceed — Check 5 will adapt.
- Verify `valgrind` is installed (for Check 4). If not, that is an S6-independent failure, not an S1 blocker.
- Record findings in `.claude/M1-progress.md` under "S1 preflight".

Commit with message: `spike: M1 preflight findings`. (Docs-only commit, no build change.)

### S2 — Spike program skeleton (Blocking)

Create `tools/spike/qquick_dock_test/` with:

```
tools/spike/qquick_dock_test/
├── CMakeLists.txt
├── main.cpp
├── main_window.hpp
├── main_window.cpp
└── qml/
    └── DockContent.qml
```

The spike's `CMakeLists.txt`:

- Uses `cmake_minimum_required(VERSION 3.22)`.
- `project(qquick_dock_test CXX)`.
- Finds Qt6 components `Core Widgets Quick QuickWidgets`.
- Produces one executable named `qquick_dock_test`.
- Does NOT depend on anything from the top-level `src/` tree.

`main_window.hpp` / `main_window.cpp`:

- A `QMainWindow` subclass.
- Constructs three `QDockWidget` instances.
- Each dock contains one `QQuickWidget` loading `qml/DockContent.qml`.
- Provides a menu bar with a "Hide Dock 1" / "Show Dock 1" toggle and an explicit "Quit" action.
- Emits SF_LOG_INFO-style stderr messages on significant events (dock floated, dock re-docked, QML loaded, render context created).
  - Since the spike is independent of `src/observability/`, a minimal local logger is acceptable: `qDebug` is fine **inside the spike** as an exception to `[CM §Forbidden-6]`. Document this exception in the spike's README.

`qml/DockContent.qml`:

- Root `Item` with a colored `Rectangle` background.
- A `Canvas` or `Shape` that redraws at 30Hz (use `Timer` with 33ms interval) to keep the render thread active.
- A visible label showing the dock's ID and current state (for screenshot evidence).

Build verification:

```
cd tools/spike/qquick_dock_test
cmake -B build -S . -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.2/gcc_64
cmake --build build
```

Must succeed with zero warnings. Run `./build/qquick_dock_test` once under `xvfb-run --auto-servernum` to confirm it doesn't immediately crash.

Commit: `spike: scaffold qquick_dock_test with three docked QQuickWidgets`.

### S3 — Check 1: Floating / re-docking / cross-monitor drag (Independent)

**What to verify**: `QQuickWidget` in `QDockWidget` survives the following operations without crashes, visual glitches, or orphaned render contexts:

1. Dock panel → detach to floating window
2. Floating window → drag to different screen area
3. Floating window → re-dock back to main window
4. Repeat 5 cycles

**Method**:

- Two sub-approaches: `manual` (human runs the spike locally) and `automated` (uses `QTest::qWait` + `QDockWidget::setFloating(true/false)` programmatically).
- **Do the automated version**. It's deterministic and reproducible.
- Add a command-line flag to the spike: `--auto-check 1` runs the Check 1 sequence, logs results to stderr, and exits with 0 (success) / non-zero (failure) in ~10 seconds.
- Success criteria: 5 cycles complete, no QML warnings in stderr, no `QSG` warnings, no segfault.

**Artifacts**:
- Save stderr to `docs/spikes/M1-artifacts/check1-log.txt`.
- Capture a screenshot at the end of cycle 5 (all docked) via `grim` / `scrot` / `import` (whichever is available). Save as `check1-end-state.png`.

**What failure looks like**: segfault during float/dock toggle, "QQuickWidget: Attempted to create a render target..." warnings, rendering pipeline stalls (rectangle stops animating).

**Fallback impact note**: If Check 1 fails, QQuickWidget in dock panels is effectively unusable for V1. Either fallback becomes the primary recommendation candidate.

### S4 — Check 2: HiDPI rendering at 125% / 150% / 175% / 200% (Independent)

**What to verify**: `QQuickWidget` content renders crisply at non-integer DPI scaling; no blurring, no clipping, no misaligned pixels.

**Method**:

- For each scale factor in {1.25, 1.50, 1.75, 2.00}:
  1. Launch the spike with `QT_SCALE_FACTOR=<value>` and `QT_AUTO_SCREEN_SCALE_FACTOR=0`.
  2. Show all three docks in their default layout.
  3. Capture a full-window screenshot after a 2-second settle time.
  4. Save as `docs/spikes/M1-artifacts/check2-scale-<value>.png`.
- Visual inspection is required (this is the part the human does). CC cannot judge "crisp" objectively.
- CC does verify objectively: no QML warnings at startup at each scale, the window geometry is proportionally correct (width/height × scale ≈ observed pixel dimensions).

**Artifacts**:
- 4 screenshots, one per scale factor.
- `check2-log.txt` with startup stderr at each scale.

**Environmental note**: this check requires an actual X or Wayland display. If run under `xvfb` the screenshots may not reflect real visual behavior. **Running under xvfb is acceptable evidence for CC's automated part, but the report must state this clearly so the human knows to re-verify on a real display if needed.**

**Fallback impact note**: HiDPI issues in QQuickWidget are the most common failure mode historically. If this check fails, the fallback decision depends on severity — blur alone might be tolerable for V1; geometry misalignment is not.

### S5 — Check 3: Context-menu event propagation across Widgets ↔ Quick boundary (Independent)

**What to verify**: A right-click on the QML content inside a dock surfaces a `QMenu` (a Qt Widgets object) at the correct screen position.

**Method**:

- In `DockContent.qml`, emit a custom signal on `MouseArea` right-click: `signal contextMenuRequested(point position)`.
- In the C++ side, connect that signal to a slot that creates a `QMenu` with two actions ("Action A", "Action B") and calls `menu->exec(globalPos)`.
- Add `--auto-check 3` flag: simulates a right-click using `QTest::mouseClick` on each dock, verifies the menu appears (check via `QApplication::activePopupWidget()` being non-null within 500ms), triggers "Action A", verifies the menu closes and the action fires.
- Repeat for all three docks.

**Success criteria**: For each dock, the menu appears within 500ms, the correct action fires when triggered, no orphaned widget references remain after closing.

**Artifacts**:
- `check3-log.txt` with events logged.
- `check3-menu-screenshot.png` captured during one of the menu-open states.

**Fallback impact note**: If context-menu propagation fails, the Control page (M7) and chart interaction (M6) lose a major interaction paradigm. This is a medium-severity failure.

### S6 — Check 4: Hide / show lifecycle without leaks (Independent)

**What to verify**: Toggling `QDockWidget::hide()` / `show()` 20 times does not leak GPU resources, file descriptors, or memory.

**Method**:

- Add `--auto-check 4` flag: hide Dock 1, wait 500ms, show Dock 1, wait 500ms — repeat 20 times.
- Between cycles, measure: `/proc/self/status` VmRSS, open file descriptors count (`/proc/self/fd | wc -l`).
- Run the whole sequence under `valgrind --tool=massif` to capture a heap profile; also under `valgrind --leak-check=full` on a shorter 5-cycle run.
- If `valgrind` is blocked by the local AppProtection software (as in M0's C2), record the block, skip the valgrind part, and rely on the `/proc/self/status` sampling only.

**Success criteria**:

- VmRSS growth across 20 cycles is < 10 MB (i.e., not unbounded).
- Open FDs at the end equal the start (within ±2 for transient file accesses).
- If valgrind ran: no "definitely lost" bytes attributable to spike code.

**Artifacts**:
- `check4-memory-trace.csv` with (cycle, rss_mb, fd_count) rows.
- `check4-valgrind.txt` if valgrind ran, or `check4-valgrind-blocked.txt` explaining why not.

**Fallback impact note**: If hide/show leaks, live observation use cases (floating a chart panel, minimizing to focus on another) become memory hazards for long sessions. High-severity.

### S7 — Check 5: Multi-instance GPU resource usage (Independent)

**What to verify**: Three simultaneous `QQuickWidget` instances share the render thread / GPU reasonably, not exploding memory or FPS.

**Method**:

- Add `--auto-check 5` flag: starts all three docks rendering, samples GPU telemetry every 500ms for 30 seconds.
- GPU telemetry source (use the first available from preflight):
  - `intel_gpu_top -J -s 500` → parse JSON for `"engines"` → GPU utilization %
  - `nvidia-smi --query-gpu=memory.used,utilization.gpu --format=csv,nounits --loop-ms=500`
  - `radeontop -d - -l 60` → parse GPU and memory fields
  - `glxinfo | grep "Video memory"` baseline + process-level `/proc/self/status` if nothing else works
- Also log CPU % of the spike process via `top -b -n 60 -d 0.5 -p <pid>`.

**Success criteria**:

- GPU memory used by the spike process: < 200 MB total across all three widgets (empirical baseline; higher means trouble).
- GPU utilization sustained < 60% (otherwise 20 charts in M6 will saturate).
- CPU utilization of the spike process: < 30% single-core.
- No GPU-context-lost warnings in stderr.

**Artifacts**:
- `check5-gpu-trace.csv` with samples.
- `check5-summary.md` with min/max/mean of each metric.

**Fallback impact note**: If three QQuickWidgets already strain the GPU, 20 of them (M6 target) won't work. This check is the most predictive of M6 feasibility.

### S8 — CI headless subset (Blocking)

Why this exists: several checks above may be blocked locally by environment issues (AppProtection, no GPU telemetry, display server limitations). A headless CI run provides contamination-free evidence for the subset of checks that don't need a real display.

**What runs in CI**:

- Check 1 (floating) — works headless with xvfb
- Check 3 (context menu) — works headless with xvfb
- Check 4 (hide/show) — partial, without valgrind (CI doesn't have the minutes budget)
- Check 5 (GPU) — **skipped** in CI (no discrete GPU on runner)
- Check 2 (HiDPI) — **skipped** in CI (no real display)

**Implementation**:

- Add `.github/workflows/m1-spike.yml` that:
  - Triggers only on push to `milestone/M1`
  - Installs Qt 6.10.2 via `jurplel/install-qt-action@v4` (same as the main CI)
  - Builds `tools/spike/qquick_dock_test/`
  - Runs `xvfb-run --auto-servernum ./build/qquick_dock_test --auto-check 1`
  - Runs `xvfb-run --auto-servernum ./build/qquick_dock_test --auto-check 3`
  - Runs `xvfb-run --auto-servernum ./build/qquick_dock_test --auto-check 4 --short`
  - Uploads artifacts (`check*-log.txt`) for inspection
- Exit status of each check is the job's success criterion.

**Success criteria**: workflow file exists, runs on milestone/M1 push, three jobs (check1, check3, check4) are green.

**Report integration**: the report cites the CI run URL and result for each headless-capable check, side-by-side with the local result.

Commit: `ci: add M1 spike headless verification workflow`.

### S9 — Report generation (Blocking)

Produce `docs/spikes/M1-qtquick-integration.md` with the structure:

```
# M1 — Qt Quick Integration Spike Report

## Executive verdict matrix

(The 5×3 table from §2.2, filled in.)

## Environment

- OS + kernel
- Qt install path and version
- Display server (X11 / Wayland)
- GPU and driver version
- CPU model
- AppProtection.so status: active / inactive
- Whether valgrind is usable on this host

## Per-check results

### Check 1 — Floating / re-docking

**Method**: (1–2 sentences)
**Result (local)**: Pass / Partial / Fail / Blocked, with one-line summary
**Result (CI)**: Pass / Partial / Fail / Blocked
**Artifacts**: links to files in M1-artifacts/
**Issues found**: (if any)
**Fallback impact**: (copied from §S3)

(Repeat structure for Checks 2–5.)

## Headless vs. local discrepancies

If any check passed in one environment and failed in the other, document why and what it means.

## Blocked items

(From `.claude/M1-partial-results.md`, consolidated.)

## Data for the human's decision

A closing section with:
- Which checks gave unambiguous Pass
- Which checks gave unambiguous Fail
- Which checks are ambiguous or environment-blocked, and what the human needs to physically verify to close them
- A list of downstream milestones that are affected by each type of failure, so the human can weigh cost

Do NOT include a recommendation. Do NOT conclude "go" or "no-go". That is the human's decision.
```

Commit: `docs: M1 Qt Quick integration spike report`.

---

## 5. Soft-HALT semantics in detail

This milestone is the first to use soft-HALT. Rules:

### 5.1 What qualifies as environmental failure

A subtask marked `independent` that fails for one of these reasons records partial results and continues:

- Required tool is not installed AND can't be installed without root / admin action
- Required tool is installed but refuses to run (AppProtection-style interference)
- Required hardware is not present (no GPU, no second monitor)
- Display server doesn't support what the check needs (Wayland-only feature under X11, or vice versa)
- Telemetry source returns empty or permission-denied data

### 5.2 What does NOT qualify

The following are still hard HALTs even on independent subtasks:

- Compile errors in the spike code itself
- QML syntax errors
- Logic bugs in the spike's test harness
- Git operation failures
- Ambiguous spec (apply `[EM §Ambiguity handling]`)

### 5.3 Process

When an environmental failure hits:

1. Write to `.claude/M1-partial-results.md` (create if needed):
   ```
   ## Check K — <name> — Partial
   
   **Reason**: <specific environmental cause>
   **Error output**: <verbatim>
   **Workaround attempted**: <one attempt max, or "none because <reason>">
   **CI alternative**: <will CI cover this check? yes/no>
   **Impact on milestone**: <what the report will say>
   ```
2. Log to `.claude/M1-progress.md` as "Check K partial, moving on".
3. Continue with the next subtask.
4. In the final report, the check's verdict is "Partial (local) + <CI result>" or "Blocked" if neither can verify.

### 5.4 Completion criteria with partials

M1 can complete successfully with up to **two independent subtasks** in Partial state, provided:

- S8 (CI headless subset) is Pass
- The report clearly identifies what the human needs to verify manually to close the partial checks
- No Blocking subtask is in Partial state

If three or more checks are Partial, HALT — the environmental situation is likely too degraded for the report to be useful.

---

## 6. M1-specific HALT triggers

In addition to `[CM §HALT]`:

1. Qt Quick cannot load at all in the spike (`QQuickWidget::setSource` returns immediately with error, or no rendering output after 5 seconds on a valid display)
2. Any blocking subtask (S1, S2, S8, S9) fails
3. Three or more independent subtasks hit Partial state
4. CI for the spike workflow fails for non-environmental reasons (compile error, logic error, workflow syntax)

---

## 7. Acceptance criteria

### 7.1 Artifacts

- [ ] `tools/spike/qquick_dock_test/` exists with `CMakeLists.txt`, C++ sources, and QML
- [ ] Spike builds cleanly with zero warnings
- [ ] Spike runs without immediate crash (headless or local)
- [ ] `docs/spikes/M1-qtquick-integration.md` exists
- [ ] `docs/spikes/M1-artifacts/` contains screenshots and logs as specified per check
- [ ] `.github/workflows/m1-spike.yml` exists and has one successful run on `milestone/M1`
- [ ] `.claude/M1-partial-results.md` exists if any check is Partial

### 7.2 Report quality

- [ ] Verdict matrix is complete with no empty cells
- [ ] Environment section documents the test conditions
- [ ] Each check section has method, result, artifacts, and fallback impact
- [ ] Report gives no recommendation on go/no-go — human makes that call
- [ ] Report identifies which ambiguous items the human needs to verify visually

### 7.3 Process

- [ ] Commits follow `[CM §Required-3]` format
- [ ] At most 2 subtasks in Partial state
- [ ] Completion report `.claude/M1-done.md` lists the verdict matrix inline
- [ ] HALT reports, if any, are resolved

### 7.4 Freezes established in this milestone

**None expected.** M1 is a spike; its output is a report, not a committed interface. Record absence of freezes in the done report.

---

## 8. What the human does after CC delivers

1. Read `docs/spikes/M1-qtquick-integration.md` in full.
2. For any check marked "verify visually" — open the spike locally and look.
3. Make the technical decision: **Go (QQuickWidget)** / **Downgrade (QWindow container)** / **Aggressive downgrade (QPainter + OpenGL)**.
4. Record the decision in `docs/architecture/architecture.md §8.5` as an amendment, or in a new ADR under `docs/architecture/decisions/`. If the decision is Go, a one-line note suffices. If it's a downgrade, the note must explain why and update M6's expectations.
5. Merge `milestone/M1` to `main`, tag `v0.0.2-alpha.1`.

The human does not need CC's opinion on which option to pick. The report's job is to make the decision obvious, not to make it for the human.

---

## 9. Notes for CC

- **The spike is deliberately isolated.** Do not wire it into the main build. Do not share code with `src/`. If you find yourself wanting to add a utility to `src/utils/`, stop — that's M2's territory, not M1's.
- **Use `qDebug` inside the spike freely.** The spike is not production code; the normal `SF_LOG_*` rule is waived in `tools/spike/`. Document this in the spike's README.
- **Screenshots are evidence.** A report without them is insufficient. Even if you can only capture them under `xvfb`, capture and label them as such.
- **You are gathering evidence, not making recommendations.** The last paragraph of `docs/spikes/M1-qtquick-integration.md` should not begin with "I recommend". It should end with data.
- **If Check 5 reveals that three QQuickWidgets already strain the GPU**, do not panic. Your job is to report the number. The human decides what that number means for M6.
- **If AppProtection.so interferes**, that's Partial — same mechanism as M0's C2. Document it, escalate to CI, continue.
- **Observe the repo state at session start** (same discipline as M0's Phase B reconciliation). `git fetch && git status && gh repo view`. The last few M0 sessions showed that the local state can drift from expectations; check before assuming.

---

## Appendix A — Session opening message for CC

Use this as the first message to CC. Adjust the workspace path if needed.

```
You are Claude Code, working on the SignalForge project on branch milestone/M1.

Your task is M1: a Qt Quick integration spike. Evidence-gathering only; no production chart code.

Required reading, in this order:
1. CLAUDE.md
2. docs/claude-code/execution-manual.md (sections 1 through 7)
3. docs/architecture/architecture.md sections 8.5, 8.6, 12
4. docs/milestones/M1-qtquick-integration-spike.md — this is your spec

Before writing any code, observe the current repo state: `git fetch origin --prune && git status && gh repo view --json defaultBranchRef,visibility`. Confirm main is at v0.0.1-alpha.1 and milestone/M1 is a clean branch from main.

Then produce only these two files and stop:

Step 1 — .claude/M1-understanding.md:
- Restate the M1 goal in your own words (3–5 sentences).
- List ambiguities or contradictions you identified, with your default interpretation for each.
- Specifically list which subtasks you expect to be independently failure-prone on this host (AppProtection, display server, GPU telemetry availability).
- List HALT risks.

Step 2 — .claude/M1-plan.md:
- Break M1 into ordered subtasks matching S1 through S9 in the spec.
- For each: expected output files, rough effort, classification (Blocking/Independent), prerequisites.
- Mark commit points.
- Note which checks you expect to execute locally vs. which will lean on CI.

After both files exist, reply "M1 understanding and plan ready for review" and stop.
Do not proceed until I say "approved, begin M1 execution".
```

---

## 10. Closing note

M1 is the first milestone where the output is a report, not a runnable module. The report's value depends on honesty: if Check 5 shows concerning GPU use, the report says so in clear numbers. If the AppProtection host blocks Check 4, the report says that and cites CI.

A well-executed M1 that says "Check 2 failed, here's evidence" is more valuable to the project than a hand-waved M1 that says "all checks passed, looks fine". The decision this milestone supports (an approach choice for M6) only works if the evidence is trustworthy.

CC's task here is reporter, not advocate.
