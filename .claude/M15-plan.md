# M15 — Plan (V0.2 AI Vision Infrastructure)

Pairs with `.claude/M15-understanding.md`. Source of truth:
`docs/milestones/M15-vision-infrastructure.md` at `4c3cffe`.
V0 charter `docs/V0-series-charter.md` governs.

V0.2 has **no calendar commitment** per V0 charter §4 + M15
spec §8. This plan sketches the subtask sequence + HALT
triggers; per-subtask LOC and durations are nominal and may
grow as the infrastructure surfaces edge cases.

---

## 0. Methodology

- **Infrastructure-first**: M15 builds the perception loop;
  V0.3 (M16+) uses it. No UI/UX fixes during M15
  (constraint per spec §2.2 #1).
- **Backend frozen** per V0 charter §3. M15 work lives in
  `tests/visual/`, `tests/screenshots/`, `docs/`, and small
  additions to non-frozen surfaces (`main_window.cpp`,
  harness shell scripts, CMake test-target wiring).
- **Per-subtask deliverable + commit** (continuing M14
  M14.3 P pattern). Each subtask closes with a single
  logical commit + push + CI green before the next.
- **Pre-commit gate** for any code-touching subtask: Debug
  + Release + debug-asan build clean; ctest 608+ green
  (the +N from new visual tests); existing M14 S1 smoke
  + mechanical-18 still PASS.
- **Documentation-only commits** use the CLAUDE.md
  §Required #2 exception (M15-progress, design decisions
  in concerns, baseline coverage map, etc.).
- **Frozen-surface budget**: 0/2 at M15 entry (V0.1 close).
  HALT #5 fires at > 2 modifications. CC tracks the
  running count in `M15-progress.md`.
