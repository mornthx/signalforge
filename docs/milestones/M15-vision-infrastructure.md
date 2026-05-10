# M15 — V0.2 AI Vision Infrastructure

| Field | Value |
|---|---|
| Milestone ID | M15 |
| Series | V0.2 (V0 Series Charter applies) |
| Estimated effort | **No calendar commitment** (quality-first per V0 charter §4) |
| Prerequisites | V0.1 tagged (M14 close) |
| Next milestone | M16 (V0.3 design phase) |
| Hard-stop type | AI vision infrastructure functionally complete + CC can drive GUI testing autonomously |
| Soft-HALT allowed | No |
| Branch | `milestone/M15` |

---

## 1. Goal

M15 builds the infrastructure that enables CC and the AI advisor to **see**
the GUI directly, not just inspect logs and record counts. Every visual
quality dimension of the SignalForge GUI must be perceivable, capturable,
describable, and verifiable by CC autonomously.

After M15 closes:
- CC drives V0.3+ UI design + implementation iterations
- Operator transitions from "primary GUI tester" to "visual baseline
  approver"
- Every GUI test in the project produces a screenshot artifact
- CC can be asked "look at this screenshot, describe + verify the chart"
  and answer correctly
- Future GUI bugs of the F4-F19 class are caught in CI before reaching
  the operator

This is **infrastructure investment**, not feature delivery. M15 does NOT
fix UI/UX issues; that is V0.3's job. M15 creates the perception loop V0.3
needs.

## 2. Scope

### 2.1 Must deliver

1. **Screenshot capture mechanism** integrated into GUI test runs:
   - Per-test screenshot saved to `tests/screenshots/<test-name>/<state>.png`
   - At minimum captured at: post-launch, after each user action,
     before-test-end
   - Headless `Q_QPA_PLATFORM=offscreen` + xvfb-run compatible
   - Reproducible (deterministic test data, no real timestamps in
     screenshot region)

2. **Vision LLM integration**:
   - CC can invoke vision capability on captured screenshots
   - Vision model returns structured description: GUI elements present,
     their positions, colors, states, text content
   - Vision capability accessible from CC's tool set (decision M15.2 selects
     specific implementation)

3. **Screenshot baseline coverage** (per decision M15.3):
   - Every major GUI state has a baseline screenshot stored in repo
   - Diff comparison: each test run compares current screenshot to baseline
   - Visual regressions reported as test failures

4. **Test framework integration** (per decision M15.4):
   - Mechanical-18 framework (M14 S6) extended to capture screenshots
   - New "visual-test" suite for tests that require visual judgement
   - GUI subset of 18-test (T4/T6/T9/T13-T18) automated with screenshot
     verification + vision-LLM analysis
   - All M14-deferred GUI tests (T7/T8/T11 from headless race) revisited
     with new infrastructure

5. **CI infrastructure**:
   - Screenshot artifacts uploaded to GitHub Actions artifacts (or local
     equivalent)
   - PR diff display: screenshot comparison vs main branch
   - Failed visual tests show baseline + actual + diff

6. **Vision-driven self-test for CC**:
   - CC can run a GUI test, view its screenshot, and validate the result
     without operator intervention
   - Operator's role is "approve baseline screenshots when CC adds new GUI
     state coverage" — minimal ongoing burden

7. **Documentation**:
   - `docs/v0.2-vision-infrastructure.md` explains the system + how to add
     visual tests
   - V1.5+ governance lessons consolidated (this is the lesson V1 missed)

8. **`.claude/M15-done.md`** with hand-off to V0.3 (M16+) including:
   - Full GUI state baseline coverage map
   - List of UX issues visible in baselines (input to V0.3 redesign)
   - Vision-LLM integration validated working

### 2.2 Must not do

1. **No UI/UX fixes** during M15. V0.2 is infrastructure; V0.3 is fixes.
   M15-discovered UX issues are documented for M16+, NOT fixed in M15.
2. **No new functional features.** Backend frozen per V0 charter §3.
3. **No operator-driven visual evaluation in CI.** Operator approves
   baseline once; CI runs vision-LLM autonomously thereafter.
4. **No frozen-surface modification** without ADR (continuing M0-M14 pattern).
5. **No M14 deferred items addressed** (race condition, F5/F7/F8 etc.) —
   those wait for V0.3.

## 3. Design Decisions (5 to lock before Phase 4)

### M15.1 — Screenshot capture mechanism

How does the test harness capture the rendered GUI?

**A. QPixmap from QQuickWidget**
- In-process Qt API: `widget->grab()` produces QPixmap
- No external tools needed
- Requires test harness to control GUI directly (or expose IPC hook)
- Doesn't work for full window (menus, dialogs popping up)

