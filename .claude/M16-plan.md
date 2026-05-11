# M16 — Plan (V0.3 Visual Identity Ownership)

Pairs with `.claude/M16-understanding.md`. Source of truth:
`docs/milestones/M16-visual-identity-ownership.md` v2;
`docs/V0-charter-amendment-v0.3.md`.

M16 has **no calendar commitment** per V0 charter §4 + §8.
This plan sketches the 10-subtask sequence + HALT triggers +
per-subtask deliverables; nominal durations may grow as
research / spike / token surfaces edge cases.

## 1. Methodology

- **Visual identity ownership is V0.3 keystone.** M17–M20+
  blocked until M16 closes (V0.3 charter §3). Without
  cross-environment determinism, V0.3 redesign pixel-diff is
  meaningless beyond exact-CI reproduction (R9 carried forward).
- **Spike before design** (R13 new). S0.5 minimal determinism
  spike between S0 and S1 — prove Fusion + bundled fonts +
  minimal palette reduces 2 baseline diffs to < 3 % cross-env
  BEFORE committing to elaborate manifesto / token / generator
  scaffolding. Spike fails → H11 → redesign technical stack
  before S1 manifesto investment.
- **Manifesto first, tokens second** (R11 carried forward + first
  applied at M16). S1 produces `docs/v0.3/visual-identity.md`
  BEFORE S2 fixes specific token values. Every token at S2 traces
  to a manifesto principle. H4 fires on contradiction.
- **Environment contract before measurement** (R14 new). S1
  also produces `docs/v0.3/rendering-environment-lock.md` +
  `docs/v0.3/visual-diff-contract.md`. S3 implements the
  contracts; S5 wires capture-time env dump. H10 rejects env-
  drift as INVALID (not soft-fail).
- **Single-source design assets** (R15 new). S2 establishes
  `resources/styles/tokens.json` canonical source; generator
  produces QSS / C++ / Python consumers; CI `--check` enforces
  freshness. H12 on generator drift.
- **Industrial reference traceability** (R10 carried forward).
  S0 collects references; S1 manifesto cites them; S6 baseline
  review re-cites them. ≥ 70 % gate; H5 below.
- **Production-fidelity acceptance bar** (R7 carried forward).
  All M16 captures via production code paths. Test-only state
  mutation produces (d) fixture-mock — not measurement-grade.
- **Per-baseline review authority** (R8 carried forward).
  Operator deliberately accepts each of 12 new M16 baselines
  at S6 + S7. No bulk-overwrite.
- **Per-subtask deliverable + commit** (continuing V0.2 pattern).
  Each subtask closes with a single logical commit + push + CI
  green before the next.
- **Pre-commit gate** for any code-touching subtask: Debug +
  Release + debug-asan build clean; ctest 612+ green (the +N
  from new visual tests + token-freshness check); existing M14
  S1 + mechanical-18 + V0.2 visual suite still PASS.
- **Documentation-only commits** use the CLAUDE.md §Required #2
  exception. S0, S1, S8 are docs-heavy; intermediate
  manifesto / contract iteration commits in this category.
- **Frozen-surface budget**: 0 / 2 at M16 entry (V0.2 close).
  HALT #5 (H2) fires at > 2 modifications. CC tracks in
  `M16-progress.md` once S0 commit lands.
- **CI vision-LLM compliance carried forward**: Phase 5
  amendment / C7. Zero `secrets.*` references for vision-LLM
  purposes; `describe.py` returns `None` under CI env;
  pixel-diff is the only CI gate. M16 does not change this.

## 2. Subtask sequence

Per spec §7 (10 subtasks with S0.5 spike inserted at R13):

