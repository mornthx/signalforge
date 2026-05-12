# M16 — Understanding (V0.3 keystone: Visual Identity Ownership)

Source: `docs/milestones/M16-visual-identity-ownership.md` v2 (at
`73f55da`); `docs/V0-charter-amendment-v0.3.md` (at `73f55da`); V0.2
close report `.claude/M15-done.md` §10 R9 + §14.

## 1. Goal

M16 is the V0.3 keystone milestone. It takes SignalForge from
"OS-styled application" (V1/V0.2 inherited Qt defaults; same binary
on operator dev vs CI runner produced 14–33 % pixel diff with
semantically identical content per V0.2 R9) to **"self-styled
application on the declared supported environment matrix"** —
Ubuntu 24.04 operator dev + CI runner only, multi-platform deferred.
After M16 closes, the same SignalForge binary on supported-matrix
environments produces visual-diff < 1 % (per algorithm contract,
not raw pixel-diff) for the same application state, with an
explicit visual identity manifesto + token-driven theming +
environment contract + spike-validated technical stack +
foundation for M17–M20+. This is the keystone because M17 widget
rebuild, M18 workflow rebuild, M19 hardware fixtures, M20 theme
variants all assume a deterministic visual foundation; without it,
V0.3 pixel-diff regression cannot distinguish "SF code changed"
from "OS environment differs", and V0.3 cannot ship to the V1.0
gate (M15-done.md §14).

## 2. What ships

Per spec §2.1 — 11 must-deliver items expanded with implementation
context:

1. **Visual identity manifesto** (`docs/v0.3/visual-identity.md`) —
   domain positioning (embedded bring-up workbench), visual hierarchy
   priorities (signal > measurement > control > chrome), signal-
   semantic visual language, theme context model, cross-platform
   determinism as design constraint, industrial reference
   traceability (R10). Manifesto comes first (R11), tokens trace
   to manifesto principles.
2. **Visual-diff algorithm contract** (`docs/v0.3/visual-diff-contract.md`)
   — primary metric (percent of pixels with any-channel delta > threshold),
   secondary metric (max contiguous cluster size), env-contract
   pre-check, image-size pre-check, optional masking, tunable
   parameters (PIXEL_THRESHOLD / CLUSTER_THRESHOLD / PERCENT_THRESHOLD).
3. **Rendering environment contract** (`docs/v0.3/rendering-environment-lock.md`)
   — Qt version / QPA platform / QStyle / font family + size / DPR /
   locale required values; disallowed env vars (QT_SCALE_FACTOR etc.);
   Wayland forbidden, software RHI required for canonical baselines.
4. **Capture-time env dump** (`tests/visual/scripts/dump_render_env.py`)
   — JSON sidecar `<state>.env.json` next to each PNG; CI uploads
   alongside artifacts; visual-diff pre-check rejects env-contract
   violations (R14 / H10).
5. **Token source-of-truth + generator** (R15) —
   `resources/styles/tokens.json` (operator-readable JSON Schema-
   validated); `tools/generate_style_assets.py` produces
   `resources/styles/tokens.qss` (Qt stylesheet snippet),
   `src/app/generated_style_tokens.hpp` (`constexpr` C++ values),
   `tests/visual/lib/generated_tokens.py` (test-consumer dict).
   CI `--check` mode fails if generated files drift from source.
6. **Qt rendering pipeline ownership** (`src/app/app_style.{hpp,cpp}`
   + `main.cpp`) — `SignalForgeStyle` class: `QApplication::setStyle("Fusion")`,
   `QFontDatabase::addApplicationFont` bundled-fonts, explicit
   `QPalette` from generated tokens, global QSS load from generated
   tokens, optional `SF_VERIFY_RENDER_ENV` startup gate. QSS discipline
   (per spec §3 M16.4 / R6 from operator review): objectName / class
   / dynamic-property selectors only, no `*` or deep descendants,
   no hot-path `setStyleSheet`, build-time linter validates.
7. **Bundled fonts** (`resources/fonts/`) — Inter (SIL OFL) regular /
   medium / bold / italic; JetBrains Mono (Apache 2.0) regular /
   medium; LICENSE files; CMake compile-into-resource via `fonts.qrc`;
   DEB installer integration; fail-fast at QApplication init if font
   load fails (H10).
8. **Cross-environment determinism verification** — re-capture V0.2
   12 production-fidelity baselines under M16 rendering, once
   operator-local + once CI; visual-diff < 1 % per state; any
   ≥ 1 % → HALT (H1).
