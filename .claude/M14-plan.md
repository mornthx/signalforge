# M14 — Plan

Pairs with `.claude/M14-understanding.md`. Source of truth:
`docs/milestones/M14-gui-audit.md` at HEAD `ee4ef38`.
CLAUDE.md governs.

M14 is **open-ended** (M14.4 U). This plan sketches the
subtask sequence and HALT triggers; per-subtask LOC and
calendar estimates are nominal and may grow as the audit
surfaces more findings.

---

## 0. Methodology

- **Audit-first**: build the regression net (S1 CI smoke)
  before fixing the known run-4 bug (S2). Per spec §8: "build
  CI smoke test FIRST, before fixing run-4 sizing bug".
  Otherwise S2's fix has no permanent regression test.
- **One bug = one commit** (M14.3 P). Mandatory format from
  spec §3.3:
  ```
  fix: M14 S<n> — <short description of bug>

  Symptom: <user-visible>
  Root cause: <technical>
  Fix: <what changed>
  ADR: ADR-NNN (if architectural)
  Verified: CI smoke test pass + operator GUI test
  ```
- **One ADR per architectural change**, continuing
  ADR-008/009/010 numbering (next is ADR-011).
- **Pre-commit gate** for code-touching subtasks: Debug +
  Release + debug-asan build clean; ctest green; CI smoke
  passing on the local Release binary; clang-format clean on
  changed files.
- **Documentation-only commits** use the CLAUDE.md §Required
  #2 exception (S0 / S3 audit-report / S5 scope-eval / S7
  done.md qualify).
- **Operator-paired audit**: S3 is interleaved CC↔operator —
  CC builds smoke-test extensions and automated checks,
  operator runs GUI dogfood and reports findings. CC fixes;
  operator re-tests.
- **Frozen-surface budget**: spec HALT #5 fires at `> 2`
  frozen `.hpp` modifications. CC tracks the running count
  starting S2 in `M14-progress.md`.

## 1. Subtask sequence