- **No operator-driven visual evaluation in CI** (constraint
  spec §2.2 #3). Operator approves baselines once at S3;
  CI then runs vision-LLM autonomously.

## 1. Subtask sequence

| ID | Title | Output | Operator-blocking? |
|---|---|---|---|
| S0 | Concerns C1–C6 + M15.2 vision-LLM survey + Phase 4 lock prep | `M15-concerns.md`, vision-LLM recommendation doc | yes (Phase 4 lock) |
| S1 | Screenshot capture infrastructure (mechanism C in-process + mechanism B xvfb+xwd hybrid; per spec §3 M15.1 D) | `tests/visual/lib/capture.{py,sh}` + `MainWindow::captureScreenshot` non-frozen public method + new CLI flag `--capture-screenshot=<path>` | no |
| S2 | Vision-LLM integration per M15.2 lock | `tests/visual/lib/describe_screenshot.py` (or chosen impl) + at least 3 visual tests using it | no |
| S3 | Baseline coverage (Y-scope; ~30–50 states) | PNGs under `tests/visual/baselines/` + `M15-progress.md` coverage map | yes (operator one-time approval) |
| S4 | Test framework integration: extend M14 S1 + mechanical-18 to capture screenshots; add `tests/visual/` suite; automate GUI subset of 18-test (T4 / T6 / T9 / T13–T18) + revisit M14-deferred T7 / T8 / T11 | extended harnesses + new test files | partial (operator confirms automation matches manual flow once) |
| S5 | CI integration: artifact upload + PR diff display + `accept-baseline.sh` script | `.github/workflows/ci.yml` extension + `scripts/accept-baseline.sh` | no |
| S6 | CC autonomy demonstration: end-to-end run-test → describe → diagnose → propose-fix without operator | `docs/m15-cc-autonomy-demo.md` recording the run | no (CC self-test) |
| S7 | M15-done.md + V0.3 hand-off | `docs/v0.2-vision-infrastructure.md` + `tests/visual/README.md` + `M15-done.md` with coverage map + V1 UX gap inventory + industrial software references | yes (Phase 2 review) |

## 2. Time budget

**No calendar commitment** per V0 charter §4. Quality is the
only gate. M15 closes when all hard-stop criteria
(understanding §3) are met.

For rough sequencing: S0 + S1 + S2 are the foundation (cannot
start S3 until S2 lands the vision-LLM + visual-test pattern).
S3 is the heart of the milestone (baselines). S4–S6 are
incremental on top. S7 is closure.

## 3. HALT triggers (M15-specific, on top of CLAUDE.md §HALT)

Per spec §6:

| # | Trigger | Source / detection | Action |
|---|---|---|---|
| H1 | Vision LLM cannot reliably describe SignalForge GUI | S2 prompt-engineering iteration; observed accuracy on baseline images < 90 % | HALT; re-evaluate Claude API vs local vs prompt design |
| H2 | Screenshot capture mechanism unreliable (false flake rate > 5 %) | S1 / S3 stability run (10× per state) | HALT; investigate Qt rendering determinism |
| H3 | Baseline approval workflow becomes operator burden (> 5 baselines failing per audit cycle) | S3 / S5 operator interaction | HALT; revisit M15.5 (current U → maybe V tolerance or T soft-fail) |
| H4 | CC vision-driven self-test demonstrates accuracy issues | S6 demonstration; baseline ground truth fails to match vision-LLM verdict | HALT; investigate prompt design + retry strategies |
| H5 | CI cost / time excessive (> 30 min added to PR runs) | S5 PR-trigger CI duration | HALT; move some checks to release-only gate |
| H6 | Existing M14 frameworks resist extension (per M15.4 Q) | S4 implementation | HALT; reconsider P (separate suite) or hybrid R |

Plus CLAUDE.md standard set (compile error 3×, test fail 3×,
new dep, frozen-interface mod without ADR, perf miss after 1
opt pass, spec/arch contradiction, Qt 6.10 anomaly, two
plausible impls, unexplained git failure).

## 4. Subtask details

### S0 — Concerns + M15.2 vision-LLM survey

**Inputs**: spec §3 (5 design decisions) + spec §6 (HALT
triggers) + understanding §5 (open questions) + this plan
§3.

**Output**: `.claude/M15-concerns.md` resolving:

- C1 — capture mechanism implementation order (start with
  C in-process + add B xvfb+xwd as needed). Per spec §3
  M15.1 D recommendation.
- C2 — **M15.2 vision-LLM survey** (the deferred decision):
  - Confirm CC's Read tool supports image input (PNG /
    JPEG) per Read's tool description. **Quick test**:
    capture a small reference PNG; Read tool can view it.
    If yes, **CC native** is the dev workflow.
  - Document Claude API path for CI (HTTP call from
    Python test harness with base64-encoded screenshot;
    `claude-3-5-sonnet-20241022` or successor; ~2–5 s
    latency; per-image cost; structured-JSON prompt).
  - Document local-model option for completeness; note CI
    overhead (GPU runners or slow CPU; model download
    pipeline; significantly larger CI image) → **not
    recommended** for V0.2.
  - Document multi-vendor option (OpenAI GPT-4V / Gemini)
    for completeness; **not recommended** unless
    multi-vendor mandate.
  - **Recommendation**: hybrid — CC native (Read tool) for
    interactive development; Claude API for CI gates.
    Phase 4 reviewer locks or chooses alternative.
- C3 — baseline coverage Y-scope state enumeration
  (~30–50 states). Spec §5.3 lists the categories;
  concerns lists the actual state names + paths.
- C4 — test-framework layout: `tests/visual/` directory
  alongside `tests/integration/gui/`; visual tests written
  in Python (test harness) calling the C++ binary like
  M14 S1; baseline diffing in Python; vision-LLM call in
  Python.
- C5 — CI failure mode: U (hard-fail + manual override)
  per spec recommendation. `scripts/accept-baseline.sh`
  takes a test name + accepted-actual path; updates
  `tests/visual/baselines/`.
