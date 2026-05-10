# M15 — Understanding (V0.2 AI Vision Infrastructure)

Source of truth: `docs/milestones/M15-vision-infrastructure.md`
(499 lines, merged to `main` at `4c3cffe` via PR #26).
Charter: `docs/V0-series-charter.md` §2.2 (V0.2 phase definition;
quality > schedule per §4 + §8).

This is the **first V0 series milestone**. The V0 charter
deferred V1.0 indefinitely after M13/M14 caught 8 release
blockers; the root cause was that AI advisor + CC do not see
the GUI. M15 builds the perception loop V0.3 redesign needs.

---

## 1. Goal in one paragraph

Equip CC + advisor to **see** the SignalForge GUI directly,
not just inspect logs and record counts. M15 delivers
screenshot capture + vision-LLM integration + state-machine-
complete baseline coverage + test-framework integration + CI
infrastructure + a CC self-test demonstration. After M15
closes, CC drives V0.3+ UI design + implementation iterations
autonomously; the operator transitions from "primary GUI
tester" to "visual baseline approver"; future GUI bugs of the
F4–F19 class are caught in CI before reaching the operator.
M15 is **infrastructure investment**, not feature delivery —
no UI/UX fixes (V0.3's job). Quality > schedule per V0
charter §8.

## 2. What ships (per spec §2.1)

1. **Screenshot capture mechanism** integrated into GUI test
   runs; per-test PNGs saved to
   `tests/screenshots/<test-name>/<state>.png`; capture at
   minimum at post-launch, after each user action, and
   pre-test-end; `Q_QPA_PLATFORM=offscreen` + xvfb-run
   compatible; reproducible (deterministic test data, no
   real timestamps in screenshot region).

2. **Vision LLM integration** — CC can invoke vision
   capability on captured screenshots; returns structured
   JSON description (window state, widgets visible, chart
   contents, connections, status bar text, errors, dialogs,
   menus). Specific implementation per M15.2 lock at
   Phase 4 (currently deferred — see §5 below).

3. **Screenshot baseline coverage** (per spec §3 M15.3 W
   recommended) — every major GUI state has a baseline PNG
   in repo; diff comparison on each test run; visual
   regressions reported as test failures. Y-scope
   (state-machine complete; ~30–50 baselines) for V0.2; Z
   (pixel-level comprehensive) deferred to V0.3+.

4. **Test framework integration** (per spec §3 M15.4 Q
   recommended) — extend M14 S1 + mechanical-18 to capture
   screenshots; add a `tests/visual/` suite for tests that
   need visual judgement; GUI subset of 18-test
   (T4 / T6 / T9 / T13–T18) automated with screenshot +
   vision-LLM analysis; M14-deferred items (T7 / T8 / T11
   from headless `UdpDriver` race) revisited with the new
   infrastructure.

5. **CI infrastructure** — screenshot artifacts uploaded
   to GitHub Actions on every run; PR diff display
   (baseline + actual + diff); per spec §3 M15.5 U,
   visual-regression failure mode is hard-fail with manual
   override via an `accept-baseline.sh` script (or PR
   comment).

6. **Vision-driven self-test** — CC can run a GUI test,
   view its screenshot, validate the result, and propose
   fixes without operator intervention. One end-to-end
   demonstration documented in M15-done.md.

7. **Documentation** — `docs/v0.2-vision-infrastructure.md`
   (system + how to add visual tests),
   `tests/visual/README.md` (per-test how-to),
   V0.2 governance lessons consolidated.

8. **`.claude/M15-done.md`** with V0.3 hand-off:
   - Full GUI state baseline coverage map
   - V1 UX gap inventory (catalog of every UX issue
     visible in V0.2 baselines — input to V0.3 redesign)
   - Industrial software design references collected
     opportunistically (LabVIEW, MATLAB Instrument Control,
     Tektronix scope GUI, Saleae Logic, NI VeriStand,
     Yokogawa IS-Series — for M16+ design study)
   - Vision-LLM integration validated working

## 3. Hard-stop criteria (per spec §1 + §5)

M15 closes when **all** hold:

1. Screenshot capture works in headless CI + locally
   (mechanism C in-process + mechanism B xvfb+xwd hybrid
   per spec §3 M15.1 D recommendation).
2. Vision LLM returns structured JSON descriptions
   reliably; used by ≥ 3 visual tests successfully (per
   spec §5.2).
3. State-machine-complete baseline coverage captured;
   each baseline operator-approved (one-time review).
4. M14 mechanical-18 (T3 today) + S1 smoke extended with
   screenshot capture; GUI subset (T4 / T6 / T9 /
   T13–T18) automated; M14-deferred T7 / T8 / T11
   revisited.
5. CI uploads screenshot artifacts; PR diff display
   working; `accept-baseline.sh` workflow documented.
6. CC autonomy end-to-end demonstration documented.
7. `M15-done.md` published with V0.3 hand-off
   (coverage map + UX gap inventory + reference notes).

## 4. Hard constraints (spec §2.2 + V0 charter §3)

1. **No UI/UX fixes during M15.** M15 is infrastructure;
   V0.3 is fixes. UX issues found get documented for M16+,
   not patched here.
2. **No new functional features.** Backend frozen per V0
   charter §3 (M2-M12 frozen `.hpp` files unchanged).
3. **No operator-driven visual evaluation in CI.** Operator
   approves baseline once; CI runs vision-LLM
   autonomously thereafter.
4. **No frozen-surface modification** without ADR
   (continuing M0-M14 ADR-008/009/010/011/013 pattern).
   M15 should be entirely in `tests/visual/`,
   `tests/screenshots/`, `docs/`, and small additions to
   non-frozen surfaces (`main_window.cpp`, harness shell
   scripts).
5. **No M14 deferred non-headless items addressed**
   (F5 / F7 / F8 / F13 / F16 / V19 stutter polish) — those
   wait for V0.3.
6. **Quality > schedule.** No calendar commitment per V0
   charter §4; M15 takes as long as it takes.

## 5. Open questions (S0 concerns will resolve / surface)

These resolve at S0 (concerns + design decision lock for
M15.2). Spec recommendations carried forward unless S0
analysis disagrees.

1. **M15.1 — Capture mechanism** (recommended D: hybrid
   B + C). S0 confirms tests/visual/ test fixtures know
   which mechanism each test uses; defines per-test
   default.
2. **M15.2 — Vision LLM choice** (DEFERRED to S0 + Phase
   4). Survey 5 options:
   - **CC native image viewing** (Read tool supports PNG /
     JPEG per Read's docs in this runtime; works for
     development workflow)
   - **Claude API vision endpoint** (HTTP from test
     harness; works in CI; per-image cost; ~2–5 s
     latency)
   - **Local vision model** (LLaVA / Qwen-VL / MiniCPM-V;
     self-hosted; GPU or slow CPU; significant CI
     infrastructure overhead)
   - **Other LLM API** (OpenAI GPT-4V / Gemini; similar
     to Claude API but different vendor)
   - **Hybrid** (CC native for dev + Claude API for CI;
     local model dropped given CI overhead)
   S0 produces recommendation with rationale; Phase 4
   locks. Likely recommendation: **hybrid (CC native dev
   + Claude API CI)** — verifies CC native available;
   defers to human at Phase 4 if multi-vendor concerns
   arise.
3. **M15.3 — Baseline coverage** (recommended W: Y for
   V0.2, Z for V0.3+). S0 enumerates the Y-scope state
   list (~30–50 baselines).
4. **M15.4 — Test framework integration**
   (recommended Q: extend M14 S1 + mechanical-18). S0
   confirms how new visual tests live alongside existing
   ones.
5. **M15.5 — CI failure mode** (recommended U: hard fail
   with `accept-baseline.sh` manual override). S0
   confirms script location + invocation.
6. **Vision-LLM prompt engineering** — what's the
   minimum prompt that produces reliable structured
   output? S0 starts the iteration; S2 implements;
   reliability gate at S6.

## 6. M2-M12 freeze surface — verified intact at V0.2 entry

Per V0 charter §3 ("Backend frozen during V0 series"), M2-M12
frozen `.hpp` files remain frozen. V0.1 close at v0.1.0 tag
confirmed sha256 records in `docs/v1.0-spec-list.md` §1
unchanged. M15 should not touch any frozen `.hpp`; if needed,
ADR + M14-style frozen-surface counter starts at 0/2.

## 7. Quality philosophy (V0 charter §8 + M15 spec §8)

- **Reality > schedule.** V0 charter recodifies V1 spec §1.
  Quality is the only gate.
- **AI must see the GUI.** The decision loop (GUI →
  operator perception → verbal report → CC inference →
  advisor decision) loses too much. M15 closes that loop.
- **Infrastructure first; redesign second.** V1 redesigned
  without perception; V0.3 redesigns with M15's perception
  loop in place.
- **One-time operator approval; ongoing CC autonomy.**
  Visual baselines reviewed once by operator; CI then
  validates autonomously. Operator role transitions from
  "primary tester" to "approver".
- **Screenshots are the spec.** When V0.3 starts, the V0.2
  baselines are the visual ground truth from which redesign
  measures change.

## 8. Cross-references

- Spec: `docs/milestones/M15-vision-infrastructure.md`
- V0 charter: `docs/V0-series-charter.md`
- V0.1 status (predecessor): `docs/v0.1-status-summary.md`
- M14 done: `.claude/M14-done.md`
- Audit history: `docs/m14-audit-operator-runs/run1-…run6-…`
- M14 mechanical-18 framework (extension target):
  `tests/integration/gui/run_mechanical_18.sh` +
  `tests/integration/gui/README.md`
- M14 S1 smoke (extension target):
  `tests/integration/gui/release_binary_smoke.sh`
- ADR pattern: `docs/architecture/decisions/ADR-008…013`
- Frozen surface: `docs/v1.0-spec-list.md` §1 (V0.x freeze
  per charter §3)
