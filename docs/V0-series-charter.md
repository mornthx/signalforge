# SignalForge V0 Series — Charter

| Field | Value |
|---|---|
| Status | Active |
| Authored | 2026-05-10 |
| Supersedes | M13/M14 V1.0 ship plan (V1.0 deferred indefinitely) |
| Successor | V1.0 (when ready, no calendar commitment) |

---

## 1. Why V0 (root cause)

The M0-M14 development cycle (~8 months) was structured for a V1.0 release.
M13 + M14 release-prerequisite cycles caught **8 V1.0 release blockers**:

1. Bench fixture leak (M13 S4 30-min soak)
2. DEB Qt 6.10 dependency manifest (M13 Gate 4)
3. DecoderRegistrar empty schema map (M13 ADR-008)
4. PipelineManager.attach never called (M13 ADR-009)
5. Chart QQuickWidget hosting + sizing (M13/M14 ADR-010/011, four sub-layers)
6. SessionWriter subscriber order (M14 F6, ADR-013)
7. Connection persistence not invoked (M14 F17, ADR-013)
8. SignalBufferRegistry idempotent re-registration (M14 F15)
9. PlaybackController setSpeed pending tick (M14 F19)

All caught **after** AI advisor + operator + CI declared the relevant feature
"functional". Operator dogfood (M13 18-test HW verification) was V1's first
real GUI exercise; M14 audit revealed F5-F19 in 60 minutes.

**Root cause**: AI advisor (Claude) and CC do not see the actual GUI. Decision
loop is:

```
GUI reality
   ↓ (visual signal)
Operator perception
   ↓ (verbal report, often binary pass/fail)
CC inference
   ↓ (log + record count + ctest)
Advisor decision
```

Three layers of information loss. Visual UX issues invisible to this loop:
chart line color, axis label legibility, dialog text clarity, timing rhythm
of replay, status-bar update smoothness, menu organization, etc.

**The V1 hardware target ("single advanced developer") spec was incompatible
with V1's actual UX**. M0-M14 built solid backend on the assumption that
"functional plumbing == ship-ready" — but real users (even single advanced
developers) need visual + interaction quality, not just plumbing.

**Conclusion**: V1.0 is not ship-ready. Continuing to patch toward V1.0
without fixing the perception loop will repeat the same pattern. V0 series
fixes the loop first.

## 2. V0 Series Phases

### V0.1 — Functional Floor (CURRENT)

**Status**: Reached at M14 close.
**Scope**: M0-M14 backend + GUI as currently exists.
**Quality**: Backend production-grade; GUI prototype-grade.
**Tag**: `v0.1.0` (internal milestone tag; no GitHub Release publish).
**Audience**: Internal developer (operator) only. No public ship promise.

What V0.1 delivers:
- Working live-mode pipeline (M2-M9)
- Working recording (M10) + replay (M11)
- Working buffer + decoder + chart-data-layer (M2-M8, M12)
- Sound packaging infrastructure (M13 DEB)
- 9 of 14 M14 audit findings resolved (F1, F4, F6, F10, F11, F12, F15, F17, F18)
- 8 V1.0 release blockers caught + closed
- 11 ADRs documenting governance trail
- Frozen-surface 0 violations
- M14 S1 release-binary smoke test (CI permanent asset)

What V0.1 does NOT deliver:
- Polished UI/UX
- Visual baseline coverage
- AI-perceivable GUI state
- Industrial-design aesthetics
- Comprehensive auto-test of GUI paths (only T3 mechanical-18 + S1 smoke)
- F5/F7/F8/F9/F13/F14/F16 + V19 stutter polish (some addressed, some open)

Acceptance: V0.1 is "stable functional floor for V0.2+ work". Not "ship-ready".

### V0.2 — AI Vision Infrastructure

**Goal**: Equip CC + advisor to see the GUI directly. Make every visual
quality dimension AI-perceivable in CI.

**Premise**: V0.3 UI/UX rebuild requires visual ground truth. Building UI
without visual perception repeats the V1 mistake at higher risk.

**Scope** (decisions M15.1-5 in V0.2 spec):
- Screenshot capture mechanism per GUI test
- Vision LLM integration (CC views screenshots, describes them, validates)
- Screenshot baseline coverage of every GUI state
- Test framework integration (extend mechanical-18 → visual-18 + more)
- CI infrastructure for screenshot artifacts

**Deliverable end state**:
- 100% of ctest GUI tests capture + archive screenshots
- 100% of 18-test items have functional + visual automation
- Every major GUI state (idle / connecting / connected / recording / replay /
  N-connection / error / dialog-open) has a baseline screenshot
- CC can be asked: "look at this screenshot, validate the chart line is
  visible at expected position with expected color" and answer correctly
- Operator role transitions from "primary tester" to "visual baseline
  approver"

**Success criteria**: V0.3 design work can begin without operator visual
dogfood for every iteration. CC drives V0.3 development; operator approves
visual baselines + final iterations.

**Tag**: `v0.2.0` (internal milestone; no GitHub Release).
**Audience**: Internal; V0.3 development unlocked.

