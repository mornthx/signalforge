# M14 — V1.0 GUI Integration Audit

| Field | Value |
|---|---|
| Milestone ID | M14 |
| Sprint | 14 |
| Estimated effort | **Open-ended** (no hard cap; M14.4 U) |
| Prerequisites | M13 closed-as-escalation; 4 prior HALTs documented (run 1-4) |
| Next milestone | None (V1.0 release after M14, OR V1.0 scope re-evaluated) |
| Hard-stop type | **GUI end-to-end correctness** (CI release-binary smoke test passes + 18-test HW verification 16+/18 pass) + **V1.0 release readiness** (final architectural decision on what V1.0 actually ships) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M14` |

**Cross-reference notation**: same as prior milestones.

---

## 1. Goal

M14 is **V1.0 GUI Integration Audit**. Unlike all prior milestones (functional/optimization/packaging), M14 has **open-ended scope** by design:

1. **Build CI release-binary smoke test** that catches GUI integration bugs in CI (not operator sessions)
2. **Audit ALL V1 GUI integration paths** systematically
3. **Fix every bug found** (no time-box)
4. **Re-evaluate V1.0 release scope** based on audit findings

After M14 closes, V1.0 ships in one of three forms:

| Outcome | Scope | Trigger |
|---|---|---|
| **V1.0 full** | All M0-M12 features ship | Audit finds bugs, all fixable, 18-test 16+/18 pass |
| **V1.0 reduced** | Subset of M0-M12 features ship; rest deferred to V1.1/V1.5+ | Audit finds architectural issue in some feature; rest works |
| **V1.0 cancelled → V1.5 first** | V1.0 not shipped; V1.5+ becomes first published release | Audit finds GUI architecture fundamentally not ready |

The M14.5 X decision (Re-evaluate V1.0 scope) authorizes this flexibility.

Quality philosophy:
- **Reality > schedule**: ship what works, not what was planned
- **Audit > patch**: systematic verification > layer-by-layer bug fix
- **CI smoke > operator sessions**: catch GUI integration bugs in automated test, not human dogfood
- **Honest > optimistic**: if V1.0 is not viable, scope it down

---

## 2. Scope

### 2.1 Must deliver

1. **CI release-binary smoke test** at `tests/ci/test_release_binary_smoke.cpp` (or shell-script equivalent):
   - Builds release `signalforge` executable
   - Launches with `Q_QPA_PLATFORM=offscreen`
   - Drives UDP fixture frames into a connected schema-decoded pipeline
   - Reads chart pixel via `QPixmap` snapshot OR Qt scene-graph readback
   - Asserts pixel is **NOT** the QQuickWidget clear color
   - Greps stderr/stdout log for known error patterns ("ChartHost.qml failed to load", "rootObject is null", etc.)
   - **Two-tier CI verification** (decision M14.1 C):
     - Tier A: Pixel diff
     - Tier B: Log error grep
   - Wired into `.github/workflows/ci.yml` (or equivalent) as required check
   - Must pass on Debug + Release + debug-asan presets

2. **GUI integration audit** at `docs/m14-gui-audit-report.md`:
   - Systematic test of every V1 GUI feature
   - Each path: operator + CI smoke must both work
   - Documented results: ✓ working / ⚠ working with caveat / ✗ broken
   - Categorized by severity:
     - **Critical**: Blocks V1.0 ship (e.g., chart doesn't render)
     - **Serious**: Major UX impact, V1.0.1 patch acceptable
     - **Minor**: Cosmetic, V1.5+ acceptable

3. **Fix all Critical bugs found in S3 audit**:
   - One commit per bug (decision M14.3 P)
   - One ADR per architectural change (continues 008/009/010 pattern)
   - Each fix verified by CI smoke test passing

4. **V1.0 scope re-evaluation document** at `docs/v1.0-scope-evaluation.md`:
   - Audit findings summary
   - Bug count + severity
   - Architectural feasibility assessment
   - Three scenarios analyzed (V1.0 full / V1.0 reduced / V1.0 cancelled)
   - **Recommendation** with rationale

5. **18-test HW verification re-run**:
   - After Critical bugs fixed, operator runs full 18-test session
   - 16+/18 pass required
   - Documented results in `docs/m14-final-verification.md`

6. **GUI integration test framework** at `tests/integration/gui/`:
   - Reusable infrastructure for `Q_QPA_PLATFORM=offscreen` GUI tests
   - Helper utilities for: chart pixel snapshot, UDP fixture injection, log capture
   - This becomes V1.5+ governance asset (every GUI change runs against this)

7. **Run-4 chart sizing fix** (immediate sub-task within M14):
   - Already-identified bug: Chart QQuickItem sized 0×0
   - Fix: `itemChange(ItemParentHasChanged)` self-sizing OR QML registration
   - Verified by S1 CI smoke test

8. **`.claude/M14-done.md`** with:
   - Audit results summary
   - All bugs found + fix commits
   - V1.0 scope decision + rationale
   - Path forward (V1.0 ship or V1.5 first)

### 2.2 Must not do

1. **No new V1 functional features**. M14 is verification + fix only.
2. **No spec amendments to M0-M12 specs** (those are merged-to-main artefacts).
3. **No silent rework of frozen interfaces** without ADR.
4. **No "good enough" acceptance**. If audit finds Critical bugs, fix or scope-down.
5. **No skipping the CI smoke test** (decision M14.1 C is binding).
6. **No premature V1.0 ship declaration** before audit complete + scope re-evaluated.

---

## 3. Design Decisions (locked by this spec)

### 3.1 Two-tier CI verification (decision M14.1 C)

CI smoke test runs both:
- **Tier A**: Pixel diff verification — chart canvas pixel ≠ clear color
- **Tier B**: Log error grep — stderr/stdout free of known error patterns

Both tiers must pass. Tier A catches rendering failures (run 4); Tier B catches Qt resource / QML errors (runs 2-3).

### 3.2 Comprehensive audit breadth (decision M14.2 Y)

Audit covers all known + likely GUI integration paths:

```
Live mode chain:
- ConnectionDialog → save to YAML
- ConnectionManager → load from YAML  
- Connection state machine → driver attach
- PipelineManager.attach → DecoderRegistrar pipelineAttached signal  ← run 1-3 found bugs
- DecoderRegistrar → SchemaDecoder construction
- SchemaDecoder → SignalValueSink (TeeSink)
- TeeSink → SignalBufferRegistry + SessionWriter
- SignalBufferRegistry → ChartManager
- ChartManager → Chart QQuickItem rendering  ← run 4 found bug
- Chart redraw timer → scene-graph paint nodes
- QQuickItem → QQuickWidget host scene  ← run 2-3 found bugs
- QQuickWidget → user-visible pane

