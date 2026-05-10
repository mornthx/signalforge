# M14 Done — V0.1 Closure

| Field | Value |
|---|---|
| Status | **V0.1 functional floor reached** |
| Authority | `docs/V0-series-charter.md` |
| Closing PR | `milestone/M14` → `main` (this PR) |
| Tag (Phase 3) | `v0.1.0` (internal milestone; no GitHub Release publish per charter §5) |
| Successor | M15 (V0.2 — AI vision infrastructure) |
| Date | 2026-05-10 |

This document supersedes the earlier V1.0-ship M14 close framing.
The V0 charter (landed on `main` 2026-05-10 via PRs #25 + #26)
deferred V1.0 indefinitely; M14's implementation work closes
**V0.1 as the functional floor for V0.2+ work**, not as a V1.0
release gate.

The detailed status of what V0.1 delivers is at
`docs/v0.1-status-summary.md`. This document is the M14 milestone
hand-off + V0.2 unlock checklist.

---

## 1. Outcome

**M14 closes as V0.1 functional floor.** Per V0 charter §2.1
"V0.1 reached at M14 close. Backend production-grade; GUI
prototype-grade. Tag `v0.1.0` (internal milestone; no GitHub
Release publish). Audience: internal developer (operator) only.
No public ship promise."

V0.1 is the stable base for V0.2 (AI vision infrastructure)
and V0.3 (industrial UI/UX rebuild). V1.0 deferred indefinitely
per charter §1 + §2.5.

### Closure gates

- [x] M14 implementation pushed on `milestone/M14`
- [x] CI green on `milestone/M14`
- [x] Operator's run-6 final verification (run-7 not needed —
  V0.1 closure does NOT require the V1 18-test 16+/18 bar; the
  V1 hardware-verification gate per charter §1 was "necessary
  but not sufficient" and is no longer the V0.1 acceptance bar)
- [x] V0 charter governs (`docs/V0-series-charter.md`)
- [x] V0.1 status summary (`docs/v0.1-status-summary.md`)
- [ ] Human Phase 2 approval of this PR
- [ ] Phase 3 (CC autonomous): merge PR + tag `v0.1.0`

---

## 2. Subtask deliverables (S0 → S6)

| ID | Title | Status | Closing commits |
|---|---|---|---|
| S0 | Concerns C1-C6 + PR #24 closure | done | `8515137` |
| S1 | CI release-binary smoke test | done | `536ff91` (+ `cbf7a25` Path α closure) |
| S2 | Run-4 chart sizing fix (ADR-011) | done | `4038191` |
| S3 | GUI audit report + operator pass | done | `5077e0b` (+ skeleton `6a4a1a2`) |
| S4 Wave 1 | F4 Path α (smoke fixture canary) | done | `7ed9d7b` + `cbf7a25` |
| S4 Wave 2 | F6 + F17 wire-up (ADR-013) | done | `5c639fe` |
| S4 Wave 3 | F12 + F15 + F18 + F11 verify | done | `18bab31` + `555f270` + `6026395` + `a53d0ce` |
| S4 Wave 4 | F14 + F19 + F9 + mechanical-18 scaffold | done | `a29f581` + `a11430f` + `8c258f7` + `22b8eac` |
| S5 | Scope evaluation (now reframed as `docs/v0.1-status-summary.md`) | reframed | `b2bd6c4` (+ this commit) |
| S6 | Operator final verification (de facto run-6) | done | `8f66349` (run-6 report) |
| S7 | This M14-done.md + PR | in progress | (this commit) |

---

## 3. Audit findings tally

| Severity | Count | Findings |
|---|---:|---|
| ✓ Resolved in M13 | 1 | F1 (chart 0×0) |
| ✓ Resolved in M14 | 9 | F4, F6, F12, F14, F15, F17, F18, F19, F9 |
| ✓ Auto-resolved | 2 | F10 (with F6), F11 (with F15a) |
| **Critical** open | 0 | — |
| Serious open | 0 | — |
| V0.x backlog | 5 | F5, F7, F8, F13, F16 |
| Smoke-only deferral | 2 | F2, F3 |

Plus mechanical-18 expansion (T1, T2, T5, T7, T8, T10, T11) +
CI smoke extensions (persistence / recording / buffer-pressure)
deferred to V0.x patch maintenance.

---

## 4. ADRs landed in M14

| ADR | Subject | Wave |
|---|---|---|
| ADR-011 | Chart host-scene geometry binding | M14 S2 |
| ADR-013 | Recording catalog + connection persistence wire-up | M14 Wave 2 |

ADR-012 + ADR-014 reserved during the cycle but not used
(Wave 1 closed Path α with no production fix; Wave 3 F15 was
bookkeeping correctness, not architectural).

Combined ADR set after V0.1 close: 001 → 011 + 013.

**Frozen-surface counter (M14): 0 / 2.** HALT #5 (>2 frozen-`.hpp`
modifications) never triggered. M2-M12 frozen `.hpp` files
sha256-audit-clean across the M13/M14 release-prereq cycle.
Per V0 charter §3, the same freeze applies to V0.2 / V0.3.