9. **V0.2 baseline regression** — old V0.2-era CI-environment-
   captured PNGs moved to `tests/visual/baselines-v0.2-archive/`;
   new M16 deterministic PNGs installed at `tests/visual/baselines/`;
   operator one-time per-state review under R8 authority.
10. **M17–M20+ foundation** (`docs/v0.3/widget-styling-guide.md`)
    — how widgets consume design tokens, QSS class naming convention,
    theme switch hook (light-only M16; dark slot for M20),
    prohibited patterns + linter info, token addition workflow.
11. **`.claude/M16-done.md`** with V0.3 progression status + M17
    hand-off (design tokens reference, widget styling guide pointer,
    cross-env determinism state, visual-diff contract reference,
    environment contract reference, V0.3 progression, V1 UX gap
    inheritance, industrial reference inventory).

Additionally, per spec §7 the **S0.5 minimal determinism spike**
ships its own deliverable: prove that Fusion + bundled fonts +
minimal palette stack reduces 2 baseline diffs (00-empty-launch +
24-dialog-add-serial) to < 3 % cross-env BEFORE manifesto / token
investment. Spike pass = continue full M16; spike fail = H11 +
redesign technical stack (R13 preventive governance).

## 3. Hard-stop criteria

Per spec §5 — 12 acceptance categories rolled up:

M16 closes when **all** hold:

1. **Manifesto landed**: `docs/v0.3/visual-identity.md` published;
   industrial reference screenshots in `docs/v0.3/references/`;
   each principle cites ≥ 1 industrial reference (R10 ≥ 70 %
   traceability gate; H5 fires at < 70 %).
2. **Algorithm contract landed**: `docs/v0.3/visual-diff-contract.md`
   published; `tests/visual/lib/compare.py` implements contract
   exactly (env pre-check, size pre-check, per-pixel + clustering
   metrics, masking support); tunables documented with defaults.
3. **Environment contract landed**: `docs/v0.3/rendering-environment-lock.md`
   published; `dump_render_env.py` emits sidecars on every capture;
   visual-diff rejects env-drift as INVALID (not soft-fail) per
   H10.
4. **S0.5 spike passed**: 2 baselines local + CI under prototype
   M16 stack; visual-diff < 3 % loose threshold; if not → H11,
   redesign before continuing.
5. **Tokens + generator landed**: `tokens.json` schema-validated
   canonical source; generator produces all 3 consumers; CI
   `--check` integrated; H12 fires on generator drift.
