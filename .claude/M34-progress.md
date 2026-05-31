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

## P2 S2 — trend + rate  ✅
Parsed table gains two §7.3 columns: **Trend** (a mini-sparkline of the last ~48 samples, painted by a
`SparklineDelegate` from a normalized `QPolygonF` + identity colour stored on the cell) and **Rate** (Hz,
measured from `queryLatest` timestamps). New layout: Name · Trend · Quality · Source · Value · Unit · Rate ·
Type · Age · Dashboard. `rate` is now a filterable field too. Test extended (rate populated + sparkline
polygon built). 724/724 ctest Debug + Release.
- **Visual baselines:** the animated Parsed view (sparklines + faster 16 ms publish) made 5 live-data states
  (22/27/33/36/46 — recording/replay/buffer) non-deterministic in the table region. Regenerated the
  Parsed-showing baselines and **masked the full table** for those 5 (the assertion there is the
  dialog/status chrome, not the live table) — sanctioned per visual-diff-contract §1 Step 3, stable over 2
  runs. **Systemic note:** the redesign's live-animated Parsed view is fundamentally at odds with pixel
  baselines for any state that shows it with flowing data; the real fix is for the harness to seed
  deterministic/frozen data (or not capture Parsed for those states) — flagged in the mask `review_at`.

### Still owed in P2
- Selecting a row driving the **right inspector** (signal stats) → inspector wiring, **P5**.

## P2 S3 — last-change column + group-by-driver  ✅
Closes out the P2 remainder.
- **Changed column** (after Age): time since the signal's *formatted value last differed* — a stuck signal's
  Age stays small (still arriving) while Changed grows, surfacing frozen feeds. Tracked per-id in a
  `changeTracker_` that survives row rebuilds. Filterable as `changed` (seconds), e.g. `changed > 5`.
- **Group by driver** toggle (header): clusters rows by `source` under a bold, full-width, non-selectable
  **driver header row**. Needed a table-row↔data-index remap (`RowData.tableRow` + `tableRowToData_`) since
  header rows break the old 1:1 invariant; `refresh`/`applyFilter`/`showRowMenu`/`visibleRowCount` all route
  through it, and an empty group's header hides when a filter removes all its rows.
- Also unified earlier: a signal's **own name is a filter field** (`temperature > 10`, parity with Raw —
  live-check fix `fa79d6d`), and the stale `source == udp:rig` placeholder replaced.
- New layout: Name · Trend · Quality · Source · Value · Unit · Rate · Type · Age · **Changed** · Dashboard.
- Tests: grouping (header rows inserted, signal count unchanged, empty-group header hides, toggle off) +
  Changed column populated/filterable. Parsed-showing visual baselines regenerated for the new column.

### P2-S3 follow-up — live-check UX refinements  ✅
Owner-requested after a live UDP session:
- **Parsed column show/hide:** right-click the table header → a checkable menu per column (Name pinned).
  `buildColumnMenu()` (testable) + `showHeaderMenu`.
- **Parsed columns resizable:** header sections switched from Stretch/ResizeToContents to **Interactive**
  with explicit initial widths + `stretchLastSection` — column borders now drag like the Raw view's.
- **Raw "Info" → "Data":** the packet-list last column renamed (enum `kInfo`→`kData`, header label).
- Test: header menu toggles a column's visibility, Name stays pinned. 18 Parsed-showing visual baselines
  regenerated for the new column widths (deterministic no-data capture — the live stream had to be paused;
  it contaminated a first regen with transient values, reaffirming the systemic note above). 11/11 visual.

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
frame-rate-bound. Result: **more** janky → it's per-refresh *work*-bound, not rate-bound. Reverted to 15 Hz.

**B-3 — dashboard jank (measured).** Instrumented `PlotView` under xvfb/software raster:
`recompute_avg_us ≈ 6` (so the LOD/query change is **not** the cost — B exonerated), `paint_avg_us ≈ 650`
per plot (≈ **320** with antialiasing off). The cost is the software-raster paint, dominated by global
antialiasing applied to fill/grid/text. Two no-visual-change fixes:
  - **Scope AA to the data polyline only** — fill/grid/text/border are axis-aligned, so AA there is wasted;
    halves per-frame paint (650 → ~320 µs). 723/723 incl. visual (axis-aligned 1 px lines are identical
    AA-on/off; the trace keeps AA).
  - **Skip the repaint when the newest sample is unchanged** — live data publishes in ~100 ms bursts, so at
    15 Hz most ticks re-stroked an identical frame; `PlotView::refresh` now `update()`s only when
    `queryEnd_` advances.
  - Net: paint work down ~4× (½ cost × ½ frequency). Remaining stutter, if any, is the 100 ms publish
    cadence (data advances in steps) and/or the software compositor — next levers if still janky.

