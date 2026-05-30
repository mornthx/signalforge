# M34 — progress (UI redesign Phase 1: the frame)

## S1 — component primitives  ✅
- `src/workbench/components/` (module now links Qt6::Widgets): the reusable building blocks the frame is
  composed from —
  - **ActivityRail** — vertical exclusive mode buttons (Connect/Inspect/…); `addMode`/`setCurrentMode`/
    `modeSelected`. The signature left-nav.
  - **StatusChip** — labeled pill with a semantic `state` property for QSS (connection/mode/quality).
  - **SectionHeader** — title + caption + trailing actions; one header for every surface.
  - **EmptyState** — title/caption/action buttons; reused for onboarding + per-context empties.
- Additive, not yet mounted → zero app risk. Tests: 4 cases / 17 assertions; clean xcb teardown
  (leaked-QApplication pattern). 715/715 ctest Debug + Release; fmt clean.

## S2 — design tokens v2  ✅
- `tokens.json` v1.1 → **v1.2** (additive): one new semantic colour token **`accent`** — the brand accent
  for primary actions + active navigation (proposal §8). Aliases `border.focus` per-theme today
  (light #3b7ddd / dark #7fb2ff / hc #00e5ff); a separate token so a future rebrand is a one-line change.
  Documented in `_manifesto_refs` + `_notes.v1_2_design_decisions` (mirrors the existing signal/status
  overlap precedent).
- `tools/generate_style_assets.py` `gen_qss()` extended with a **Workbench frame** section, generated for
  all three themes: `#workbenchTopBar`, `#activityRail` + `#railButton` (left-accent on `:checked`),
  `#segmentButton` (bottom-accent on `:checked`), `#workbenchInspector`, `#workbenchDrawer`, `#statusChip`
  (rounded pill, `[state=…]`-coloured label), `#emptyState`. objectName-scoped → matches only the new
  frame; **zero pixel change** to the current app (none of these objectNames are mounted yet).
- Regenerated all 5 consumers (qss ×3, hpp, py). Gates green: `--validate`, `--check` fresh, `lint_qss`
  clean, `test_qss_linter` 18/18. **715/715 ctest Debug + Release** (all 11 visual baselines unchanged).

## S3 — WorkbenchFrame shell (rail | content | inspector | drawer)  ✅
- **SegmentedControl** (`components/segmented_control.{hpp,cpp}`) — horizontal exclusive `#segmentButton`s
  that switch a sub-view within a mode (Raw / Parsed / Dashboard inside Inspect). Mirrors `ActivityRail`'s
  API on the horizontal axis: `addSegment`/`setCurrentSegment`/`segmentSelected`.
- **WorkbenchFrame** (`workbench_frame.{hpp,cpp}`) — the redesigned shell, a pure layout container with no
  business logic:
  - top bar (`#workbenchTopBar`): title + trailing chip/action region (`setTitle`, `addTopBarWidget`).
  - left `ActivityRail` → `QStackedWidget` content; `addMode(id,label,content)`, `setCurrentMode` (silent),
    `modeChanged` on user click. First mode auto-current; duplicate ids ignored.
  - right inspector (`#workbenchInspector`) + bottom drawer (`#workbenchDrawer`): `setInspector`/`setDrawer`
    (content swap, old deleteLater'd) + `set*Visible`; both hidden until shown. Horizontal + vertical
    `QSplitter`s give resizable content|inspector and middle|drawer with non-collapsible panes.
- Additive, **not yet mounted** → zero app risk. Tests: frame 4 cases (mode swap + emit, silent set,
  inspector/drawer show-hide + content, duplicate-id) + SegmentedControl 1 case. **720/720 ctest Debug +
  Release**; clang-format clean; clean xcb teardown (leaked-QApplication).

## S4 — integrate into MainWindow + regenerate baselines  ✅
The activity-rail frame is now the application's central widget; the QTabWidget/QStackedWidget workspace
and the left connection dock are gone.

- **Frame mounted:** `WorkbenchFrame` is the central widget. Rail = **Connect · Inspect** (v1 per §11).
  - **Connect mode** (§7.1): a `QStackedWidget` — page 0 the onboarding empty-state, page 1 the connection
    manager (the legacy left `QDockWidget` is dissolved into this mode; `connectionManagerBody_`). No
    connection → onboarding + park on Connect; first connection → reveal the manager + move to Inspect.
  - **Inspect mode** (§6/§7): a `SegmentedControl` **[Raw | Parsed | Dashboard]** over a `QStackedWidget`
    of the existing views, default landing Parsed.
- **Dashboard toolbar relocated** (§7.4, owner point #1): the +Plot/+Table/+Bar/+Gauge / Live / time-range
  toolbar moved off the global `addToolBar` into a **dashboard-local toolbar** inside the Dashboard segment
  page — it only appears with the Dashboard view.
- **Switch plumbing rewired:** `showWorkspaceTab` / `ensureDashboardVisible` drive the segmented control +
  force Inspect mode; `updateEmptyStateVisibility` drives the Connect stack + rail mode; the
  connection-status-strip click and the M19 harness now switch to Connect mode (dock is gone).
- **Baselines:** all 48 visual states recaptured and accepted wholesale (PNG + env sidecar). Per-theme
  accent verified (light blue / dark blue / high-contrast cyan). **720/720 ctest Debug + Release**;
  clang-format clean.

### Deferred (later phases, per proposal §12 — NOT regressions)
- **Parsed "on dashboard" marker** (§7.3, owner point #2) → **P2**: needs Dashboard→Parsed membership
  wiring; out of frame-integration scope. Still owed.
- **Right inspector + bottom drawer** are built into the frame but mounted **empty/hidden** — selection-
  driven content (packet fields / signal stats / widget config) is **P5** (§9).
- **Top bar** carries the app title only; the connection chip / clock / palette / settings (§6) are a later
  refinement — the existing bottom status strip is retained for now (coexists below the frame).
- **Connect panel on config-save-failure with 0 connections** shows onboarding (not the empty manager +
  banner); the failure is still surfaced in the status strip. Acceptable IA consequence of onboarding
  living in Connect; revisit if the banner needs prominence.

---

# Phase 2 (P2) — Parsed identity UI (§7.3)

## P2 S1 — swatch + quality badge + on-dashboard marker  ✅
The Parsed table (Tier 2) becomes a real signal browser. Columns are now: identity swatch + **Name ·
Quality · Source · Value · Unit · Type · Age · Dashboard**.

- **Identity swatch:** a per-signal colour square (DecorationRole) from the shared `SignalIdentity`
  palette index resolved against the active theme — the SSOT colour, consistent across views (dashboard
  unification is later, P4).
- **Quality badge:** `Good/Stale/Uncertain/Bad` (from `qualityFromAge`, 1.5 s / 5 s thresholds; no data →
  Bad), coloured green/amber/red per the active theme's status tokens.
- **"On dashboard" marker** (owner point #2): a "● on" marker per row driven by `Dashboard::showsSignal`,
  and the **row action flips** — right-click shows *Remove from dashboard* when the signal is already
  shown, else the *Add to dashboard ▸ <type>* submenu. Removal routes to `removeSignalEverywhere`; markers
  refresh immediately on `panelsChanged`.
- **Filterable:** `quality` and `dashboard`/`on_dashboard` are now display-filter fields too (Wireshark
  direction) — e.g. `quality == bad`, `dashboard == true`.
- **Architecture:** `ParsedSignalsView` stays in `inspect` and depends on neither the theme nor the
  dashboard — the app injects three providers (`setSignalColorProvider` / `setQualityColorProvider` /
  `setDashboardMembershipProvider`). `MainWindow` owns the shared `SignalIdentity` (SSOT). `inspect` now
  links `workbench` (for `Quality`/`qualityFromAge`).
- Tests: 2 new interaction cases (swatch consulted + decoration set, quality text, marker reflects
  membership, both fields filterable; menu Add/Remove flip + remove emission). **722/722 ctest Debug +
  Release**; clang-format clean.
- Baselines: the Parsed-showing states recaptured + accepted (new columns render correctly — distinct
  swatches per signal, quality badges, empty dashboard markers). Widened the recording-status masks for
  states 27/46 (the activity-rail reflow shifted the runtime recording counter past the old mask edge —
  status-bar runtime content, unrelated to the table; pending owner review with the rest of the regen).

### Still owed in P2 (next subtasks)
- **Mini-sparkline** of recent trend per signal (§7.3).
- **Rate** (Hz) and **last-change** columns; **group-by-driver**.
- Selecting a row driving the **right inspector** (signal stats) → that's the inspector wiring, **P5**.

## P2 live-rendering fixes (owner-observed, A + B)  ✅
Live UDP observation surfaced two rendering bugs:

**A — dashboard jank (my P2 regression).** `ParsedSignalsView`'s 10 Hz refresh did full per-row work even
while hidden behind the Dashboard segment. Gated the timer tick on `isVisible()` (direct `refresh()` calls
unaffected) + refresh on `showEvent`. Commit `ac7b38e`.

**B — plot waveform corruption (triangles / freeze / ¼-glitch).** Root cause: `SignalBufferConfig::
estimatedRateHz` was **never populated in production** → defaulted to **1000 Hz** in `selectLodLevel`, so
every live window selected LOD level 3 (bin = 1000 samples = 20 s of 50 Hz data). A 1-min plot collapsed to
~3 min/max bins ("two triangles," frozen ~20 s); short windows flickered as 20 s bins intermittently
overlapped.
  - Fix: **count-based LOD selection** — `selectLodLevel(windowSampleCount, target)` picks the finest
    level whose output (2 pts/bin) fits the `2*target` budget, using the ACTUAL in-window sample count. No
    rate estimate at all; removed the dead `estimatedRateHz_` member (config field retained for compat).
  - **Deviation (recorded):** this recalibrates the LOD level-selection thresholds described in
    `docs/milestones/M6-signal-buffer.md §4.5` (a milestone doc, not `architecture.md`). The old
    density/rate thresholds are superseded; behaviour is now count-driven. Tests updated:
    `signal_buffer_query_test` (target 100→200 pts @ L1, target 1000→1000 raw, L3 envelope forced via
    target 1) and the S9 `test_chart_lod_selection` (full window → L2 ~1200 pts, fills the budget instead
    of collapsing to ~200).
  - **Benchmark (perf path, §5.4 #2):** reader `queryRange(60 s/1 kHz, target 2000)` **337 k → ~107 k
    queries/s**. Intentional: the old path was fast only because LOD 3 returned ~120 points (the bug); the
    new path returns ~1200 (faithful). Still **10.6× over the ≥10 k/s target**; the live app needs ~120/s.
  - 722/722 ctest Debug + Release; **no visual-baseline impact** (baselines capture static states, not
    live waveforms). Plot's empty→raw fallback retained for sparse/recent data.

**B-2 — LOD "sawtooth" after B.** Once B made 1-min windows actually use LOD (level 1), the plot drew a
sawtooth: the min/max envelope was emitted at fixed positions (min @ `t_start`, max @ `t_end`) regardless
of where the extremes actually occurred — on falling segments that inverts the two points.
  - Fix: each `LodBin` now records `t_min`/`t_max` (the timestamps of the min and max samples, tracked by
    index in the writer's bin builder — read twice per bin, not per sample). `queryRange` emits the two
    extremes at their real times in chronological order, so a connected polyline traces the true envelope.
    Test ramps are monotonic so their assertions are unchanged; added a falling-bin test asserting
    max-before-min ordering. S10 envelope check now takes min/max by value, not position. LOD-memory
    overhead factor 1.11 → 1.22 (the bin grew ~1.5×); S7 estimate-vs-actual stays within 20%.
  - **Benchmark:** writer (double) ~9.6 M → ~7.2 M samples/s idle (the larger bin), still ~15× over the
    ingest target; reader ~117 k q/s. 723/723 ctest Debug + Release.

**Dashboard 30 Hz (`a70232d`).** Owner experiment: panel refresh 15 Hz → 30 Hz to test whether the jank is
frame-rate-bound. Trivially reverted.