Recording chain:
- File → Record menu → SessionWriter.start
- SessionWriter → SessionFileWriter worker thread
- TeeSink delivery to writer
- Status bar bytes counter update  ← may have bugs
- File → Stop Record → SessionWriter.stop
- File integrity verification

Replay chain:
- File → Open Session menu → SessionReader
- SessionReader → SessionPlayer → PlaybackController
- PlaybackController → MainWindow replay UI mode switch
- Replay toolbar (Play/Pause/Step/Seek/Speed) all functional?
- ChartManager re-renders from replay data  ← may have bugs
- Status bar updates current position

Mode transitions:
- Live → Replay (M11 ReplayModeManager)
- Replay → Live  ← M11 logic
- Connection auto-disconnect on Replay enter
- Connection auto-reconnect on Replay exit (decision dialog)

Persistence:
- Quit app → connection list saved to YAML
- Restart → connections auto-loaded
- Auto-connect on startup (M9 feature) — actually works?

UI elements:
- Toolbar buttons all responsive?
- Menu items all functional?
- Dialog boxes work?
- Status bar updates?
- Multi-chart UI:
  - Add Chart button
  - Multiple charts coexist
  - Per-chart signal selection
  - Chart removal (currently missing UI!)
- Signal Selector:
  - Filter / search
  - Toggle on/off
  - Multi-signal toggle
- Settings dialog (if any)
```

For each path, audit:
- Does it work in operator GUI session?
- Does it work in CI smoke test?
- Does production binary behave same as test binary?
- Are there log errors during normal use?

### 3.3 Per-bug commit aggregation (decision M14.3 P)

Each bug = one commit. ADR if architectural change. Maintains audit trail.

Format:
```
fix: M14 S<n> — <short description of bug>