**B-4 — 60 Hz waveform (owner request).** After B-3 the paint is cheap, but 15 Hz still looked choppy. Owner
asked to try 60 Hz. Bumping the refresh timer alone does nothing — the buffer only published new data to
readers every 100 ms (so the plot advanced ~10 Hz regardless). Two coupled changes:
  - **Buffer publish flush 100 ms → 16 ms** so a live 50 Hz signal becomes visible to readers ~per sample
    (≈50 Hz) — the plot then advances at the data rate, smoothly.
  - **Count cadence now fires on the ABSOLUTE push count** (`pushCount_ % cadence`), not "samples since
    last publish". The frequent time-flushes used to reset the count window, so a push-then-stop left the
    final sub-16 ms batch unpublished (broke 4 LOD tests: 198 vs 200 bins, etc.). Absolute count makes the
    final batch always publish; the time-flush is now purely additive. Removed the dead
    `pushesSincePublish_` member. (Writer bench unaffected — it's count-dominated, microseconds apart.)
  - **Dashboard refresh 15 Hz → 60 Hz**; `skip-repaint` still gates on `queryEnd_`, so it paints at the
    data rate (~50 Hz), not a fixed 60. 723/723 ctest Debug + Release. **Owner confirmed smooth.**

**B-5 — configurable refresh rate (owner request).** The rate is no longer a hard-coded constant:
`Dashboard::setRefreshRateHz` / `refreshRateHz()` (default 60, clamped [1, 144]) + a **View → Refresh rate**
menu (15 / 30 / 60 Hz). 724/724 ctest Debug + Release. Future: per-panel rates (video panels will run at
their own cadence — see [[heterogeneous_frame_rates]] / proposal §10).

---

# Phase 3 (P3) — Raw Wireshark dissection (§7.2)

The Raw tier (原报文) becomes a real packet dissector: packet list → schema-driven dissection tree → hex
pane with byte-range highlighting + field-level display filter. Deferred from M31 (the M31 Raw view was
list + hex only).

## P3 S1 — dissection core (`FrameDissector` + shared `field_codec`)  ✅
- **`decode/field_codec.hpp`** (new, header-only): the byte-reading primitives (`readUnsigned`/`signExtend`/
  `readFloat32`/`readFloat64`, encoding classifiers, `layoutMatches`) extracted out of `schema_decoder.cpp`'s
  anonymous namespace into one shared, inline source of truth. **`SchemaDecoder` migrated onto them** (its
  private `layoutMatches` now delegates) — no public-interface or schema change, so the M5 freeze holds; the
  point is that the live decode path and the UI dissector can never disagree on how a byte becomes a value.
- **`decode/frame_dissector.{hpp,cpp}`** (new): `FrameDissector(Schema)` → `dissect(payload)` returns a
  `Dissection{matched, layoutName, diagnostic, payloadSize, fields[]}` where each `DissectedField` carries
  `name / byteOffset / byteLength / bitStart / bitCount / rawHex / value / unit / typeLabel / truncated /
  children`. Bitfields expand into bit-slice children; the magic fingerprint is a synthetic `(magic)` node so
  every pinned byte is accounted for. **Unlike the decoder it does not bail on a malformed frame** — it
  dissects every in-bounds field and marks the rest `truncated`, because a short/garbled frame is exactly
  what the Raw view exists to explain.
- Tests: 8 cases (per-field byte ranges + decoded values, bitfield children, big-endian + signed
  sign-extension, float32 + fixed string, multi-layout first-match, unmatched/empty/malformed diagnostics).
  732/732 ctest Debug + Release; clang-format clean. Commit `bbfd1ab`.

## P3 S2 — dissection tree UI + byte highlighting  ✅
- **`RawPacketView` → three panes** (list │ dissection tree │ hex, a vertical splitter 4:4:3). Selecting a
  packet dissects it into a **Field · Value · Type · Bytes** `QTreeWidget` (bitfield children nested, magic
  node first); selecting a tree node **highlights that field's byte range in both the hex and ASCII columns**
  of the dump (extra-selection spans recorded as the dump is built) and scrolls it into view.
- **Decoupled like the Parsed identity**: the app injects a `DissectorProvider` mapping a frame's `source` to
  its dissector. `MainWindow` builds one `FrameDissector` **per driver type** (the source's `:`-prefix —
  `udp`/`serial`/… — since a frame's self-reported `source` is `udp:127.0.0.1:port`, not the connection
  driverId) from the connection's `decoderSchemaId` on connect, mirroring the `DecoderRegistrar`'s per-type,
  last-config-wins model. With no schema for a source the tree shows a raw-bytes placeholder; the list + hex
  panes still work (the Raw view never hard-depends on a decoder). `inspect` now links `signalforge_decoder`
  (PUBLIC, no cycle).
- Tests: 2 interaction cases (tree built with decoded values + byte-range highlight on field select;
  placeholder when no dissector). **734/734 ctest Debug + Release, 11/11 visual** — Raw is not a captured
  default state (landing is Parsed), so **no baseline change**. clang-format clean. Commit `d67914d`.

## P3 S3 — field-level display filter  ✅
- The Raw filter bar now resolves **dissected field names** in addition to packet metadata: `temperature >
  20`, `status.alarm == 1`, dotted `parent.child` for bit slices (both `alarm` and `status.alarm` resolve).
  Values are typed to match `ParsedSignalsView` (bool for true/false, double when numeric, else string).
- **Lazy**: a filter that only references builtins (`len`, `source`, …) never pays for dissection; the
  per-row field map is filled on the first non-builtin lookup and reused across the expression. Cost is
  bounded (~µs/frame × visible rows, only when a field filter is active).
- Test: filter narrows on a dissected numeric field, combined builtin + field, clear restores. Builtins win
  on name collisions (documented). 734/734 ctest Debug + Release (non-visual; S3 changes no rendered output).
  clang-format clean.

### Still owed (later phases, per proposal §12)
- **P4** Dashboard view/edit + widget registry (+ unify dashboard colours onto `SignalIdentity`).
- **P5** right-inspector / bottom-drawer selection wiring (frame slots exist but mount empty) + top-bar
  connection chip. The Raw dissection tree is a natural P5 inspector feeder (select a field → inspector).
- **P2 remainder**: last-change column, group-by-driver.

---

# Phase 4 (P4) — Dashboard colour unification (§7.4)

## P4 S1 — unify panel colours onto SignalIdentity  ✅
A signal was a *different* colour in each tier: Parsed used the shared `SignalIdentity` palette, PlotView used
a local round-robin `kPalette` (reset per plot, unstable), MeterView used one hardcoded fill. P4-S1 routes
**all** panel colours through the same identity provider, so a signal's plot trace / bar / gauge fill matches
its Parsed swatch.

- New `SignalColorProvider` alias (`panel_types.hpp`): `std::function<QColor(const QString&)>`. `Panel` gains
  a virtual `setSignalColorProvider` (no-op; Numeric/State/Table show no colour). **PlotPanel** forwards it to
  `PlotView::setColorProvider` (resolves new series + re-colours existing ones on re-set — theme-change path);
  **MeterPanel** forwards the bound signal's colour to `MeterView::setColor` (applied on set + each refresh).
- `Dashboard::setSignalColorProvider` stores + forwards to every current/future panel. **MainWindow injects
  the same lambda it gives ParsedSignalsView** (`signalPaletteColor(signalIdentity_.colorIndex(id))`), so the
  SSOT is shared — no new dashboard→workbench/theme dependency (colour is injected, like the Parsed view).
- Fallbacks preserved: no provider → PlotView's round-robin, MeterView's default accent.
- Tests: PlotView resolves + re-colours via provider; Dashboard forwards to a plot panel (new + on re-set).
  **Zero visual-baseline impact** (the identity palette matches the old plot colours; 11/11 visual unchanged).
  728+/... ctest Debug + Release; clang-format clean.

**Deliberately NOT done (avoided over-abstraction):** a standalone panel "widget registry" / factory class —
the existing `makePanel()` switch + `PanelConfig` + `PanelConfigDialog` (type/signals/range/unit/decimals) +
free-form drag/resize already cover add/configure/remove, so a registry would be abstraction before a second
consumer. Revisit only when a plugin/serialization need appears.

### Still owed
- **P5** right-inspector/bottom-drawer selection wiring + top-bar connection chip.