- C6 — frozen-surface counter — start at 0/2; track in
  `M15-progress.md`.

**Effort**: docs-only commit
(`docs: M15 S0 — concerns C1-C6 + M15.2 vision-LLM survey`).

**Phase 4 gate**: human reviews S0 + locks M15.2.

### S1 — Screenshot capture infrastructure

**Inputs**: spec §4.1; M14 S1 + mechanical-18 patterns
(`tests/integration/gui/release_binary_smoke.sh`,
`run_mechanical_18.sh`); existing
`MainWindow::grabChartImage` (M14 S1).

**Output**:

- `MainWindow::captureScreenshot(const QString& path)` —
  non-frozen public method; uses `widget->grab()` for
  full window; saves PNG. Reuses
  `--capture-screenshot=<path>` CLI flag (new) + existing
  `--exit-after-ms` for orchestration.
- `tests/visual/lib/capture.py` — Python helper that
  launches `signalforge`, drives fixture, captures
  screenshot. Wraps mechanism C (in-process via
  `--capture-screenshot`) and mechanism B (xvfb +
  `xwd | convert`) per spec §3 M15.1 D.
- `tests/visual/CMakeLists.txt` — wire the new tests into
  ctest under a `visual` label.
- One end-to-end "smoke" visual test that captures the
  M14 S1 fixture's chart pane and saves a baseline.

**Effort**: 1 commit (`build: M15 S1 — screenshot capture
infrastructure`).

### S2 — Vision-LLM integration

**Inputs**: M15.2 Phase 4 lock + S0 concerns C2 +
spec §4.2.

**Output**:

- `tests/visual/lib/describe_screenshot.py` — function
  takes PNG path, returns structured JSON GUI description
  per spec §4.2 schema.
- Prompt template producing reliable structured output
  (low-temperature; schema-constrained; SignalForge
  context-prefix).
- ≥ 3 visual tests using the function (per spec §5.2).
- Error handling: retry on transient errors, fail-fast on
  auth errors, deterministic fallback to "vision
  unavailable" verdict that surfaces in test output.

**Effort**: 1–2 commits (depending on whether prompt
iteration warrants its own commit).

### S3 — Baseline coverage (Y-scope)

**Inputs**: S0 C3 state enumeration; S1 capture
infrastructure; S2 vision-LLM verdict.

**Output**:

- One PNG per state under
  `tests/visual/baselines/<state-name>.png`. Y-scope
  ~30–50 baselines covering: idle, connecting, connected
  (1 / 2 / N drivers), recording active, replay loaded /
  playing / paused, file-open dialog, connection-edit
  dialog, quit-while-recording confirm, live↔replay
  3-option dialog, error states, etc.
- `M15-progress.md` coverage map listing each baseline +
  the test that produces it.
- One-time operator review + approval (single sit-down,
  not ongoing).

**Effort**: 1 commit per logical group (likely 2–3
commits).

**Operator-blocking**: yes, baseline approval (one-time).

### S4 — Test framework integration

**Inputs**: S1 + S2 working; M14 mechanical-18 + S1 smoke
as targets to extend.

**Output**:

- M14 S1 smoke captures a baseline screenshot
  post-fixture-load (regression-protected).
- M14 mechanical-18 (T3 today) extended with screenshot
  capture per test.
- Visual-test versions of the M14 GUI subset:
  T4 (Replay file picker), T6 (Auto-connect on startup),
  T9 (Quit-while-recording prompt),
  T13 (Replay GUI open), T14 (Play/Pause UI),
  T15 (Step ◀/▶ visual), T16 (Scrubber drag),
  T17 (Speed combo visual), T18 (Live↔Replay 3-option).
- M14-deferred T7 / T8 / T11 (UdpDriver headless
  readyRead race) revisited — does the visual layer
  resolve them, or document why visual approach can't
  help.

**Effort**: 1 commit per test group or per subsystem.

### S5 — CI integration