| ID | Title | Output | Operator-blocking? |
|---|---|---|---|
| S0 | Concerns C1–C9 + reference research | `M16-concerns.md`, `docs/v0.3/references/` folder populated, reference inventory | Phase 4 (direction) |
| S0.5 | Minimal determinism technical spike (R13) | Captures 2 V0.2 baselines local + CI under prototype stack; visual-diff measurement; PASS (< 3 %) or HALT (H11) | yes (proceed / HALT decision) |
| S1 | Visual identity manifesto + algorithm + env contracts | `docs/v0.3/visual-identity.md`, `docs/v0.3/visual-diff-contract.md`, `docs/v0.3/rendering-environment-lock.md` | Phase 4 (manifesto approval) |
| S2 | Token source + generator | `tokens.json` + `tokens.schema.json` + `generate_style_assets.py` + 3 generated files + CI `--check` | partial (token review) |
| S3 | Visual-diff algorithm implementation + QSS linter | `compare.py` extended with contract + env sidecar pre-check; QSS selector linter | no |
| S4 | Qt rendering ownership + bundled fonts | `app_style.{hpp,cpp}` + `main.cpp` wiring + `resources/fonts/` + `fonts.qrc` + DEB integration | no |
| S5 | Env dump + capture infrastructure | `dump_render_env.py` + `capture_baselines.py` env-sidecar emit + CI artifact extension | no |
| S6 | Cross-env determinism verification | Re-capture V0.2 12 baselines local + CI under M16; verify < 1 % per state; H1 on failure | yes (verify + per-state operator review) |
| S7 | V0.2 baseline migration | Archive old V0.2-era baselines to `tests/visual/baselines-v0.2-archive/`; install M16 baselines at `tests/visual/baselines/`; ctest green | yes (final per-state baseline approval) |
| S8 | M17+ foundation docs + close + PR | `widget-styling-guide.md` + `.claude/M16-done.md` + PR to `main` | Phase 2 (PR review) |

## 3. Time budget

**No calendar commitment** per V0 charter §4 + §8. Quality is
the only gate. M16 closes when all 12 acceptance categories
(per spec §5) + close gate (< 1 % cross-env diff on 12 V0.2
baselines under M16 rendering) hold.

For rough sequencing (per spec §7 nominal):

- S0 (concerns + research): 1 day
- S0.5 (spike): 0.5–1 day — **preventive; saves rework if fails**
- S1 (manifesto + contracts): 1–2 days
- S2 (tokens + generator): 1 day
- S3 (diff + linter): 1 day
- S4 (Qt rendering + fonts): 1–2 days
- S5 (env dump): 0.5–1 day
- S6 (cross-env verify): 0.5–1 day
- S7 (baseline migration): 0.5 day
- S8 (docs + close): 0.5–1 day

Estimate ~1–1.5 weeks concentrated work; actual driven by
quality. S0.5 saves time if spike reveals infeasibility (catches
before manifesto + tokens commit).

## 4. HALT triggers (M16-specific, on top of CLAUDE.md §HALT)

Per spec §6:

| # | Trigger | Source / detection | Action |
|---|---|---|---|
| H1 | Cross-environment visual-diff ≥ 1 % on any V0.2 baseline after M16 rendering | S6 verification | HALT; remaining OS coupling not closed |
| H2 | Frozen-surface count > 2 | progress.md counter | HALT #5 standard; ADR required |
| H3 | Font licensing concern at S4 | License audit at S4 close | HALT; revisit Q3 with operator |
| H4 | Manifesto contradicts tokens at S2 review | S2 token-vs-manifesto cross-check | HALT; revisit manifesto or tokens (R11) |
| H5 | Industrial reference traceability gap (> 30 % of design decisions without citation) | S1 manifesto self-audit + Phase 4 review | HALT; manifesto over-reaches or under-researches (R10) |
| H6 | CMake font integration fails at S4 | Build error at S4 | HALT; revisit `fonts.qrc` approach (resource compile vs filesystem) |
| H7 | QSS startup performance regression > 50 ms | Measurement at S3 close | HALT; investigate per-widget styling |
| H8 | V0.2 baseline regression breaks > 3 baselines visually (R8 operator rejection at S6) | Operator review at S6 | HALT; investigate tokens vs manifesto |
| H9 | Operator rejects manifesto at S1 Phase 4 | Phase 4 review | HALT; revisit design direction |
| H10 (NEW R14) | Environment drift on env sidecar | S6 / S7 capture-time diff | HALT; env sidecars must match required contract values; fix env first, do not accept/reject baselines |
| H11 (NEW R13) | S0.5 spike fails | Spike measurement at S0.5 close | HALT; minimal determinism stack cannot reduce 2 baseline diff to < 3 %; redesign before manifesto / token investment. Investigate font rasterization / DPI / platform plugin / Qt patch behavior. May result in M16 scope amendment (fontconfig pinning, FreeType version pin, etc.) |
| H12 (NEW R15) | Token generator drift | CI `--check` in workflow | HALT only if generator itself broken; otherwise re-run generator + commit |