**B. xwd + xvfb-run**
- External: launch app under xvfb, use `xwd` to capture full window
- Catches everything (menus, dialogs, status bar)
- Adds xwd dependency (small)
- Doesn't run on headless QPA-offscreen (xvfb required)

**C. Q_QPA_PLATFORM=offscreen + QPixmap snapshot via IPC**
- Existing M14 S1 smoke harness pattern
- Add IPC hook (signal or D-Bus) to trigger snapshot
- Saves to file on signal
- Already partially implemented (`--dump-chart-png` in M14 S1)

**D. Both B and C, depending on test type**
- Quick visual smoke: C (fast, in-process)
- Full system test: B (real X11 visual fidelity)

**Recommendation**: D. Most tests use C; full-window / dialog / menu tests
use B. Spec §4.1 defines per-test mechanism.

### M15.2 — Vision LLM integration

**DECISION DEFERRED** to M15 S0 concerns. CC's environment may or may not
support direct image viewing (Read tool with image input). The choice
between:

- Claude API vision endpoint (HTTP API call from test harness)
- Local vision model (e.g., LLaVA, Qwen-VL, self-hosted)
- Layered (local for fast smoke + Claude API for release gate)
- Other LLM provider with vision capability (OpenAI GPT-4V, Gemini, etc.)
- CC native image-viewing tool (if available in CC's runtime)

… should be made by CC at S0 after surveying:

1. Whether CC's current toolset includes direct image viewing
2. Cost / latency / privacy / network constraints for each option
3. Whether a local model can be reliably set up in CI without significant
   infrastructure overhead
4. Whether a hybrid approach (local for development, API for CI) is
   feasible

S0 concerns must produce one of:
- Recommendation with rationale → human approves at Phase 4
- Open question requiring human input → human chooses at Phase 4

The spec leaves the choice unfixed. M15 cannot start S1 implementation
until M15.2 is locked at Phase 4.

**Constraints on the chosen mechanism** (regardless of which LLM):
- Returns structured JSON description of GUI state (per §4.2)
- Compatible with both CC's local workflow and CI runner workflow
- Failure handling: retry on transient errors, fail-fast on auth errors
- No new external runtime dependency for SignalForge itself (vision
  infrastructure is test-only)

### M15.3 — Screenshot baseline coverage scope

Which GUI states get baseline screenshots?

**X. Minimal — just the 18-test states**
- ~17-18 baselines
- Covers acceptance test scope only

**Y. State-machine complete**
- Every major application state (idle / connecting / connected /
  recording / replay / dialog-open / error / multi-connection / etc.)
- ~30-50 baselines
- Catches state-transition bugs

**Z. Pixel-level comprehensive**
- Every menu open, every dialog, every chart with N signals,
  every theme, every locale (when added)
- 100+ baselines
- Ongoing maintenance overhead

**W. Y for V0.2; Z for V0.3+**
- V0.2 ships state-machine coverage
- V0.3 expands as redesign happens

**Recommendation**: W. Y is right balance for M15 ship.

### M15.4 — Test framework integration

How does new visual capability integrate with existing testing?

**P. New separate suite `tests/visual/`**
- Independent harness
- Easy to skip in fast CI runs
- Duplicates fixture infrastructure

**Q. Extend M14 S1 + mechanical-18 frameworks**
- Reuse existing fixture + harness infrastructure
- Tests gain screenshot output + vision check
- Fewer new files

**R. Hybrid: M14 frameworks for smoke, new `tests/visual/` for full**
- Smoke uses Q for speed
- Full visual coverage uses P for completeness

**Recommendation**: Q. Single framework reduces maintenance; P split adds
infrastructure debt without clear benefit.

### M15.5 — CI failure mode for visual regression

When a visual test fails (screenshot diff or LLM disapproval), what happens?

**S. Hard fail (blocks PR merge)**
- Strictest; ensures no visual regression
- Risk: brittle if screenshots are too tight (font rendering variations etc.)

**T. Soft fail (warns but doesn't block)**
- More tolerant of false positives
- Risk: visual regressions ignored

**U. Hard fail with manual override**
- Default hard fail
- Operator can mark "intended change, update baseline" via PR comment or
  separate `accept-baseline.sh` script
- Industry standard for visual regression testing (e.g., Percy, Chromatic)

**V. Hard fail for known states; soft fail for new states**
- Once a baseline exists, regression is blocked
- New states without baseline yet are soft fail (with prompt to add baseline)

**Recommendation**: U. Standard pattern; clear workflow.

## 4. Key Implementation Details

### 4.1 Screenshot capture per-test mechanism

```cpp
// In MainWindow or test harness
void MainWindow::captureScreenshot(const QString& path) {
    // Mechanism C: in-process QPixmap of full window
    QPixmap pixmap = grab();
    pixmap.save(path, "PNG");
}

// Triggered by:
// - SIGUSR1 handler (M14 pattern, extended)
// - --capture-screenshot=<path> CLI flag
// - DBus method (more invasive, defer to V0.3 if needed)
```

For full-window mechanism B (xwd):
```bash
# In CI test runner
xvfb-run --server-args="-screen 0 1920x1080x24" \
    bash -c "
        signalforge --auto-load-test-fixture &
        sleep 2
        xwd -root -out /tmp/screenshot.xwd
        convert /tmp/screenshot.xwd /tmp/screenshot.png
    "
```

### 4.2 Vision LLM workflow (example; mechanism per M15.2 lock)

The following is an **example skeleton** assuming Claude API. Actual
implementation depends on M15.2 decision (locked at Phase 4):

```python
# tests/visual/lib/describe_screenshot.py — example
# Implementation language + specific LLM provider per M15.2 decision.

def describe_screenshot(path: str) -> dict:
    """Return structured GUI description from a screenshot.
    
    Returns JSON-style dict with keys:
    - window_state: str (idle/connecting/connected/recording/replay/dialog-open/error)
    - widgets_visible: list of str (toolbar/connection_panel/chart_pane/...)
    - chart_contents: dict (lines_visible, trace_count, color, axis_labels_visible)
    - connections: list of {id, state}
    - status_bar_text: str
    - errors_visible: list of str
    - dialogs_open: list of str
    - menu_open: str | None
    """
    # Implementation per M15.2 decision: Claude API / local model / CC tool / etc.
    raise NotImplementedError("M15 S1 implementation per Phase 4 lock")


def compare_descriptions(actual: dict, expected: dict) -> list:
    """Return list of differences between two GUI descriptions."""
    # Common across implementation choice.
    ...
```

The prompt template (when LLM-based) should:
- Specify SignalForge as the application context
- Request structured JSON output  
- Include schema example to constrain output format
- Be deterministic (low temperature)
- Handle ambiguity gracefully (return null for uncertain fields, not hallucinate)

The integration point is the same regardless of M15.2 choice: a function
that takes an image path and returns a structured dict. The function may:
- Make an HTTP API call (Claude/OpenAI/Gemini API)
- Invoke a local model (LLaVA via Ollama, Qwen-VL via vLLM, etc.)
- Call a CC-native tool (if exists in CC's runtime)
- Pipe to a subprocess running the chosen mechanism

### 4.3 Visual test workflow

```python
# tests/visual/test_chart_renders_signal.py
def test_chart_renders_signal():
    # Setup: launch SF, connect UDP, drive frames
    sf = launch_signalforge_test()
    sf.add_connection(driver="udp", schema="modbus_style")
    sf.connect("conn-1")
    drive_udp_frames(rate=100, count=500)
    sleep(2)
    
    # Capture
    screenshot = sf.capture("post-frames.png")
    
    # Vision check
    description = describe_screenshot(screenshot)
    
    # Assertions
    assert description["chart_contents"]["lines_visible"] == True
    assert description["chart_contents"]["trace_count"] >= 1
    assert description["window_state"] == "connected"
    assert "error" not in description.get("indicators", [])
    
    # Baseline diff
    baseline = "tests/visual/baselines/chart_renders_signal.png"
    diff = compare_images(screenshot, baseline)
    assert diff.percent_changed < 5  # Tolerance for font etc.
```

### 4.4 Baseline approval workflow

When a test produces a new screenshot diff:

```
PR run output:
========
Visual test failed: chart_renders_signal
Baseline: tests/visual/baselines/chart_renders_signal.png
Actual:   tests/screenshots/chart_renders_signal/post-frames.png
Diff:     12.3% pixels changed

Vision LLM verdict (on actual):
  window_state: "connected"
  chart_contents: {lines_visible: true, trace_count: 1, color: "blue"}

Vision LLM verdict (on baseline):
  window_state: "connected"
  chart_contents: {lines_visible: true, trace_count: 1, color: "green"}  

Difference detected: chart line color changed from green to blue.

If intended, run:
  ./scripts/accept-baseline.sh chart_renders_signal
========
```

Human (operator) can:
1. Run `accept-baseline.sh` to update the baseline
2. Or fix the regression and re-run

After M15 stabilizes, CC could potentially be authorized to run
`accept-baseline.sh` itself (V0.2.1 or V0.3 evaluation).

## 5. Acceptance criteria

### 5.1 Screenshot capture infrastructure
- [ ] Mechanism C (in-process QPixmap snapshot) wired into MainWindow
  with --capture-screenshot CLI + IPC trigger
- [ ] Mechanism B (xvfb + xwd) wired into CI test harness
- [ ] M14 S1 smoke captures baseline screenshot post-fixture-load
- [ ] Mechanical-18 (T3 currently) extended to capture screenshot per test

### 5.2 Vision LLM integration
- [ ] `tests/visual/lib/describe_screenshot.py` (or equivalent) implemented
- [ ] Returns structured JSON description
- [ ] Used by at least 3 visual tests successfully
- [ ] Handles vision API errors gracefully

### 5.3 Baseline coverage
- [ ] Y-scope (state-machine complete) baselines captured for all major
  GUI states:
  - idle (no connections)
  - connecting / connected / disconnecting / error (per state)
  - 1 / 2 / N connections
  - recording active
  - replay loaded + playing + paused
  - file-open dialog
  - connection-edit dialog
  - quit-while-recording confirm dialog
  - live↔replay 3-option dialog
- [ ] Each baseline reviewed + approved by operator (one-time review)

### 5.4 Test framework
- [ ] Existing 18-test mechanical subset extended with screenshot capture
- [ ] M14 GUI subset (T4/T6/T9/T13-T18) automated with screenshot + vision
- [ ] M14 race-blocked tests (T7/T8/T11) revisited; either resolved or
  documented why visual approach can't help

### 5.5 CI integration
- [ ] Screenshots uploaded as GitHub Actions artifacts on every CI run
- [ ] Failed visual tests display baseline + actual + diff in PR
- [ ] `accept-baseline.sh` script for human approval workflow

### 5.6 CC autonomy demonstration
- [ ] CC can run `make visual-test` (or equivalent), inspect failures
  via vision-LLM, diagnose root cause, propose fix — all without operator
  GUI dogfood
- [ ] One end-to-end demonstration documented

### 5.7 Documentation
- [ ] `docs/v0.2-vision-infrastructure.md` — usage guide
- [ ] `tests/visual/README.md` — how to add new visual tests
- [ ] M15-done.md — hand-off to V0.3 with baseline coverage map

## 6. M15-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Vision LLM cannot reliably describe SignalForge GUI** — HALT;
   re-evaluate Claude API vs local model vs prompt engineering
2. **Screenshot capture mechanism unreliable** (false flakes > 5%) —
   HALT; investigate Qt rendering determinism
3. **Baseline approval workflow becomes operator burden** — HALT; revisit
   M15.5 decision (current U → maybe V or T tolerance)
4. **CC vision-driven self-test demonstrates accuracy issues** — HALT;
   investigate prompt design + retry strategies
5. **CI cost / time excessive** (>30 min added to PR runs) — HALT;
   move some checks to release-only gate
6. **Existing M14 frameworks resist extension** (per M15.4 Q decision) —
   HALT; reconsider P (separate suite) or hybrid

## 7. V0.3 hand-off

M15-done.md must include:

### Visual baseline coverage map
List of every GUI state covered, with screenshot path. This becomes the
"reference reality" for V0.3 redesign.

### V1 UX gap inventory
Reviewing the V0.2 baselines, document every UX issue visible:
- Chart line colors (current pastel — needs better contrast)
- Status bar information density (or lack thereof)
- Connection list visual hierarchy
- Dialog text readability
- Menu organization
- Signal selector ergonomics
- Multi-chart layout
- Replay toolbar discoverability
- Status indicators (Idle/Connecting/Connected — visual differentiation)
- Theme support gap (currently white-only)
- Window title bar information
- Keyboard shortcut coverage
- Tab order / focus ring visibility
- Error message clarity
- (etc.)

V0.3 design phase starts here.

### Industrial software references
While doing V0.2, opportunistically note industrial reference points:
- LabVIEW chart customization
- MATLAB Instrument Control Toolbox connection panel
- Tektronix scope GUI hierarchy
- Saleae Logic timeline
- (etc.)

These become V0.3 design study inputs.

### V0.3 spec writing scope
- M16 spec: Design tokens + theme system + visual language
- M17 spec: Core widget rebuild
- M18 spec: Workflow rebuild
- (Possibly M19+ if scope expands)

## 8. Closing

M15 is the missing piece V1 didn't have. Building it before V0.3 redesign
prevents repeating the F4-F19 pattern at higher cost.

Once CC can see the GUI, every UX iteration is grounded in visual reality.
V0.3 redesign becomes possible because feedback loops actually work.

Quality > schedule. M15 takes as long as it takes.