**Inputs**: S4 visual tests passing locally; existing
`.github/workflows/ci.yml`.

**Output**:

- `.github/workflows/ci.yml` extension to upload
  `tests/screenshots/**` as GitHub Actions artifacts
  on every CI run.
- PR-comment automation (or branch-protection check)
  showing baseline + actual + diff on visual-test
  failures.
- `scripts/accept-baseline.sh` — takes test name + path;
  copies actual → baseline; commits via the operator's
  workflow. Documented in `tests/visual/README.md`.

**Effort**: 1 commit (`build: M15 S5 — CI integration +
accept-baseline workflow`).

### S6 — CC autonomy demonstration

**Inputs**: S1–S5 complete.

**Output**:

- `docs/m15-cc-autonomy-demo.md` — recorded end-to-end
  CC run: introduce a deliberate visual regression in
  a test branch; CC runs visual test; CC views screenshot
  via vision-LLM; CC describes the regression; CC proposes
  the fix. All without operator GUI dogfood.
- Demonstrates the V0 charter §1 perception loop closure.

**Effort**: 1 commit (`docs: M15 S6 — CC vision-driven
self-test demonstration`).

### S7 — M15-done.md + V0.3 hand-off

**Inputs**: all prior subtask outcomes; S3 baseline
collection + S6 demo.

**Output**:

- `docs/v0.2-vision-infrastructure.md` — usage guide for
  developers / future CC sessions.
- `tests/visual/README.md` — how to add new visual tests.
- `.claude/M15-done.md` per spec §7:
  - Visual baseline coverage map
  - V1 UX gap inventory (catalog of every UX issue
    visible in baselines — chart line color, status-bar
    density, dialog text, menu organization, signal
    selector ergonomics, multi-chart layout, replay
    toolbar, status indicators, theme gap, etc.).
    This becomes V0.3 (M16+) design input.
  - Industrial software references collected
    opportunistically during M15 (LabVIEW, MATLAB
    Instrument Control, Tektronix, Saleae Logic, NI
    VeriStand, Yokogawa IS-Series).
  - V0.3 spec-writing scope (M16 design tokens / M17
    core widget rebuild / M18 workflow rebuild —
    possibly more).

**Effort**: 1 commit.

## 5. Operator-blocking deliverables

CC-blocking (M15 commits land these autonomously):

- S0 concerns + M15.2 survey
- S1 screenshot capture infrastructure
- S2 vision-LLM integration
- S3 baseline screenshots
- S4 test-framework extensions
- S5 CI integration
- S6 CC autonomy demonstration
- S7 documentation + done.md

Operator-blocking:

- Phase 4 review of S0 + lock M15.2 (vision-LLM choice)
- S3 one-time baseline approval
- S4 confirm visual-test automation matches manual flow
  (one-time review)
- S7 Phase 2 review of M15-done.md → "approved, merge V0.2
  close and begin V0.3 (M16) bootstrap"

## 6. Branching + PR plan

- `milestone/M15` branched from `main` (V0.1 close at
  `81fe24d`). Already created locally + pushed.
- M15 PR opens at S7 closure: `milestone/M15 → main` (per
  V0 charter §6 + V1 PR pattern).
- After PR merge: tag `v0.2.0` (internal milestone, no
  GitHub Release publish per V0 charter §5). Bootstrap
  M16 next session.

## 7. Cross-references

- Spec: `docs/milestones/M15-vision-infrastructure.md`
- Understanding: `.claude/M15-understanding.md`
- V0 charter: `docs/V0-series-charter.md`
- V0.1 status: `docs/v0.1-status-summary.md`
- M14 done (predecessor): `.claude/M14-done.md`
- M14 mechanical-18 framework (extension target):
  `tests/integration/gui/run_mechanical_18.sh` +
  `tests/integration/gui/README.md`
- M14 S1 smoke (extension target):
  `tests/integration/gui/release_binary_smoke.sh`