Symptom: <user-visible behavior>
Root cause: <technical explanation>
Fix: <what was changed>
ADR: ADR-NNN (if architectural)
Verified: CI smoke test pass + operator GUI test
```

### 3.4 Open-ended timeline (decision M14.4 U)

**No hard time cap**. M14 closes when:
- All Critical audit findings fixed
- CI smoke test passing
- 18-test HW verification 16+/18 pass
- V1.0 scope decision finalized

If audit reveals architectural issues that cannot be fixed in days, escalate to:
- V1.0 reduced scope (M14.5 X path)
- V1.0 cancelled, V1.5 first

### 3.5 V1.0 scope re-evaluation (decision M14.5 X)

After audit, **explicit decision** between three scenarios:

**Scenario A: V1.0 full ship**
- All M0-M12 features work in operator GUI
- 18-test 16+/18 pass
- CI smoke green
- → ship `v1.0.0` as originally planned

**Scenario B: V1.0 reduced ship**
- Some M0-M12 features broken architecturally
- Working subset is shipworthy
- Document deferred features in v1.0 release notes
- → ship `v1.0.0` with reduced feature set + clear V1.5+ migration plan

**Scenario C: V1.0 cancelled, V1.5 first**
- V1 GUI architecture fundamentally not ship-ready
- Multiple architectural issues require rewrites
- → cancel `v1.0.0` tag, reschedule as V1.5 with proper architecture audit upfront

The decision is **made post-audit**, not pre-determined. M14 spec authorizes the flexibility.

### 3.6 No soft-HALT (inherits M2-M13)

### 3.7 Frozen surface protection

Any frozen .hpp modification requires ADR. M14 expects multiple ADRs (one per architectural fix).

---

## 4. Key Implementation Details

### 4.1 CI smoke test infrastructure

**Approach 1: GTest C++ test launching subprocess**

```cpp
// tests/integration/gui/test_release_binary_smoke.cpp

#include <catch2/catch_test_macros.hpp>
#include <QProcess>
#include <QStandardPaths>
#include <QImage>

TEST_CASE("M14: release binary launches and renders chart", "[m14][gui][smoke]") {
    QString binaryPath = QStandardPaths::findExecutable("signalforge");
    REQUIRE(!binaryPath.isEmpty());
    
    QProcess app;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORM", "offscreen");
    env.insert("SF_TEST_MODE", "1");  // app may auto-load test fixture
    app.setProcessEnvironment(env);
    
    app.start(binaryPath, QStringList() << "--auto-load-test-fixture");
    REQUIRE(app.waitForStarted(5000));
    
    // Wait for app to settle (chart created + decoder attached)
    QThread::msleep(2000);
    
    // Drive UDP fixture (separate thread/process)
    sendUdpFixtureFrames("127.0.0.1", 9998);
    QThread::msleep(1000);
    
    // Capture chart pane via SignalForge IPC or grab via screenshot
    QImage chartImage = captureChartPane(app);
    
    // Tier A: pixel diff (any non-white pixel in chart area)
    bool hasNonClearPixel = false;
    for (int y = 0; y < chartImage.height(); ++y) {
        for (int x = 0; x < chartImage.width(); ++x) {
            if (chartImage.pixel(x, y) != qRgb(255, 255, 255)) {
                hasNonClearPixel = true;
                break;
            }
        }
        if (hasNonClearPixel) break;
    }
    REQUIRE(hasNonClearPixel);  // Chart shows something
    
    // Tier B: log error grep
    QByteArray stderr_log = app.readAllStandardError();
    REQUIRE_FALSE(stderr_log.contains("ChartHost.qml failed to load"));
    REQUIRE_FALSE(stderr_log.contains("rootObject() is null"));
    REQUIRE_FALSE(stderr_log.contains("decoder pipeline empty"));
    REQUIRE_FALSE(stderr_log.contains("setParentItem(nullptr)"));
    
    app.terminate();
    app.waitForFinished(2000);
}
```

**Approach 2: Shell script harness (simpler)**

```bash
#!/bin/bash
# tests/ci/release_binary_smoke.sh

