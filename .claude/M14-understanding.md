# M14 — Understanding

Source of truth: `docs/milestones/M14-gui-audit.md` (556
lines, authored on `docs/m14-spec`, currently at HEAD
`ee4ef38`). Architectural prereqs: all M0-M12 freezes + M13
release-prereq cycle (ADR-008 / ADR-009 / ADR-010), with PR
#24 still **OPEN** and `milestone/M13` HEAD `bf4c752` (note:
`docs/m14-spec` adds the M14 spec doc on top, and
`milestone/M14` branches from that combined HEAD).

M13 was escalated to M14 after four operator dogfood runs of
the 18-test HW verification protocol caught four distinct
layers of the same C++ → QML hand-off gap that no per-module
unit test exercised end-to-end. M14 is V1's first dedicated
**audit + verification** milestone; it explicitly authorizes
V1.0 scope reduction or cancellation if the audit reveals
that V1.0 is not ship-ready.

---

## 1. Goal in one paragraph

M14 builds a **CI release-binary GUI smoke test** that catches
chart-rendering, qrc-registration, and pipeline-attach
regressions in CI rather than in operator sessions; performs a
**comprehensive audit** of every V1 GUI integration path
(live, recording, replay, mode transitions, persistence,
multi-chart, signal selector); **fixes every Critical bug**
found (one commit per bug, ADR per architectural change in
the ADR-008/009/010 pattern); and produces an explicit **V1.0
scope re-evaluation** that picks one of three scenarios — V1.0
full ship, V1.0 reduced ship, or V1.0 cancelled (V1.5 first).
Closure unblocks PR #24 (or replaces it) and finalizes whether
`v1.0.0` ships at all. M14 is **open-ended**: no time-cap; the
milestone ends when the audit is honest, the bugs are fixed
or scoped down, and the CI smoke test reliably catches all
four prior runs' bug classes.

## 2. What ships (per spec §2.1)

1. **CI release-binary smoke test** (must) at
   `tests/integration/gui/` or `tests/ci/`:
   - Headless `Q_QPA_PLATFORM=offscreen` launch of the release
     `signalforge` binary
   - UDP fixture frames driven into a connected pipeline
   - **Tier A** (M14.1 C): chart pixel ≠ clear-color
     assertion via `QImage` snapshot or scene-graph readback
   - **Tier B** (M14.1 C): log error grep for
     `ChartHost.qml failed to load`,
     `rootObject() is null`,
     `decoder pipeline empty`,
     `setParentItem(nullptr)` etc.
   - Both tiers required; passes on Debug + Release +
     debug-asan; wired as required CI check
   - Approach choice (Approach 1 C++/QProcess vs Approach 2
     shell+Python) deferred to S1 implementation; CLAUDE.md
     §1 forbids new deps so any new tool must be already
     installed (Qt 6, Catch2, Python 3 system) — no new
     external runtime libraries
2. **GUI integration test framework** at
   `tests/integration/gui/` — reusable Qt-offscreen helpers
   for chart pixel snapshot, UDP fixture injection, log
   capture. V1.5+ governance asset
3. **Run-4 chart sizing fix** (S2): Chart QQuickItem currently
   sized 0×0 — fix via `itemChange(ItemParentHasChanged)`
   self-binding, or QML registration. Verified by S1 smoke
   test
4. **Comprehensive GUI audit** at
   `docs/m14-gui-audit-report.md` — per-path verdicts (✓ /
   ⚠ / ✗), severity (Critical / Serious / Minor), proposed
   fix per ✗
5. **All Critical bug fixes** — one commit per bug, ADR per
   architectural change, each verified by CI smoke
6. **V1.0 scope re-evaluation** at
   `docs/v1.0-scope-evaluation.md` — Scenario A/B/C explicitly
   chosen with rationale (audit summary, architectural
   feasibility, deferred-features list if Scenario B, V1.5
   plan if Scenario C)
7. **18-test HW verification re-run** results at
   `docs/m14-final-verification.md` — operator-driven; 16+/18
   required for Scenario A
8. **`.claude/M14-done.md`** with combined V1 governance
   lessons (ADR-008/009/010 pattern), ship/scope path forward,
   V1.5+ architectural roadmap, CI smoke as permanent V1+
   asset

## 3. Hard constraints (spec §2.2 + CLAUDE.md)

1. **No new V1 functional features**. M14 is verification +
   fix only.
2. **No spec amendments to M0-M12 specs** — those are
   merged-to-`main` artifacts.
3. **No silent rework of frozen interfaces**. Any frozen
   `.hpp` modification requires an ADR (continuing the
   ADR-008/009/010 pattern). Spec HALT trigger #5 fires if
   `> 2` frozen `.hpp` files need modification → V1.0 scope
   re-evaluation.
4. **No "good enough"** acceptance — Critical bugs are fixed
   or scope-reduced; never ignored.
5. **CI smoke test is non-negotiable** (M14.1 C). It must
   exist and reliably catch all four prior runs' bug classes
   before audit S3 begins.
