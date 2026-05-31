# M22 — Dashboard P1 (understanding)

## Source

Executes **P1** of `docs/v0.3/dashboard-interaction-design.md`: the **Table / DataGrid panel**
(design §3.3) — "a Table bound to a whole driver answers 'is this device sane?' in one card —
the single most useful bring-up view." Continues the dashboard work from P0 (M21). Branched off
`milestone/M21` (P0 + the C2 teardown fix), since M21 is not yet merged.

## What P1 delivers

1. A new `TablePanel`: a multi-signal card showing one row per signal —
   **Signal · Value · Unit · Updated(age)** — read from `SignalBuffer::queryLatestOne()`.
2. `PanelType::Table`; `Dashboard::addTablePanel(signalIds)`; a `+ Table` toolbar affordance
   that tables all currently-registered signals.
3. A small refactor: hoist `addSignal` / `removeSignal` / `isMultiSignal` / `detachChart` to
   virtuals on the `Panel` base so the Dashboard handles multi-signal panels (Plot, Table)
   polymorphically — removing the P0 `static_cast` downcasts (and their clang-tidy warnings).

Out of scope (later phases): per-driver auto-grouping UI, column sorting, P2 plot readability,
P3 Bar/Gauge.

## Key facts (reused from P0, unchanged)

- Current value + staleness: `SignalBuffer::queryLatestOne()` → `optional<LatestValue{value,
  timestamp, age}}`. Age → "Updated" column. Same 100-sample publish-cadence caveat as
  M21 concern C1 (slow signals lag).
- `Dashboard::showsSignal` already iterates `hasSignal` across panels, so a Table's signals
  make the signal-list checkboxes reflect table membership for free.
- Panels are QWidgets; TablePanel uses a `QTableWidget` (fully headless-testable).

## Decisions (autonomous, logged for review)

- **D1 — `+ Table` tables all current signals.** Simplest useful "see everything" view for V1.
  Per-driver scoping / drag-to-table is a follow-up. The user can remove rows by unticking
  signals in the list (routes through `removeSignalEverywhere` → Table keeps, drops the row).
- **D2 — Table is "wide"** (full grid row), like Plot, since it lists many signals.
- **D3 — refactor to virtual add/removeSignal on `Panel`** (vs. P0 downcasts). Touches only
  M21 (unmerged, non-frozen) code.

## Definition of done

New `TablePanel` has `tests/unit/dashboard/table_panel_test.cpp` ≥70% public surface; Debug +
Release build & ctest green; clang-format clean; clang-tidy matches module baseline; Doxygen on
public decls; `.claude/M22-progress.md` current; conforming local commits (no push). ASan
CI-authoritative (local host blocked).