set -e

# Launch with offscreen QPA
QT_QPA_PLATFORM=offscreen LD_LIBRARY_PATH=/path/to/qt6.10/lib \
    /opt/signalforge/bin/signalforge --auto-load-test-fixture &
APP_PID=$!

sleep 2  # let it init

# Drive fixture
python3 -c "
import socket, struct
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
for i in range(100):
    sock.sendto(b'\\x01\\x03' + struct.pack('>HHH', i, i*2, i*3), 
                ('127.0.0.1', 9998))
"

sleep 1

# Capture log
LOG_FILE=~/.local/state/signalforge/logs/signalforge.log

# Tier B: log grep
if grep -E "ChartHost.qml failed|rootObject.* is null" "$LOG_FILE"; then
    echo "FAIL: log error pattern found"
    kill -TERM $APP_PID 2>/dev/null || true
    exit 1
fi

# Tier A: chart pixel diff
# (requires app to expose IPC or screenshot capability)
# Simplest: app dumps chart canvas to PNG on signal
kill -USR1 $APP_PID  # tell app to dump chart
sleep 0.5

CHART_DUMP=/tmp/signalforge_chart_dump.png
if [ ! -f "$CHART_DUMP" ]; then
    echo "FAIL: chart dump file missing"
    kill -TERM $APP_PID
    exit 1
fi