6. **No premature V1.0 ship** before audit completion + scope
   re-evaluation.
7. **No new dependencies** (CLAUDE.md §Forbidden #1). Smoke
   test uses existing toolchain (Qt 6.10, Catch2, system
   Python 3, QProcess, QImage).
8. **No `git push --force`** to any branch. PR #24 (M13)
   stays OPEN until M14's scope decision is made.

## 4. Hard-stop criteria (spec §1 / §5)

M14 closes when **all** hold:

1. CI release-binary smoke test passing on Debug + Release +
   debug-asan; wired as required CI check
2. CI smoke test demonstrably catches all four prior runs'
   bug classes (regression-protect verification)
3. All paths in spec §3.2 audited and categorized
4. All Critical bugs fixed (one commit per bug, ADR per
   architectural change)
5. V1.0 scope decision finalized (Scenario A / B / C in
   `docs/v1.0-scope-evaluation.md`)
6. Scenario A only: 18-test HW verification 16+/18 pass
7. `M14-done.md` published with hand-off

## 5. Open questions / known risks (post-spec-read)

These are surfaced now and confirmed/closed during S0
concerns. Not blockers for Phase 4 approval; flagged so the
human sees them upfront.

1. **Smoke-test approach** (C++ QProcess vs shell+Python).
   Spec leaves this to CC; recommendation: Approach 2
   (shell+Python) for portability and because the Python step
   needs PIL/Pillow which is a CLAUDE.md §1 dependency check.
   Deferred to S1.
2. **Scenario A vs B vs C — likely outcome**. Cannot be
   predicted before audit. Run-4 alone (chart sizing) is
   plausibly Scenario-A fixable, but the audit may surface
   more architectural issues (e.g., dock-floating, theme,
   multi-chart interactions). M14.5 X authorizes the
   flexibility; CC must not pre-commit to Scenario A.
3. **Operator pairing cadence** — S3 audit needs operator
   dogfood interleaved with CC's CI work. Concrete cadence
   (daily ping-pong vs batch-review) not specified by spec.
   Deferred to S0 concerns.
4. **PR #24 status during M14** — stays OPEN per spec §1.
   Question for S0: should M14 commits land on
   `milestone/M14` and merge into `milestone/M13` as a
   sub-PR, or should M14 be its own PR that supersedes #24?
   Per spec §1 "Both milestones merge together as the V1.0
   release" — implies single combined merge at end. CC plan
   §1 will assume separate PR until clarified.
5. **CI smoke test maintenance** — V1.5+ governance asset.
   Question: does the smoke test live in `tests/integration/`
   (run by ctest) or `tests/ci/` (run only in CI)? Spec uses
   both names interchangeably; CC plan will pick
   `tests/integration/gui/` for ctest discovery (matches
   M13's `tests/integration/test_v1_live_mode_pipeline.cpp`
   pattern).
6. **Frozen-surface count budget** — spec HALT #5 fires at
   `> 2` frozen `.hpp` modifications. Run-4 chart sizing fix
   probably touches `chart.cpp` only (not `.hpp`). But the
   audit may find more. CC will track running count starting
   S2.

## 6. M2-M13 freeze surface — verified intact at M14 entry

M13 closure (PR #24 still OPEN) confirmed at S6 freeze-record
collection that all 26 M2-M12 frozen `.hpp` sha256s match
prior `*-done.md` records. ADR-008 added an additive method
to M5-frozen `decoder_registrar.hpp`; ADR-010 added a single
`Q_INIT_RESOURCE(qml)` line to `main.cpp` (not frozen). All
other M13 release-prereq commits stayed in non-frozen files
(`main_window.cpp`, `connection_manager.cpp`, qrc resources).
M14 inherits this surface and continues the additive
discipline.

## 7. Quality philosophy (spec §1, §7, §8)

- **Reality > schedule** — ship what works, not what was
  planned. M14.5 X authorizes scope reduction.
- **Audit > patch** — systematic verification before
  layer-by-layer bug fix. M13 ran out of patches; M14 starts
  with audit framework.
- **CI smoke > operator sessions** — catch GUI integration
  bugs in automated test, not human dogfood. Smoke test is
  built FIRST (S1) so subsequent fixes can be regression-
  protected.
- **Honest > optimistic** — V1.5 first beats broken V1.0. The
  V1.0 release tag is final; getting it right matters more
  than getting it shipped.

## 8. Cross-references

- Spec: `docs/milestones/M14-gui-audit.md`
- M13 closure: `.claude/M13-done.md` §"M13 not release-ready
  — escalating to M14"
- ADRs: `docs/architecture/decisions/ADR-008-…` (registrar
  runtime schema), ADR-009 (MainWindow plumbing), ADR-010
  (chart QQuickWidget host scene + Q_INIT_RESOURCE)
- Run-1→Run-4 forensic chain documented in
  `docs/architecture/decisions/ADR-010` §Implementation
  lesson
- Open PR #24 — stays OPEN; not merged until M14 scope
  decision