6. **Qt rendering ownership**: `SignalForgeStyle` in
   `src/app/app_style.{hpp,cpp}`; `main.cpp` invokes at
   QApplication init; Fusion forced; QPalette explicit; QSS
   loaded; QSS linter passes; frozen-surface counter ≤ 2 (H2 / HALT #5).
7. **Bundled fonts**: Inter + JetBrains Mono in `resources/fonts/`
   with LICENSE files; CMake `.qrc` compile; `addApplicationFont`
   at startup; fail-fast on load failure; DEB integration.
8. **Cross-environment determinism**: V0.2's 12 baselines
   re-captured local + CI under M16; env sidecars 100 % contract-
   compliant; visual-diff < 1 % for all 12 states; H1 fires on
   any state ≥ 1 %.
9. **V0.2 baseline regression**: old archive moved; new M16
   baselines installed; operator R8 per-state approval; ctest
   visual suite green local + CI.
10. **QSS discipline + performance**: linter integrated; no
    hot-path `setStyleSheet` (CI grep check optional); QSS startup
    overhead < 50 ms (H7 fires above 50 ms).
11. **M17+ foundation docs**: `widget-styling-guide.md` published
    with token consumption examples, QSS class naming convention,
    prohibited patterns + linter info, theme switch hook
    (light-only; dark slot), token addition workflow.
12. **Documentation complete**: all 4 V0.3 docs published
    (visual-identity / visual-diff-contract / rendering-environment-lock
    / widget-styling-guide); references folder populated;
    `.claude/M16-done.md` with M17 hand-off.

## 4. Hard constraints

Per spec §2.2 — 9 must-not-do items:

1. **No V1 UX gap fixes during M16** — M15-done.md §4 items #1–#10
   are M17/M18 scope; only item #11 (visual identity ownership)
   closes via M16.
2. **No widget rebuild during M16** — M17 scope; M16 is
   foundation-only.
3. **No new functional features** — backend frozen per V0 charter §3.
4. **No M2–M12 frozen `.hpp` modification** without ADR + frozen-
   surface counter bump; HALT #5 at > 2 modifications.
5. **No dark theme variant in M16** — light-only ships per Q2
   default; dark M20 inherits the generator infrastructure.
6. **No multi-platform scope expansion** — Ubuntu 24.04 operator
   dev + CI runner only; multi-platform deferred to V0.4+ / V1.x.
7. **No design without manifesto basis** (R11) — every token
   value traces to a manifesto principle; H4 fires on contradiction.
8. **No tokens without generator-driven single-source** (R15) —
   no hand-written values in QSS / C++ / Python that should
   come from `tokens.json`; H12 fires on drift.
9. **No baseline acceptance without environment contract compliance**
   (R14) — env sidecar must match required contract values; H10
   fires on drift.

## 5. Open questions for S0 (concerns C1–C9)

These resolve during S0 in `.claude/M16-concerns.md`. Phase 4
operator review locks each before S0.5 spike begins.

- **C1 — Industrial reference research methodology**: which
  references (Saleae Logic 2 / Tektronix MSO / LabVIEW Fuse +
  any anti-references) get studied; how screenshots are sourced
  (vendor public material vs operator-installed evaluation versions);
  citation format for R10 traceability (in-line markdown + URL +
  filename of stored screenshot + observed-version tag).
- **C2 — Visual-diff algorithm tunable parameter values**:
  PIXEL_THRESHOLD (default 4/255 ≈ 1.5 %); CLUSTER_THRESHOLD
  (default 200 px); PERCENT_THRESHOLD (default 1.0 %); per-state
  override mechanism. Tunable defaults justified empirically
  against the V0.2 R8 14–33 % cross-env drift evidence.
- **C3 — Environment contract values**: Qt patch flexibility
  (Qt 6.10.2 only vs 6.10.x band); locale (`C.UTF-8` vs
  `en_US.UTF-8` — pick one for canonical); xvfb specific args
  (`-screen 0 1280x800x24` vs 1280x720); device pixel ratio
  forced to 1.0; freetype + fontconfig version observation
  (advisory not gating).
- **C4 — tokens.json schema design**: JSON Schema 2020-12 file
  `resources/styles/tokens.schema.json`; theme top-level (`light`
  required, `dark` optional slot for M20); color hex-only vs
  named-rgb; font sizes integer point sizes; spacing scale
  validation (multiples of 4); semantic key naming (`bg.primary` /
  `signal.0`–`signal.7` etc.); versioning (`version: "1.0"` per
  spec §4.3 sketch).
- **C5 — Qt rendering implementation choices**: QSS resource
  compile vs runtime load (resource compile lockstep with binary;
  runtime load allows post-build theme swap — pick resource compile
  for M16 determinism); QPalette ColorRole mapping (all 18
  QPalette roles set from tokens, no defaults); `SignalForgeStyle`
  static-class vs instance (static — no per-window state).
- **C6 — Bundled fonts approach**: Qt resource compile via
  `fonts.qrc` (single binary, fonts compiled in) vs filesystem
  install + `addApplicationFont` at runtime (split deploy, easier
  font update). Resource compile recommended for M16
  determinism. DEB integration: fonts.qrc inside binary plus
  `/usr/share/fonts/signalforge/` for filesystem fallback?
- **C7 — S0.5 spike scope details (critical for R13 enforcement)**:
  which 2 baselines (00-empty-launch + 24-dialog-add-serial
  recommended — empty-state for chrome-only diff; dialog for
  text-heavy form diff); exact prototype stack (Fusion + Inter
  Regular 12pt + bundled Inter + minimal QPalette of 4–6 roles,
  no QSS yet); success/HALT threshold (< 3 % loose = continue;
  ≥ 3 % → H11 + redesign); operator local environment
  documentation (Ubuntu 24.04 + Yaru remnants vs CI Ubuntu 24.04
  + Fusion default).
- **C8 — V0.2 baseline migration mechanics**: archive directory
  structure (`tests/visual/baselines-v0.2-archive/` vs `baselines/v0.2/`
  subdir); accept-baseline.sh updates (does the script need a
  `--archive-old` flag or does S7 use a one-time `mv` operation?);
  index file mapping old → new (audit trail); operator review
  process for the 12 new M16 baselines (R8 per-state).
- **C9 — M17+ foundation interface**: what M17 widget rebuild
  needs from M16 (header path for generated tokens, QSS class
  naming, palette role mapping); what theme switch hook API
  looks like for M20 dark (function signature, runtime vs
  startup-only switch, palette emit signal for live update vs
  re-launch); how M17 widget tests integrate with the env-dump
  contract (sidecar emit hook).

## 6. M2–M12 frozen surface — verified intact at M16 entry

V0 charter §3 + §7 + CLAUDE.md §HALT #5: M2–M12 `.hpp` files
remain frozen. Per `docs/v1.0-spec-list.md` §1 freeze record,
no modification of those files during V0.x without ADR + counter
bump (HALT #5 at > 2 per milestone).

M16 work targets non-frozen surfaces:

- `main_window.cpp` (V1 integration point) — likely small change
  for `SignalForgeStyle` init wiring, possibly no change if
  init lives entirely in `main.cpp`.
- `main.cpp` — `QApplication` style + palette + font + QSS init.
- New non-frozen surfaces (M16-introduced):
  - `src/app/app_style.hpp` / `app_style.cpp` (not pre-existing;
    not frozen at creation per ADR-pattern).
  - `resources/styles/tokens.json`, `tokens.schema.json`,
    `tokens.qss` (generated).
  - `resources/styles/global.qss` (hand-written; references
    generated tokens).
  - `resources/fonts/*.ttf` + `LICENSE.*` + `fonts.qrc`.
  - `tools/generate_style_assets.py`.
  - `src/app/generated_style_tokens.hpp` (generated).
  - `tests/visual/lib/generated_tokens.py` (generated).
  - `tests/visual/scripts/dump_render_env.py`.
- Existing visual-test infrastructure under `tests/visual/`
  extended for env sidecar + algorithm contract (non-frozen).
- `.github/workflows/ci.yml` extended for token-freshness check
  + env sidecar artifact upload (non-frozen).

Frozen-surface counter at M16 entry: **0 / 2** (clean).

If any M16 work requires a frozen `.hpp` change (none anticipated
at this scope), ADR follows V1 pattern (008 / 009 / 010 / 011 /
013): document why frozen + design + counter bump; HALT #5 if
counter > 2.

## 7. Quality philosophy

V0 charter §8 carried forward + new R13 discipline:

- **Quality > schedule**. M16 closes when all 12 acceptance
  categories hold + close gate (< 1 % cross-env diff on 12
  baselines). No calendar commitment.
- **Spike before design** (R13, this milestone's new discipline).
  S0.5 minimal determinism spike runs BEFORE manifesto +
  token investment. Spike fails → H11 → redesign technical
  stack (e.g., add fontconfig pin, force specific FreeType
  version, switch from Fusion to a custom QStyle). This is
  preventive governance: catches infeasibility cheaper than
  HALT during S4 implementation.
- **Environment contract before measurement** (R14, this
  milestone's new discipline). Without environment contract +
  per-capture env dump, "deterministic" is unfalsifiable; H10
  rejects env-drift as INVALID diff (not soft-fail).
- **Single-source design assets** (R15, this milestone's new
  discipline). `tokens.json` is canonical; generator produces
  all consumers; CI `--check` enforces freshness; H12 fires
  on drift. Prevents the V0.2-era hand-edited-in-multiple-places
  failure mode.
- **Manifesto-first ordering** (R11 carried forward + first
  applied here). S1 produces manifesto BEFORE S2 fixes specific
  token values. Every token traces to a manifesto principle.
- **Industrial reference traceability** (R10 carried forward).
  Every design decision cites at least one industrial reference
  (Saleae / Tektronix / LabVIEW / MATLAB / NI / Yokogawa).
  ≥ 70 % traceability gate; H5 fires below.
- **Production-fidelity acceptance bar** (R7 carried forward).
  Captures must reflect production code paths, not test-only
  state mutation.
- **Per-baseline review authority** (R8 carried forward).
  Operator deliberately accepts each M16 baseline at S6 + S7.
- **Plan ordering source of truth** (R5 carried forward).
  Post-compaction CC re-reads M16-plan.md before next subtask.

## 8. Cross-references

- M16 spec: `docs/milestones/M16-visual-identity-ownership.md`
- V0.3 charter amendment: `docs/V0-charter-amendment-v0.3.md`
- V0 series charter: `docs/V0-series-charter.md`
- M15 closure: `.claude/M15-done.md` (R9 retrospective + §14 V1.0 cross-env ship gate)
- M15 progress: `.claude/M15-progress.md` (§S3 R7 baselines + V1 UX gap inventory)
- M15 CC autonomy demo: `docs/m15-cc-autonomy-demo.md` (S6 demo — pixel-diff insufficiency motivation)
- V0.2 vision infrastructure guide: `docs/v0.2-vision-infrastructure.md`
- ADR-010 software-RHI rasterization lesson: `docs/architecture/adrs/ADR-010-*`
- Operator-approved V0.2 baselines: `tests/visual/baselines/` (12 production-fidelity at HEAD `91c2633`)
- Operator-approved accept workflow: `scripts/accept-baseline.sh`
- CLAUDE.md (governance contract): top of repo
