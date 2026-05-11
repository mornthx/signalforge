# V0 Series Charter — V0.3 Phase Amendment

| Field | Value |
|---|---|
| Amendment date | 2026-05-11 |
| Authored after | V0.2 close (v0.2.0 at 91c2633), M15-done.md R9 + §14 |
| Revised after | Operator review (2026-05-11) — 6 substantive risks identified |
| Supersedes | V0 Charter §2.2 "V0.3 — Industrial UI/UX Rebuild" original framing |
| Status | Active |

---

## 1. Why amend

V0 Series Charter §2.2 originally framed V0.3 as "Industrial UI/UX
Rebuild" with sub-milestones M16 design tokens / M17 widget rebuild /
M18 workflow rebuild. This framing was preliminary, authored before
V0.2 R9 discovery surfaced two structural realities:

1. **SignalForge V1 does not own its visual identity.** Same binary
   on operator dev (Ubuntu + Yaru theme + Cantarell fonts) vs CI
   runner (xvfb + Fusion fallback + base fonts) produces 14-33%
   pixel diff with semantically identical content. Pixel-diff
   regression detection cannot work cross-environment until this
   gap closes. (M15-done.md V1 UX inventory #11; §14 V1.0 cross-env
   ship gate framing.)

2. **V0.2 baseline pool is OS-coupled.** 12 production-fidelity
   baselines are CI-environment-specific snapshots. They cannot
   serve as V0.3 measurement reference until V0.3 owns the
   rendering cascade. (M15-done.md §10 R9 retrospective.)

The original M16 = "design tokens" framing addresses the symptom
(no consistent colors/fonts) without addressing the root (no owned
rendering cascade). This amendment reframes V0.3 to start with
ownership before tokenization.

## 2. V0.3 reframed scope

V0.3 ("Industrial UI/UX rebuild") expands to two pillars:

### Pillar A — Visual identity ownership (M16)

Take SignalForge from "OS-styled application" to "self-styled
application" **on the declared supported environment matrix**.
Same binary on environments within the supported matrix produces
visual-diff under defined tolerance for same application state.
This is V0.3 keystone — without it, M17+ work has cross-environment
fragility and pixel-diff regression cannot reliably distinguish
"SF code changed" from "OS environment differs".

**Scope of determinism**: "deterministic rendering" is bounded by
the **declared supported environment matrix** (currently Ubuntu
24.04 operator dev + CI runner; multi-platform deferred to V0.4+
or V1.0). Unsupported environment configurations (other
distributions, other Qt versions, Wayland-vs-X11 differences,
HiDPI variants, etc.) are best-effort and do not define release
gates.

### Pillar B — Industrial UI/UX rebuild (M17+)

Standing on M16's ownership foundation, redesign widgets +
workflows + interactions to match industrial signal-analysis tool
standards (Tektronix / Saleae / LabVIEW / NI references).

This pillar contains M17 widget rebuild, M18 workflow rebuild,
M19 hardware fixtures, M20+ interactive states (subject to
revision per V0.3 progress).

## 3. V0.3 sub-milestone progression (revised)

### M16 — Visual identity ownership (keystone, first priority)

**Goal**: Take ownership of SignalForge's rendering cascade.
Same binary, deterministic rendering across **declared supported
environment matrix** (Ubuntu 24.04 operator dev + CI runner for M16).

**Status**: V0.3 keystone milestone. M17+ blocked until M16
closes.

**Key deliverables** (full scope in M16 spec):
1. SignalForge visual identity manifesto (`docs/v0.3/visual-identity.md`)
2. Token source-of-truth + generator (`resources/styles/tokens.json` + `tools/generate_style_assets.py` producing QSS / C++ / Python)
3. Qt rendering pipeline ownership (QApplication style + palette + fonts + QSS)
4. Bundled fonts (Inter + JetBrains Mono)
5. **Rendering environment contract** (`docs/v0.3/rendering-environment-lock.md` + per-capture env dump)
6. **Visual-diff algorithm contract** (`docs/v0.3/visual-diff-contract.md`)
7. **Minimal determinism technical spike** (S0.5; proves stack reduces diff before full design investment)
8. Cross-environment determinism verification (visual-diff < 1% per defined algorithm)
9. V0.2 baseline regression to deterministic M16 baselines
10. Foundation deliverables for M17-M20+ work

**Close gate**: "Same commit, same binary build profile, same
declared M16 rendering environment contract, on Ubuntu 24.04
operator local + CI runner, produces visual-diff < 1% (per defined
algorithm) for all 12 V0.2 production-fidelity baselines re-captured
under M16."

Pixel-identical is the aspiration; visual-diff under measured
tolerance is the measurable gate.