---

## 5. V0 governance lessons (consolidated, M0 → M14)

V0 charter §1 articulates the root cause of the M13/M14
release-prereq cycle: **AI advisor + CC do not see the actual
GUI**. The decision loop (GUI → operator perception → verbal
report → CC inference → advisor decision) loses three layers of
visual information. M0–M14 built solid backend on the assumption
that "functional plumbing == ship-ready" but real users (even
single advanced developers) need visual + interaction quality
in addition to plumbing.

### 5.1 V0.1 lessons captured (from M14 work)

- **Per-test binary link topology ≠ release-binary link
  topology.** ADR-010 §Implementation lesson: every static-
  archive registration (`Q_INIT_RESOURCE`, `qmlRegisterType`,
  `qRegisterMetaType`) needs an explicit symbol reference.
- **Frozen-surface protocol works.** All M2–M12 frozen `.hpp`
  files unchanged across the M13/M14 cycle. ADR-008's additive
  `setSchemaForDriverType` is the correct frozen-surface escape
  hatch.
- **CI release-binary smoke is required.** M14 S1 catches
  scene-graph + qrc + chart-sizing regressions automatically.
- **Operator + CC daily ping-pong cadence finds bugs
  reactively.** V0.2 vision infrastructure replaces it with
  proactive screenshot-based perception.
- **Honest scope reduction beats brittle automation.** M14 S6
  mechanical-18 scaffold ships with 1 of 8 tests automated +
  the rest documented as V0.x candidates.

### 5.2 What V0.2+ inherits (V0 charter §1)

- **AI must see the GUI** to validate it. M15 builds the
  screenshot capture + vision LLM integration that closes
  the perception loop.
- **Quality > schedule.** V0 charter §8 rewords V1 spec §1
  ("Reality > schedule") and now governs without exception.

---

## 6. V0.1 → V0.2 hand-off

Phase 3 of this PR (CC autonomous after Phase 2 approval):

```bash
# Merge V0.1 close
gh pr merge <PR_NUMBER> --merge --delete-branch=false

# Tag (internal milestone; NO GitHub Release publish per
# charter §5; this distinguishes V0.x from a public V1.0
# release).
git tag -a v0.1.0 -m "V0.1 — functional floor (M14 close); V1.0 deferred per V0 charter"
git push origin v0.1.0

# Update local main
git checkout main && git pull origin main
git log --oneline -3
```

V0.2 (M15) bootstrap follows per CLAUDE.md milestone-closure
flow Phase 3 step 9 (e onward). M15 spec is at
`docs/milestones/M15-vision-infrastructure.md`. Awaiting human
"approved, merge V0.1 close and begin V0.2 (M15) bootstrap"
(or literal equivalent) to authorize Phase 3.

V0.x patch maintenance (F5/F7/F8/F13/F16 + smoke extensions +
mechanical-18 expansion + UdpDriver headless race fix) folds
into V0.2 / V0.3 work case-by-case per charter §3.

---

## 7. Cross-references

- V0 charter: `docs/V0-series-charter.md`
- V0.1 status summary: `docs/v0.1-status-summary.md`
- M14 spec: `docs/milestones/M14-gui-audit.md`
- M15 (V0.2) spec: `docs/milestones/M15-vision-infrastructure.md`
- M14 understanding/plan/concerns/progress: `.claude/M14-*.md`
- Audit report: `docs/m14-gui-audit-report.md`
- Operator runs: `docs/m14-audit-operator-runs/run1-…` through
  `run6-…`
- ADRs added in M14: ADR-011, ADR-013
- M13 escalation context: `.claude/M13-done.md` §"M13 not
  release-ready — escalating to M14"
- Frozen surface: `docs/v1.0-spec-list.md` (file retains V1
  name; disclaimer added at top — V0 charter §3 keeps the
  freeze in force)
- V0+ governance asset: `tests/integration/gui/README.md`
