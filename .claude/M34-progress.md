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

## S4 — integrate into MainWindow + regenerate baselines  ⏳ (next — the risky step, last)