Plus CLAUDE.md standard set: compile error 3×, test fail 3×,
new dep, frozen-interface modification without ADR, perf miss
after 1 optimisation pass, spec/architecture contradiction,
Qt 6.10 anomaly, two plausible implementations, unexplained
git failure.

## 5. Subtask details

### S0 — Concerns + reference research

**Inputs**: M16 spec §3 (7 locked design decisions) + §4
(implementation sketches) + §6 (HALT triggers) + §7 (subtask
sequence) + §8 (R10–R15 enforcement) + understanding §5 (open
questions C1–C9) + V0.3 charter amendment §6 (R10–R15
discipline).

**Output**:

- `.claude/M16-concerns.md` resolving C1–C9 (this commit
  produces draft; Phase 4 review locks values).
- `docs/v0.3/references/` folder populated with industrial
  software reference screenshots + citation files. Per
  spec §3 M16.1 references: Saleae Logic 2 (primary), Tektronix
  MSO series (primary), LabVIEW Fuse (primary). Anti-references:
  consumer-app aesthetic, Material Design clone, smartphone-touch
  optimisation.
- Reference inventory map: which design principle each
  screenshot supports.
- `M16-progress.md` scaffold (subtask state table + R10–R15
  discipline reminder + frozen-surface counter at 0 / 2).

**Effort**: docs-only commit
(`docs: M16 S0 — concerns C1-C9 + reference research`).

**Phase 4 gate**: human reviews S0 + locks C1–C9 values.

### S0.5 — Minimal determinism technical spike (R13)

**Inputs**: S0 reference research + locked C2 (algorithm
tunables) + locked C3 (environment contract values) + locked
C7 (spike scope).

**Output**:

- Prototype branch / patch implementing minimal M16 stack:
  - `QApplication::setStyle("Fusion")` in `main.cpp`.
  - `QFontDatabase::addApplicationFont` loading Inter (bundled
    from a temporary fixture or downloaded to a per-spike
    workdir; not committed to repo at S0.5).
  - Minimal `QPalette` setting only 4–6 critical ColorRoles.
  - **No QSS**, **no token generator**, **no manifesto** at
    S0.5 — that's the point of R13 spike: test the technical
    foundation isolated.
- Capture 2 V0.2 baselines (C7-locked; recommended 00 + 24)
  under prototype rendering on operator local + CI.
- Apply the visual-diff algorithm (S3-prefigured; minimal
  Python implementation at S0.5) to measure cross-env diff.
- Result document: `docs/v0.3/spike-result.md` with diff
  numbers per baseline + env sidecar snapshots + interpretation.
- **Gate**: visual-diff < 3 % per baseline = PASS (continue
  M16); ≥ 3 % = H11 + HALT report.

**Effort**: 0.5–1 day. Single commit if PASS; PR ephemeral
prototype branch may be kept for trail.

**Operator-blocking**: yes. Operator reviews `spike-result.md`
+ approves "proceed to S1" OR triggers redesign (per R13
preventive governance).

### S1 — Manifesto + algorithm + env contracts

