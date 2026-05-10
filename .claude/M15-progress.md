# M15 — Progress

Live state for `milestone/M15` execution. Updated by CC after
each subtask close + on each operator interaction.

Source: `.claude/M15-understanding.md` + `.claude/M15-plan.md`
+ `.claude/M15-concerns.md`. Spec:
`docs/milestones/M15-vision-infrastructure.md`. Charter:
`docs/V0-series-charter.md`.

---

## Subtask state

| ID | Title | Status | Commits | Notes |
|---|---|---|---|---|
| S0 | Concerns C1-C7 + M15.2 local-only hybrid lock + empirical CC native test | done | `2fe5034` | Empirical Read-tool test PASSED on SignalForge PNG; M15.2 locked |
| S1 | Screenshot capture infrastructure (mechanism C in-process + B xvfb+xwd stub) | done | `d82d630` | `MainWindow::captureScreenshot` + `--capture-screenshot-*` CLI flags + `tests/visual/` Python harness + pixel-diff comparator + `scripts/accept-baseline.sh` + ctest `visual` label. Test 609 passes; M14 S1 + mechanical-18 still PASS |
| S2 | Vision-LLM integration | **done** | (this commit) | Schema + validator + CC-native prompt template + optional MiMo API wrapper (urllib stdlib; never CI) + 3 visual tests (empty, with-connection, chart-with-signal). 611/611 ctest |
| S3 | Baseline coverage (Y-scope; 38 baselines per C3) | not started | — | Operator-blocking (one-time approval) |
| S4 | Test framework integration: extend M14 S1 + mechanical-18 + visual-test suite | not started | — | Per C4 layout |
| S5 | CI integration (artifact upload + pixel-diff gate + accept-baseline.sh) | not started | — | Per C5 + C7: NO LLM in CI; pixel diff only |
| S6 | CC autonomy demonstration | not started | — | End-to-end self-test |
| S7 | M15-done.md + V0.3 hand-off | not started | — | Coverage map + V1 UX gap inventory + industrial refs |

---

## Phase 5 amendment carried forward (C7 reminder)

**Public-repo security: CI runs no vision LLM.**

- Zero `secrets.*` references in any
  `.github/workflows/*.yml` for vision-LLM purposes
- No API keys committed to the repo (`.env*` gitignored)
- Vision-LLM calls are local-only (CC's Read tool, optional
  MiMo via operator-local API key)
- CI gates: pixel-level baseline diff only (deterministic;
  no LLM verdict)
- **HALT trigger H7**: any accidental API key reference or
  vision-LLM call from CI → HALT immediately (security
  violation), revert + audit logs

CC self-checks every CI workflow change against this
constraint at S5.

---

## Quality > schedule discipline (V0 charter §4 + §8)

V0.2 has **no calendar commitment**. M15 closes when all 7
hard-stop criteria from `M15-understanding.md` §3 hold:

1. Screenshot capture works headless + locally
2. Vision-LLM returns reliable structured JSON; ≥ 3 visual
   tests use it
3. State-machine-complete baseline coverage captured;
   operator-approved one-time
4. M14 S1 + mechanical-18 extended with screenshots; GUI
   subset automated; M14-deferred items revisited
5. CI uploads screenshot artifacts; PR diff display +
   accept-baseline.sh workflow
6. CC autonomy end-to-end demonstration documented
7. M15-done.md published with V0.3 hand-off

Until all 7 hold, M15 stays open. No premature close.

---

## Frozen-surface modifications (C6 counter)

| Counter | Limit (HALT #5) | Status |
|---|---|---|
| **0 / 2** | > 2 → HALT | clean baseline |

Baseline reset at M15 S0 entry. Per V0 charter §3, M2-M12
frozen `.hpp` files (per `docs/v1.0-spec-list.md` §1) remain
frozen for V0.2 + V0.3. ADR + counter bump required for any
modification. `main_window.hpp` is **not** frozen (V1
integration point); M15 additions there are unrestricted.

| # | File | sha256 pre / post | Commit | ADR |
|---|---|---|---|---|

(empty — no M15 frozen-surface modifications yet)

---

## Empirical test artifact (S0 evidence)

`/tmp/m15-s0-empirical/chart.png` — 661×720 PNG (3 463 bytes)
captured from M14 S1 fixture. CC Read tool described
contents accurately:

- White background canvas (chart-pane framebuffer)
- Bright orange QSGSimpleRectNode ~60×60 at (10, 10)
  (the SF_F4_DIAG diagnostic rect from ADR-010)
- No chart line (xvfb + software-RHI rasterization
  limitation per ADR-010 Path α)

Confirms M15.2 lock: CC native multimodal Read tool is
sufficient for SignalForge GUI screenshot description in V0.2.

---

## HALT log

(empty)

---

## Cross-references

- Spec: `docs/milestones/M15-vision-infrastructure.md`
- Understanding: `.claude/M15-understanding.md`
- Plan: `.claude/M15-plan.md`
- Concerns: `.claude/M15-concerns.md`
- V0 charter: `docs/V0-series-charter.md`
- V0.1 status: `docs/v0.1-status-summary.md`
- M14 done: `.claude/M14-done.md`
- M14 mechanical-18 (extension target):
  `tests/integration/gui/run_mechanical_18.sh`
- M14 S1 smoke (extension target):
  `tests/integration/gui/release_binary_smoke.sh`