### V0.3 — Industrial UI/UX Rebuild

**Goal**: Redesign SignalForge's UI/UX from the ground up, informed by
industrial software references and V0.2 visual perception infrastructure.

**Premise**: V1 UI was "make it work". V0.3 is "make it work AND make it
feel like a real instrument-grade tool".

**Methodology** (per decision M15.4):
1. CC studies industrial software references (LabVIEW, MATLAB Instrument
   Control Toolbox, Tektronix oscilloscope software, Saleae Logic, Yokogawa
   IS-Series, NI VeriStand)
2. CC analyzes each reference's design choices: WHY this layout, WHY this
   color, WHY this interaction pattern
3. CC proposes SignalForge equivalent: which features SF should adopt,
   which to reject, which to invent for SF's domain
4. CC produces V0.3 UI design draft (mockups + interaction specs)
5. Operator reviews + iterates with CC
6. Implementation in M16-M18 sub-milestones (or however many needed)

**Probable sub-milestone breakdown** (subject to V0.3 spec):
- M16: Design tokens + theme system + visual language
- M17: Core widget rebuild (connection panel, chart, signal selector,
  status bar)
- M18: Workflow rebuild (record / replay / mode transitions / dialogs /
  multi-window / dock layout)

**Success criteria**:
- All UI elements have intentional design rationale
- Dark mode + light mode both polished
- Industrial-grade aesthetic (operator can use full work-day without
  visual fatigue)
- All V1 UX gaps closed (F5/F7/F8/F9/F13/F14/F16 + V0.2 audit findings)
- CC + operator agree V0.3 is production-quality

**Tag**: `v0.3.0` (internal milestone; possible alpha-release for select
external testers if quality reached).

### V0.4+ (TBD)

If V0.3 not yet production-quality, iterate as V0.4, V0.5, etc. No calendar
commitment; quality is the only gate.

### V1.0 — Production Release

**Trigger**: Operator + CC + (possibly) external alpha testers agree that
V0.x has reached production quality.

**V1.0 ship gate definition**: Deferred to V0.3 close. The original V1
hardware-verification gate (16+/18 of 18-test) is necessary but not
sufficient. Final gate framing TBD.

**Tag**: `v1.0.0` (final, immutable, GitHub Release publish).

## 3. Backend Frozen During V0 Series

To protect V0.x development from regressing solid backend work:

- M2-M12 frozen .hpp surfaces remain frozen as in V1 spec
- All V0.2 / V0.3 work in: src/app/, src/chart/ (.cpp only), tests/,
  resources/, docs/
- Backend bugs discovered during V0.2/V0.3 → V0.x patch (case-by-case
  review)
- ADR pattern (008/009/010/011/013) continues for any frozen-surface
  modification

## 4. Spec Writing Discipline (V0 Series Adaptation)

Each V0.x milestone gets a spec following M0-M14 structure. Adaptations:

- **No fixed effort estimates** (per decision: quality-first, no calendar)
- **Hard-stop criteria emphasizes quality, not schedule**
- **HALT triggers include "AI cannot perceive intended visual outcome"**
  (new V0.2+ HALT)
- **Operator's role explicitly defined per milestone** (decreasing as
  V0.2 vision infrastructure matures)

## 5. Naming + Tagging

| Tag | Status | Publish? |
|---|---|---|
| v0.0.13-alpha.1 | Already on origin (M12 close) | No (already done) |
| v0.1.0 | M14 close (now) | No (internal) |
| v0.1.x | V0.1 patch maintenance branch | No |
| v0.2.0 | V0.2 close | No |
| v0.3.0 | V0.3 close | No (possibly alpha to externals) |
| v0.4.0+ | V0.x iteration if needed | No |
| v1.0.0 | Production ship | Yes (GitHub Release publish) |

## 6. Decision Authorization

V0 series decisions follow M0-M14 Phase 4 protocol:
- Spec changes: human authorizes
- Implementation: CC executes per spec
- Phase 4 review: human approves understanding + plan before Phase 5
- Phase 5 implementation: CC executes
- Phase 1 closure: PR + merge approval required

V0.x → V0.(x+1) transition: human + CC review V0.x close report; agree
to proceed.

V1.0 readiness assessment: collaborative human + CC + (optionally) external
review.

## 7. Governance Trail Preservation

V0 series inherits M0-M14 governance trail:
- 11 ADRs (001-011 + 013) preserved
- M0-M14 spec docs preserved on main
- All HALT reports preserved on milestone branches
- run-1 → run-7 forensic audit reports preserved

V0.x adds:
- V0.2 spec (M15)
- V0.3 spec (M16-M18 multi-spec)
- V0.x sub-milestone done.md files
- Visual baseline screenshots (V0.2+)
- Industrial reference design notes (V0.3)

## 8. Closing principle

> "Reality > schedule"
> — V1 spec §1, internalized properly this time

V0 series exists because V1 schedule pressure overrode reality assessment.
V0 corrects this. V1.0 ships when reality says it's ready, not when calendar
says it should be.