### M17 — Widget rebuild

**Goal**: Rebuild core widgets per M16 design tokens. Fix V1 production
bugs surfaced in V0.2 fidelity audits.

**Scope** (subject to M17 spec):
1. Chart widget — fix `rebuildChartWidgets()` segfault under
   tight-loop chart-add timing (V0.2 HALT-20260510T172100Z;
   defer V0.3 per spec §2.2 #1)
2. Connection panel widget rebuild (industrial-grade visual hierarchy)
3. Signal selector widget rebuild
4. Status bar rebuild (live progress for recording per V1 UX gap #9 / #10)
5. Toolbar + menu visual rebuild

**Pre-requisites**: M16 closed.

### M18 — Workflow rebuild

**Goal**: Rebuild user-facing workflows per industrial standards.
Address V1 UX gaps surfaced in M15-done.md §4.

**Scope** (subject to M18 spec):
1. Recording workflow (V1 UX gap #4 auto-connect, #8 commands UI, #9 live progress)
2. Replay workflow (V1 UX gap #3 play toggle, #5 records display, #6 seek slider, #10 seek feedback)
3. Connection lifecycle (V1 UX gap #4 auto-connect, #7 default driver)
4. Mode transition dialogs
5. Quit-while-recording confirm
6. buffer/seek percent encoding (V1 UX gap #1 trivial fix)

**Pre-requisites**: M16 closed + M17 widget foundation in place.

### M19 — Hardware fixtures + extended state coverage

**Goal**: Lift 15 operator-manual residual states from V0.2 S3
to automated capture. Fault-injection + hardware-dependent +
transient states.

**Scope** (subject to M19 spec):
1. Serial / TCP fixture infrastructure (V0.2 deferred: states 08-11)
2. Fault-injection harness for error dialogs (states 27-29)
3. Transient ConnectionState capture (states 03, 06, 07)
4. Extreme buffer-pressure capture (states 34, 35)
5. Specialised modal flows (states 16, 22, 23)

**Outcome**: V0.2's 38-baseline target reached (12 V0.2 + 26 V0.3-deferred captured).

**Pre-requisites**: M16 closed + M17/M18 widgets/workflows stable.

### M20 — Interactive states + theme variants

**Goal**: Dark theme variant + accessibility variants + interactive
state polish.

**Scope** (subject to M20 spec):
1. Dark theme palette (built on M16 token generator infrastructure)
2. High-contrast accessibility variant
3. Keyboard navigation completeness
4. Tab order + focus ring visibility
5. Theme runtime toggle

**Pre-requisites**: M16-M19 closed; light theme stable as base.

### Beyond M20

If V0.3 quality not yet production-grade, iterate as M21+. No
calendar commitment per V0 charter §4 + §8. V1.0 ships when
quality acceptable, not when count of milestones reached.

## 4. V0.3 close criteria (provisional)

V0.3 closes (and v0.3.0 tags) when **all** hold:

1. **M16 keystone**: cross-environment determinism verified on
   declared supported environment matrix per defined visual-diff algorithm
2. **All V1 UX gaps closed**: M15-done.md §4 items 1-11 fixed in M17/M18/M19
3. **V0.2 deferred baselines captured**: 26 V0.3-hand-off states from M15-done.md §5 captured + accepted
4. **Vision infrastructure regression-free**: V0.2 S6 demo equivalent passes on V0.3 GUI
5. **Industrial reference alignment documented**: design decisions traced to LabVIEW / MATLAB / Tektronix / Saleae / NI / Yokogawa references where applicable
6. **Operator + CC acceptance**: V0.3 GUI usable for sustained work session without "looks broken" findings

If V0.3 close criteria met **and** V1.0 ship gate met (per §5
below), V1.0 tag follows directly.

If V0.3 close but V1.0 ship gate not met (e.g., additional polish
or beta tester feedback needed), V0.4 iteration follows.

## 5. V1.0 ship gate (revised per operator review)

Codified for V0 charter authoritative reference:

**SignalForge V1.0 ship requires deterministic Qt rendering across
the declared supported environment matrix.** Same binary on
environments within the supported matrix produces visual-diff under
the defined tolerance for same application state.

**Scope of "supported environment matrix"**: Defined per V1.0 release.
At minimum: Ubuntu 24.04 (operator dev) + Ubuntu 24.04 CI runner +
DEB package install on Ubuntu 24.04. Other distributions / OS /
display configurations are best-effort and do not define the release
gate. Multi-platform support (Windows / macOS) is candidate for V0.4+
or V1.x extensions, not required for V1.0.

Industrial signal-analysis software (LabVIEW / MATLAB Instrument
Control Toolbox / Tektronix scope GUI / Saleae Logic / NI VeriStand
/ Yokogawa IS-Series) all guarantee cross-platform pixel-determinism
within their declared supported platforms. SignalForge V1.0 must
match this expectation within its declared supported environment
matrix.

This requirement closes via M16 (visual identity ownership) for the
Ubuntu 24.04 + CI scope. Multi-platform extensions inherit the
invariant by following the same M16 design pattern (Fusion + bundled
fonts + explicit palette + QSS) on additional platforms.

## 6. Governance discipline carried forward

V0.2 R5/R7/R8/R9 lessons remain active for V0.3:

- **R5** Plan ordering source of truth (re-read plan after compaction)
- **R7** Production-fidelity acceptance bar (capture reflects production behavior, not fixture-mock)
- **R8** Per-baseline review authority (operator-deliberate approval)
- **R9** Cross-environment measurement coupling (M16 closes this for Ubuntu scope)

New V0.3-specific governance disciplines:

- **R10** Industrial reference traceability — design decisions cite
  the industrial software reference they derive from. Avoids
  "design by AI taste" without grounded basis. Examples:
  - "Chart widget background = #1a1d23" → traced to "Saleae Logic
    dark theme + Tektronix 5/6 Series scope display" (with screenshots)
  - "Status bar live byte progress" → traced to "Tektronix recording
    indicator pattern; Saleae capture progress bar pattern"

- **R11** Visual identity manifesto first, tokens second — M16 must
  produce the visual identity manifesto document **before**
  finalising specific token values. This prevents "design tokens
  without rationale" — every token traces back to a manifesto
  principle.

- **R12** Baseline regression discipline — every V0.3 milestone
  closes with V0.2 baselines re-captured + diff-verified. Any
  baseline regression is HALT trigger (intentional changes get
  operator-deliberate acceptance per R8; unintentional regression
  blocks milestone close).

- **R13** (NEW per operator review) **Technical spike before
  design investment** — when a milestone depends on technical
  unknowns (rendering determinism, performance, third-party API
  behavior, etc.), insert a minimal spike subtask BEFORE the full
  design work. If the spike fails, redesign before committing
  to elaborate manifestos / tokens / specs that the technical
  foundation cannot support. R13 is preventive governance, not
  reactive — catches infeasibility cheaper than HALT during
  implementation.

- **R14** (NEW per operator review) **Environment contract
  discipline** — when a milestone deliverable depends on environment
  behavior (Qt version, fonts, DPI, platform plugin, locale, etc.),
  produce an explicit environment contract document + capture-time
  env dump mechanism. Without environment contract, "deterministic"
  is unfalsifiable and HALT debugging takes infinite time.

- **R15** (NEW per operator review) **Generated assets single-source
  discipline** — when design assets (tokens, themes, palettes) feed
  multiple consumers (C++ runtime, QSS, Python tests, etc.), define
  one canonical source + generator producing all consumers. CI
  enforces "generated assets up to date" check. Avoids drift between
  runtime + test + documentation.

## 7. Backend remains frozen per V0 charter §3

V0.3 does not touch M2-M12 frozen .hpp surfaces. Production-fidelity
gaps requiring production code change (V1 UX gaps #3 / #6 / #9 / #10;
multi-chart segfault) target non-frozen surfaces:

- `main_window.cpp` (V1 integration point, unfrozen per M9/M11/M13/M14 precedent)
- `playback_controller.cpp` (.cpp-only changes; .hpp untouched)
- Chart `*.cpp` files where ADR-010/011 precedent established access rules
- Test infrastructure under `tests/`
- New non-frozen surfaces:
  - `src/app/app_style.{hpp,cpp}` (M16-introduced)
  - `resources/styles/`, `resources/fonts/`, `tools/generate_style_assets.py`

Any V0.3 work requiring frozen-surface modification follows V1
ADR pattern (008/009/010/011/013): ADR + frozen-surface counter
+ HALT #5 at > 2 / milestone.

## 8. Naming + tagging (unchanged)

Per V0 charter §5:
- `v0.3.0` tag: M20 close (or whichever M-x close defines V0.3 done)
- Internal milestone tag only
- No GitHub Release publish

If V0.3 close + V1.0 ship gate both met:
- `v1.0.0` tag follows V0.3 close merge
- GitHub Release publish at v1.0.0 (first public release)

## 9. Closing principle (unchanged)

> "Reality > schedule"
> — V1 spec §1, V0 charter §8

V0.3 takes as long as quality demands. No calendar commitment.
Each milestone closes when its hard-stop criteria met.

V0.3 + V1.0 ship occur when reality says ready, not when calendar
says due.