# Use ImageMagick or Python to verify non-white pixels
HAS_NON_WHITE=$(python3 -c "
from PIL import Image
img = Image.open('$CHART_DUMP')
for x in range(img.width):
    for y in range(img.height):
        r, g, b = img.getpixel((x, y))[:3]
        if (r, g, b) != (255, 255, 255):
            print('YES')
            exit()
print('NO')
")

kill -TERM $APP_PID
wait $APP_PID 2>/dev/null

if [ "$HAS_NON_WHITE" = "YES" ]; then
    echo "PASS: chart rendered non-clear-color pixels"
    exit 0
else
    echo "FAIL: chart all white"
    exit 1
fi
```

CC will choose the approach that fits the existing CI workflow best. Recommendation: Approach 2 (shell) for portability.

### 4.2 Audit methodology

For each GUI path in §3.2:

1. **Operator test**: launches release `signalforge`, performs the action, observes result
2. **CI smoke test extension**: adds programmatic verification of same action
3. **Log inspection**: grep for known error patterns
4. **State verification**: check internal state (via debug log or IPC)

Document each path:
- Path description
- Expected behavior
- Operator-observed behavior
- CI smoke result
- Status: ✓ working / ⚠ caveat / ✗ broken
- If ✗: severity (Critical / Serious / Minor) + proposed fix

### 4.3 V1.0 scope evaluation rubric

After audit, document at `docs/v1.0-scope-evaluation.md`:

```markdown
# V1.0 Scope Evaluation (M14 outcome)

## Audit summary
- Total paths audited: N
- Working: X (paths)
- Working with caveat: Y
- Broken Critical: Z1
- Broken Serious: Z2
- Broken Minor: Z3

## Architectural assessment
- Are Critical bugs all fixable in M14?
- Do any require fundamental rewrite?
- Is V1.0 GUI architecture (QQuickWidget + QQuickItem hosting) viable?

## Scope decision

[Choose Scenario A / B / C with explicit rationale]

### Scenario A: V1.0 full ship
- Justification: ...
- Action plan: ...
- Risk: ...

### Scenario B: V1.0 reduced ship
- Features included: ...
- Features deferred: ...
- Migration path: ...

### Scenario C: V1.0 cancelled
- V1.5 architectural plan: ...
- Lessons learned: ...
```

---

## 5. Acceptance criteria

### 5.1 CI smoke test

- [ ] CI smoke test added to `tests/integration/gui/` or `tests/ci/`
- [ ] Test runs on Debug + Release + debug-asan presets
- [ ] Tier A (pixel diff) passes
- [ ] Tier B (log grep) passes
- [ ] Test added to required CI checks
- [ ] CI green on milestone/M14

### 5.2 GUI audit

- [ ] `docs/m14-gui-audit-report.md` published
- [ ] All paths in §3.2 audited
- [ ] All findings categorized by severity
- [ ] All Critical findings either fixed in M14 or documented as V1.0 cancellation cause

### 5.3 Bug fixes

- [ ] All Critical bugs fixed (one commit per bug, M14.3 P)
- [ ] ADRs written for architectural changes
- [ ] Each fix verified by CI smoke + operator GUI test

### 5.4 V1.0 scope decision

- [ ] `docs/v1.0-scope-evaluation.md` published
- [ ] Scenario A/B/C explicitly chosen with rationale
- [ ] If Scenario A: 18-test re-run results show 16+/18 pass
- [ ] If Scenario B: deferred features list + V1.5+ migration path
- [ ] If Scenario C: V1.5 architectural plan outline

### 5.5 Operator validation

- [ ] After all Critical fixes, operator runs 18-test HW verification
- [ ] Result: 16+/18 pass (Scenario A) OR documented inability per scope decision
- [ ] Documented at `docs/m14-final-verification.md`

### 5.6 Hand-off

- [ ] M14-done.md hand-off:
  - V1 governance lessons (combined ADR-008/009/010 pattern)
  - V1.0 ship plan (per scope decision)
  - V1.5+ architectural improvements roadmap
  - CI smoke test as permanent V1+ governance asset

---

## 6. M14-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Critical bug discovered without fixable path** → HALT + escalate to M14.5 X (Scenario B/C decision)
2. **CI smoke test cannot reliably catch a known bug** → HALT + redesign smoke test
3. **Audit reveals > 10 Critical bugs** → HALT + scope re-evaluation immediately (likely Scenario C)
4. **18-test HW verification < 12/18** (after fixes) → HALT + Scenario B/C decision
5. **Architectural issue requires modifying > 2 frozen .hpp files** → HALT + V1.0 scope re-evaluation
6. **Audit timeline exceeds 14 calendar days** → HALT + reconsider M14 scope (split into M14a/M14b?)

---

## 7. Closing note

M14 is V1 project's most honest milestone. Unlike M0-M13 which had bounded scope and target deliverables, M14 explicitly acknowledges that V1.0 may not be ship-ready and authorizes scope reduction or cancellation if needed.

The discipline:
- **No optimism**. Audit honestly.
- **No fast-path**. Find every bug.
- **No premature ship**. V1.0 ships only when it works.

V1 governance evolution:
- M0-M11: feature delivery
- M12: performance optimization
- M13: packaging + release prerequisites
- M14: **GUI integration audit + V1.0 reality check**

After M14, V1 has:
- Complete CI coverage (including release-binary GUI smoke)
- Fully audited GUI integration paths
- Honest scope decision for V1.0
- Foundation for V1.5+ proper architectural work

Quality discipline:
- **Reality > schedule**: ship what works
- **Audit > patch**: systematic > layer-by-layer
- **CI > operator**: catch in automation, not in user hands
- **Honest > optimistic**: V1.5 first is better than broken V1.0

When in doubt, do the audit. Find the bugs. Make the scope call. The V1.0 release tag is final and unmovable — getting it right matters more than getting it shipped.

---

## 8. Notes for CC

- **Open-ended timeline is feature, not bug**. Don't try to compress audit. Take time to find every bug.

- **CI smoke test priority**: build it FIRST (S1), before fixing run-4 sizing bug (S2). The smoke test is what prevents M15 from being needed.

- **Audit is operator-paired**. CC builds smoke test infrastructure + automated checks; operator runs GUI dogfood + reports findings. CC fixes; operator re-tests.

- **Bug discovery may exceed initial estimate**. If S3 audit reveals 20 bugs, fix them all (open-ended). If reveals 50 bugs, escalate to scope re-evaluation.

- **ADR-008/009/010 set the pattern**. Continue with ADR-011/012/... for each architectural fix.

- **V1.0 scope decision is collaborative**. CC presents audit findings; human + CC decide Scenario A/B/C together. Not CC unilateral.

- **CI smoke test is V1+ permanent asset**. Whatever pattern CC builds becomes the regression gate for V1.5+ and V2 GUI work. Build it well.

- **Don't optimize for V1.0 ship date**. Optimize for V1.0 actually working. The user wants a real product, not a tagged commit.