**Inputs**: S0 references + S0.5 spike PASS + locked C1–C9
values.

**Output**:

- `docs/v0.3/visual-identity.md` (manifesto):
  - Domain positioning (embedded bring-up workbench).
  - Visual hierarchy priorities.
  - Signal-semantic visual language.
  - Theme context model.
  - Cross-platform determinism as design constraint.
  - Industrial reference traceability per R10.
  - ≥ 70 % of principles cite ≥ 1 industrial reference (H5
    gate).
- `docs/v0.3/visual-diff-contract.md`:
  - Algorithm steps per spec §4.1.
  - Tunable parameters (C2-locked values).
  - Implementation will follow at S3.
- `docs/v0.3/rendering-environment-lock.md`:
  - Required Qt / QPA / style / font / DPR / locale values
    (C3-locked).
  - Disallowed env vars.
  - Env dump sidecar format.
  - Implementation will follow at S5.

**Effort**: 1–2 days. Single docs commit
(`docs: M16 S1 — visual identity manifesto + visual-diff +
rendering-env contracts`).

**Phase 4 gate**: operator reviews manifesto. H9 if rejected;
revisit design direction.

### S2 — Token source + generator

**Inputs**: S1 manifesto + locked C4 (token schema).

**Output**:

- `resources/styles/tokens.json` canonical source (light theme
  values traced to manifesto).
- `resources/styles/tokens.schema.json` JSON Schema 2020-12.
- `tools/generate_style_assets.py` producing:
  - `resources/styles/tokens.qss`.
  - `src/app/generated_style_tokens.hpp`.
  - `tests/visual/lib/generated_tokens.py`.
- CI workflow extension: `python3 tools/generate_style_assets.py
  --check` step (H12 on drift).

**Effort**: 1 day. Single commit
(`build: M16 S2 — tokens.json + generator + 3 generated
consumers`).

**Operator review**: rough review of generated values; not
blocking.

### S3 — Visual-diff algorithm implementation + QSS linter

**Inputs**: S1 algorithm contract + S2 tokens (for token-based
test expectations).

**Output**:

- `tests/visual/lib/compare.py` extended:
  - `compare_with_contract(actual, baseline, mask=None,
    pixel_threshold=4, cluster_threshold=200,
    percent_threshold=1.0)` new API.
  - Env sidecar pre-check (reads `<actual>.env.json` +
    `<baseline>.env.json`; mismatched required fields →
    INVALID, not soft-fail per H10).
  - Image size pre-check (mismatch = immediate fail).
  - Per-pixel-channel delta computation.
  - 4-connected clustering for max-cluster metric.
  - Optional masking via `<state>.mask.json`.
  - Backward-compat: old `compare_baseline` API preserved
    until S7 migration.
- QSS selector linter (`tools/lint_qss.py`):
  - Parses QSS for prohibited patterns (`*`, deep descendants,
    inline `setStyleSheet` calls in non-test source).
  - Run by CMake at build time (CMake custom target).

**Effort**: 1 day. Single commit
(`build: M16 S3 — compare.py contract + QSS selector linter`).

### S4 — Qt rendering ownership + bundled fonts

**Inputs**: S2 tokens + S3 contract values + locked C5 + C6.

**Output**:

- `src/app/app_style.hpp` + `src/app/app_style.cpp`:
  `SignalForgeStyle` class with `applyAtStartup(QApplication*)`,
  `loadBundledFonts()`, `applyLightPalette(QApplication*)`,
  `applyGlobalStylesheet(QApplication*)`,
  `verifyEnvironmentContract()` static methods.
- `src/app/main.cpp` updated: `QApplication::setStyle("Fusion")`
  + `SignalForgeStyle::applyAtStartup(&app)` after construction.
- `resources/fonts/`: Inter (regular / medium / bold / italic),
  JetBrains Mono (regular / medium), `LICENSE.OFL.Inter`,
  `LICENSE.Apache2.JetBrainsMono`, `fonts.qrc`.