| ID | Title | Net LOC est. | Output | Operator-blocking? |
|---|---|---:|---|---|
| S0 | M14-concerns.md (open questions §5 of understanding) | ~250 | docs | no |
| S1 | CI release-binary smoke test (Tier A pixel + Tier B log) + reusable `tests/integration/gui/` framework | ~600 | code + tests | no |
| S2 | Run-4 chart sizing fix (with ADR-011 if architectural) | ~150 | code | partial (operator confirms post-fix) |
| S3 | Comprehensive GUI audit + `docs/m14-gui-audit-report.md` | ~800 | docs + tests | yes (operator paired) |
| S4 | Fix all Critical bugs surfaced in S3 (per-bug commits, ADR-011+) | open-ended | code + ADRs | partial (operator confirms each) |
| S5 | V1.0 scope re-evaluation at `docs/v1.0-scope-evaluation.md` | ~400 | docs | yes (collaborative decision) |
| S6 | Operator 18-test HW verification re-run + `docs/m14-final-verification.md` | ~200 | docs | yes |
| S7 | M14-done.md + V1.0 ship/scope hand-off; PR for `milestone/M14` (and resolution of PR #24) | ~500 | docs + git | yes |

S1 + S2 are the regression-protection foundation. S3 is the
heart of the milestone (audit). S4 is unbounded (whatever S3
finds). S5 makes the V1.0 ship decision. S6 + S7 are
operator-driven closure.

## 2. Time budget (nominal)

Spec target: open-ended (M14.4 U). Initial estimate:
- S0: < 1 day
- S1: 1-2 days (smoke-test infrastructure is the most
  involved CC-only deliverable)
- S2: < 1 day (run-4 fix, well-scoped)
- S3: 2-3 days (operator-paired; depends on operator cadence)
- S4: 1-7 days (open-ended; depends on bug count + severity)
- S5: < 1 day (writes against S3 + S4 outcomes)
- S6: ~half a day (operator-driven; CC observes)
- S7: < 1 day

**Calendar minimum**: ~7 days (best case, audit finds only
run-4 + minor issues).
**Calendar likely**: 10-14 days (M14 HALT #6 trigger).
**Calendar pessimistic**: > 14 days (HALT → M14a/M14b split
or Scenario C).

## 3. HALT triggers (M14-specific, on top of CLAUDE.md §HALT)

| # | Trigger | Source | Action |
|---|---|---|---|
| H1 | Critical bug discovered without fixable path | S3 audit / S4 fix | HALT; escalate to M14.5 X (Scenario B/C decision) |
| H2 | CI smoke test cannot reliably catch a known prior-run bug class | S1 close + regression-protect verification | HALT; redesign smoke test |
| H3 | Audit reveals > 10 Critical bugs | S3 close | HALT; scope re-evaluation immediately (likely Scenario C) |
| H4 | 18-test HW verification < 12/18 (after fixes) | S6 | HALT; Scenario B/C decision |
| H5 | Architectural fix requires modifying > 2 frozen `.hpp` files | S4 | HALT; V1.0 scope re-evaluation |
| H6 | Audit timeline exceeds 14 calendar days | running counter from S0 start | HALT; reconsider M14 scope (split into M14a / M14b?) |

Plus CLAUDE.md standard set (compile error 3×, test fail 3×,
new dep, frozen-interface mod without ADR, perf miss after 1
opt pass, spec/arch contradiction, Qt 6.10 anomaly, two
plausible impls, unexplained git failure).

## 4. Subtask details

### S0 — Concerns

**Inputs**: spec §3, §6, §8 + understanding §5 + this plan §3.

**Output**: `.claude/M14-concerns.md` resolving the open
questions raised in understanding §5:
- C1: smoke-test approach choice (Approach 1 C++/QProcess vs
  Approach 2 shell+Python). Decide based on existing
  toolchain + portability + CLAUDE.md §1 dep-list compliance.
- C2: operator pairing cadence (daily ping-pong vs batch
  review).
- C3: PR #24 / `milestone/M14` PR strategy (single combined
  merge at end? Or replace #24 with #25?).
- C4: smoke test location (`tests/integration/gui/` vs
  `tests/ci/`).
- C5: scope-decision predetermination (CC must NOT pre-commit
  to Scenario A; concerns notes this).
- C6: frozen-surface counter — start at 0; track in
  M14-progress.md.

**Effort**: < 1 day; one docs-only commit
(`docs: M14 S0 — concerns C1-C6`).

### S1 — CI release-binary smoke test

**Inputs**: M14 spec §4.1 (Approach 1 + Approach 2 sketches);
S0 C1 decision; existing CI workflow in
`.github/workflows/ci.yml`; the four prior runs' bug classes
(no decoder, blank chart, missing qrc, 0×0 sizing).

**Output**:
- `tests/integration/gui/test_release_binary_smoke.cpp` (or
  `tests/ci/release_binary_smoke.sh` per S0 decision)
- `tests/integration/gui/CMakeLists.txt` + helper utilities
  (UDP fixture sender, log-grep helper, chart-pixel snapshot)
- `--auto-load-test-fixture` CLI flag in `signalforge` main
  (or `SF_TEST_MODE=1` env hook) that auto-loads a UDP-driven
  configuration for the smoke test
- `--dump-chart-png <path>` IPC hook (or `SIGUSR1` handler)
  for Tier A pixel capture on Approach 2
- `.github/workflows/ci.yml` extension to run the smoke as a
  required check on push + PR
- Tier A: pixel-diff assertion (chart canvas ≠ clear color).
  Lib: `QImage::pixel` comparison; threshold = "any
  non-white pixel in chart bounding box"
- Tier B: log error grep over `~/.local/state/signalforge/
  logs/signalforge.log` (or stderr capture). Patterns at
  minimum:
  - `ChartHost.qml failed to load`
  - `rootObject.* is null`
  - `decoder pipeline empty`
  - `setParentItem(nullptr)`

**Regression-protect verification (CC-internal, before
declaring S1 done)**:
1. Revert ADR-009 lambda → smoke fails Tier A (no decoder)
2. Revert ADR-010 `setSource` → smoke fails Tier A + Tier B
   (`ChartHost.qml failed to load`)
3. Revert ADR-010 `Q_INIT_RESOURCE(qml)` → smoke fails Tier A
   + Tier B (qrc not registered)
4. Stub Chart `setSize(0, 0)` → smoke fails Tier A (run-4 bug)

If all four reverts produce a smoke failure that pre-fix it
catches, S1 is done. Re-apply patches.

**Effort**: 1-2 days; one or two commits
(`build: M14 S1 — CI release-binary smoke test infrastructure`,
`build: M14 S1 — wire smoke test into CI workflow`).

### S2 — Run-4 chart sizing fix

**Inputs**: run-4 dogfood symptom (Chart 0×0); M8 chart
design; S1 smoke test as regression test.

**Diagnostic before fix**:
- Reproduce locally with `Q_QPA_PLATFORM=offscreen`
  signalforge — confirm Chart QQuickItem has `width()==0,
  height()==0` after `setParentItem(rootObject())`.
- Determine root cause: is `ChartHost.qml`'s
  `anchors.fill: parent` not propagating to the Chart child,
  or is the Chart not subscribed to its parent's geometry
  changes?

**Two candidate fixes** (HALT trigger #9: two plausible
impls):
- **Fix A**: `Chart::itemChange(ItemParentHasChanged)` in
  `chart.cpp` (NOT `.hpp` — frozen-surface friendly) binds
  Chart's `width`/`height` to the parent's via Qt property
  binding (or signal/slot on `widthChanged`/`heightChanged`).
- **Fix B**: register `Chart` as a QML type
  (`qmlRegisterType`) and instantiate via
  `ChartHost.qml`'s QML scene with `anchors.fill: parent`
  applied directly. Likely larger blast radius.

CC selects one + writes ADR-011 if the choice is
architectural. Both fixes verified by S1 smoke test passing
the pixel-diff Tier A. If neither produces an obvious fix,
HALT #9.

**Effort**: < 1 day; one commit
(`fix: M14 S2 — Chart self-binds to host geometry (run-4)`).

### S3 — Comprehensive GUI audit

**Inputs**: spec §3.2 path matrix; S1 smoke as automated
check; operator dogfood as human check.

**Output**: `docs/m14-gui-audit-report.md` per spec §4.2:
- For each path: description, expected behavior, observed
  behavior (operator + CI), status (✓ / ⚠ / ✗), severity
  (Critical / Serious / Minor), proposed fix per ✗
- Path matrix from spec §3.2:
  - Live-mode chain (12 sub-paths from
    ConnectionDialog → user-visible pane)
  - Recording chain (6 sub-paths)
  - Replay chain (6 sub-paths)
  - Mode transitions (4 sub-paths)
  - Persistence (3 sub-paths)
  - UI elements (12 sub-paths: toolbar, menu, dialogs,
    status, multi-chart, signal selector, settings)

**Methodology** (spec §4.2): operator + CI smoke must agree
per path; CC adds smoke-test extensions for paths beyond the
S1 baseline.

**HALT branches**: H3 (> 10 Critical) → immediate scope
re-eval; H1 (Critical without fixable path) → escalate to
M14.5 X.

**Effort**: 2-3 days CC + interleaved operator dogfood; one
commit per audit-report iteration
(`docs: M14 S3 — GUI audit report (initial)`,
`docs: M14 S3 — GUI audit report (operator pass 1)`,
… until matrix complete).

### S4 — Fix all Critical bugs

**Inputs**: S3 audit-report's Critical-severity findings.

**Output**: per-bug commits + ADR-011, ADR-012, … as needed.
Each fix verified by S1 smoke test passing both tiers.

**Iteration**: per bug,
1. Reproduce in S1 smoke (or extend smoke to cover this path)
2. Diagnose root cause
3. Decide fix scope (frozen-surface check; ADR if
   architectural)
4. Implement
5. Verify smoke green + operator confirms
6. Commit

**HALT branches**: H1 (Critical without fixable path), H5
(> 2 frozen .hpp).

**Effort**: open-ended; CC tracks running count in
M14-progress.md.

### S5 — V1.0 scope re-evaluation

**Inputs**: S3 audit summary + S4 fix tally + S6 18-test
results (in advance, S5 may iterate).

**Output**: `docs/v1.0-scope-evaluation.md` per spec §4.3:
- Audit summary (paths audited, ✓/⚠/✗ counts, severity
  breakdown)
- Architectural feasibility assessment (are Critical bugs all
  fixable in M14? Do any require fundamental rewrite?
  Is QQuickWidget+QQuickItem hosting viable?)
- Three-scenario analysis (A full / B reduced / C cancelled)
- **Recommendation** with rationale, written collaboratively
  with the human (spec §8 "V1.0 scope decision is
  collaborative; not CC unilateral")

CC drafts the document; human reviews + finalizes the
Scenario choice; CC commits the finalized version.

**Effort**: < 1 day CC drafting + collaborative review; one
commit (`docs: M14 S5 — V1.0 scope re-evaluation
(Scenario X)`).

### S6 — Operator 18-test HW re-run

**Inputs**: 18-test HW protocol from
`docs/m13-hardware-verification.md`; all S4 fixes shipped on
`milestone/M14`; .deb rebuilt with M14 fixes included.

**Output**: `docs/m14-final-verification.md` —
operator-driven results table (16+/18 required for Scenario
A). CC's role: prepare the .deb + docs; observe operator
session; record results.

**HALT branch**: H4 (< 12/18 → Scenario B/C decision).

**Effort**: ~half a day operator + CC observation; one commit
(`docs: M14 S6 — 18-test HW final verification (X/18 pass)`).

### S7 — M14-done.md + V1.0 ship/scope hand-off

**Inputs**: all prior subtask outcomes.

**Output**:
- `.claude/M14-done.md` per spec §5.6: V1 governance lessons
  (combined ADR-008/009/010 + M14 pattern), V1.0 ship plan
  per scope decision, V1.5+ architectural improvements
  roadmap, CI smoke test as permanent V1+ governance asset
- PR strategy resolution (per S0 C3): either
  `milestone/M14` opens its own PR that supersedes #24, or
  M14 commits get merged into `milestone/M13` first and PR
  #24 carries the combined V1.0 work
- Phase 2/3 hand-off announcement: "M14 ready. Awaiting
  approval to merge M14 [+M13] and ship V1.0.0
  [or scope-revised outcome]"

**Effort**: < 1 day; one or two commits + PR creation /
update.

## 5. Operator-blocking deliverables (split per understanding)

CC-blocking (M14 commits land these):
- S0 concerns
- S1 CI smoke + framework
- S2 run-4 fix
- S3 audit-report drafting (operator findings folded in)
- S4 bug fixes
- S5 scope-eval drafting

Operator-blocking (recorded in M14-progress.md, surfaced in
M14-done.md hand-off):
- S3 operator dogfood passes
- S4 operator confirmation per fix
- S5 collaborative scope decision finalization
- S6 18-test HW re-run
- S7 V1.0 ship gate (Phase 2 approval)

## 6. Branching + PR plan

- `milestone/M14` branched from `docs/m14-spec` HEAD
  `ee4ef38` (= `milestone/M13` `bf4c752` + the M14 spec
  commit). Already created locally.
- Future: per S0 C3, decide whether to:
  - **(a)** Open a fresh PR `milestone/M14 → main` that
    supersedes PR #24 (cleanest; PR #24 closed without
    merge)
  - **(b)** Open a sub-PR `milestone/M14 → milestone/M13`
    and let PR #24 carry the combined V1.0 work
  - **(c)** Cherry-pick S2-S7 commits from
    `milestone/M14` onto `milestone/M13` and close
    `milestone/M14`

CC plan §1 assumes path (a) until clarified at S0; this is
the cleanest and matches "Both milestones merge together as
the V1.0 release" by viewing `milestone/M14` as the V1.0
release branch.

## 7. Cross-references

- Spec: `docs/milestones/M14-gui-audit.md`
- Understanding: `.claude/M14-understanding.md`
- M13 closure: `.claude/M13-done.md` §"M13 not release-ready
  — escalating to M14"
- Run-1→Run-4 forensics: `docs/architecture/decisions/ADR-010`
  §Implementation lesson
- Open PR #24 — stays OPEN until S7 PR-strategy resolution
