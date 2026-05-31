# M34 — Done report

**Milestone:** M34 — ground-up UI/UX redesign (activity-rail workbench;
Raw/Parsed/Dashboard tiers; right inspector + cross-tier selection).

## PR
- **PR #32** — https://github.com/mornthx/signalforge/pull/32
- Base `main` ← head `milestone/M34`
- Head commit: `0deba99` (`inspect: avoid QCollator leak — case-insensitive compare`)
- Range: 101 commits, 303 files, +21684 / -688
- **Merge SHA: _<placeholder — filled after merge>_**
- **Not merged** — awaiting the owner's merge authorization phrase.

## CI status (head `0deba99`, run 26710642223)
- **Blocking gate GREEN** on all three presets:
  - `build (debug)`: ✅  ·  `build (debug-asan)`: ✅  ·  `build (release)`: ✅
  - Build + ~750 unit/logic tests pass, incl. AddressSanitizer / UBSan / LeakSanitizer.
- **Visual baselines: non-blocking** (this milestone). The M15 full-window pixel-diff
  tests are environment-specific (baselines captured on the operator's machine; CI font
  anti-aliasing differs), so they false-red on CI while the app renders correctly
  (verified by downloading CI's render). They still run + upload screenshots as an
  informational signal; `continue-on-error` keeps the job green. Local visual: **11/11**.
  See `memory/ci_visual_baseline_divergence.md`.

## Scope delivered (P0–P5)
- **P0/P1** activity-rail `WorkbenchFrame` (top bar + rail + segmented Inspect + inspector
  + drawer); retired the QTabWidget workspace.
- **P2** Parsed signal browser — live table, display filter, group-by-driver (collapsible),
  column show/hide + resize, header-click sorting (type-grouped value sort), trend
  sparkline, rate / changed / quality columns.
- **P3** Raw Wireshark dissection — packet list │ schema dissection tree │ hex with
  byte-range highlight; field-level filter; shared `decode/field_codec.hpp`.
- **P4** Dashboard colour unification onto `SignalIdentity`; scrollable surface.
- **P5** right inspector (signal / Raw-field / dashboard-panel; dismissible; per-tier
  restore); cross-tier drill-through + persistent highlight; per-driver colour + recolor;
  signal rename; inline panel range/unit/decimals/size; top-bar connection chip;
  mode-gating.

## Deviations & concerns
- **Visual CI gate relaxed to non-blocking** (owner-directed, 2026-05-31): fixed baselines
  are impractical during the active redesign + the local/CI rendering divergence. Build +
  logic + sanitizers remain the authoritative gate. Re-tighten (capture baselines in the
  CI environment) is a future option.
- **PR exceeds the 800-net-line guideline** — inherent to a cumulative milestone merge; the
  closure flow mandates the milestone PR.

## Next (per CLAUDE.md milestone-closure flow)
Awaiting **Phase 2 (human checkpoint A)**: owner reviews this report and replies
"approved, merge M34 and begin M35 bootstrap" (or literal equivalent) to authorize merge +
tag + M35 bootstrap. CC will not merge before that phrase.