- CMake: `fonts.qrc` compiled into binary; DEB installer
  payload includes font files for filesystem fallback.
- Fail-fast at QApplication init if font load fails (H10
  triggered if not).

**Effort**: 1–2 days. Single commit
(`build: M16 S4 — Qt rendering ownership + bundled fonts`).
Frozen-surface counter check: 0 → expected to stay 0 (all
work on non-frozen surfaces).

### S5 — Env dump + capture infrastructure

**Inputs**: S1 env contract + S4 rendering (so `dump_render_env.py`
can query `QApplication` state via a SignalForge CLI hook).

**Output**:

- `tests/visual/scripts/dump_render_env.py` — emit JSON sidecar
  per spec §4.5.
- `src/app/main.cpp` CLI flag `--dump-render-env <path>` (new):
  invokes `SignalForgeStyle::dumpEnvironmentJson` then exits.
- `tests/visual/scripts/capture_baselines.py` extended:
  - After each PNG capture, invoke `dump_render_env.py` to
    produce `<state>.env.json` sidecar.
  - Both files uploaded together by CI artifact step (S5
    workflow extension).
- `.github/workflows/ci.yml` extended:
  - Token-freshness check (`generate_style_assets.py --check`).
  - Env sidecar artifact bundle (already covered by existing
    `tests/screenshots/**` glob; verify).

**Effort**: 0.5–1 day. Single commit
(`build: M16 S5 — env dump + capture-baselines.py sidecar
integration`).

### S6 — Cross-env determinism verification

**Inputs**: S2–S5 complete; V0.2's 12 production-fidelity
baselines preserved at `tests/visual/baselines/` (current
HEAD `91c2633` set; will be replaced by M16 baselines at S7).

**Output**:

- Re-capture all 12 V0.2 baselines under M16 rendering on
  operator local + CI run.
- Apply visual-diff algorithm (compare each pair: M16-local
  vs M16-CI).
- Verify all 12 states < 1 % per algorithm.
- Capture env sidecars; verify 100 % contract compliance.
- Operator R8 per-state review: each new M16 baseline is
  visually correct vs the V0.2-era baseline (semantic
  equivalence + production-fidelity preserved).
- Coverage map in `M16-progress.md` §S6.

**Effort**: 0.5–1 day. Single commit if all pass; multiple if
H1 + iterations needed.

**Operator-blocking**: yes (per-state R8 review). If > 3
states visually rejected by operator → H8 → investigate.

### S7 — V0.2 baseline migration

**Inputs**: S6 PASS + operator approval of 12 M16 baselines.

**Output**:

- Move `tests/visual/baselines/*.png` → `tests/visual/baselines-v0.2-archive/`
  (12 files + the M14-S1-smoke if accepted at M15 close).
- Install 12 M16-rendering baselines + sidecars at
  `tests/visual/baselines/`.
- `tests/visual/baselines-v0.2-archive/INDEX.md` cataloguing
  each archived baseline with the V0.2 commit SHA it came from.
- `scripts/accept-baseline.sh` updated if needed (likely no
  change — promote path stays `tests/screenshots/<...>/<state>.png`
  → `tests/visual/baselines/<state>.png`).
- ctest visual suite green local + CI.

**Effort**: 0.5 day. Single commit
(`fix: M16 S7 — V0.2 baseline migration to M16 deterministic
baselines`).

**Operator-blocking**: yes (final per-state baseline approval
before archive becomes immutable).

### S8 — M17+ foundation docs + close + PR

**Inputs**: all prior subtask outcomes.

**Output**:

- `docs/v0.3/widget-styling-guide.md`:
  - Token consumption examples.
  - QSS class naming convention.
  - Prohibited patterns + linter info.
  - Theme switch hook (light-only; dark slot for M20).
  - Token addition workflow.
- `.claude/M16-done.md` per spec §9 (M17+ hand-off):
  - Design tokens reference.
  - Widget styling guide pointer.
  - Cross-env determinism state.
  - Visual-diff contract reference.
  - Environment contract reference.
  - V0.3 progression.
  - V1 UX gap inheritance (items #1–#10 → M17/M18; #11
    closed by M16).
  - Industrial reference inventory.
- Open PR `milestone/M16` → `main` with M16-done.md as body
  (or summary thereof). Phase 2 operator review unlocks
  Phase 3 merge + tag (if V0.3 tagging chosen at this
  milestone — likely defer to V0.3 close not M16 individual).

**Effort**: 0.5–1 day. Single commit (`docs: M16 S8 —
widget-styling-guide + M16-done.md`).

**Phase 2 gate**: operator reviews PR + says "approved, merge
M16 and begin M17 bootstrap". Phase 3 (CC autonomous): merge
PR, no v0.3.x tag (V0.3 tag follows V0.3 close at M20 per
charter amendment §3 + §8), bootstrap `milestone/M17`.

## 6. Operator-blocking deliverables

CC-blocking (M16 commits land these autonomously):

- S0 concerns + reference research
- S0.5 spike implementation + measurement (operator only
  proceeds/HALTs based on spike result)
- S1 docs draft (operator reviews; CC iterates if H9)
- S2 token implementation
- S3 diff + linter implementation
- S4 Qt rendering wiring
- S5 env dump implementation
- S7 baseline migration mechanics
- S8 docs

Operator-blocking:

- Phase 4 reference direction review (post-S0)
- **S0.5 proceed/HALT decision** (post-spike; if spike fails,
  redesign before continuing per R13)
- Phase 4 manifesto approval (post-S1)
- S2 token review (rough check; not strict gate)
- S6 cross-env baseline approval (per-baseline R8 review under
  M16 rendering)
- S7 final baseline approval before old V0.2 archival
- S8 Phase 2 PR review

## 7. Branching + PR plan

- `milestone/M16` branched from `main` (V0.3 charter merge at
  `a0472c4` + V0.2 close at `91c2633`). Already created
  locally + pushed at Step h.
- M16 PR opens at S8 closure: `milestone/M16 → main` (per
  V0 charter §6 + V0.2 PR pattern from `#28`).
- No `v0.3.x` tag at M16 close: V0.3 tagging follows V0.3
  close (M20 or wherever V0.3 closes per charter amendment §3,
  §8). M16 is keystone but not V0.3-complete.
- After PR merge: bootstrap `milestone/M17` (V0.3 Pillar B widget
  rebuild) per CLAUDE.md §"Milestone closure flow" Phase 3.

## 8. Cross-references

- Spec: `docs/milestones/M16-visual-identity-ownership.md` (v2)
- V0.3 charter amendment: `docs/V0-charter-amendment-v0.3.md`
- V0 series charter: `docs/V0-series-charter.md`
- Understanding: `.claude/M16-understanding.md` (this file's pair)
- M15 closure (predecessor): `.claude/M15-done.md` (R5/R7/R8/R9
  retrospective + §14 V1.0 cross-env ship gate framing)
- M15 progress (S3 baselines + V1 UX gap inventory):
  `.claude/M15-progress.md`
- V0.2 vision infrastructure guide:
  `docs/v0.2-vision-infrastructure.md`
- V0.2 production-fidelity baselines (M16 source for re-capture):
  `tests/visual/baselines/` at HEAD `91c2633`
- V0.2 capture orchestrator (M16 extension target):
  `tests/visual/scripts/capture_baselines.py`
- V0.2 compare module (M16 extension target):
  `tests/visual/lib/compare.py`
- V0.2 visual-test framework (M16 extension target):
  `tests/visual/tests/test_states_*.py`
- M14 S1 smoke (Tier C extension verified at V0.2 close):
  `tests/integration/gui/release_binary_smoke.sh`
- ADR-010 software-RHI lesson: `docs/architecture/adrs/ADR-010-*`
- CLAUDE.md governance contract: top of repo
