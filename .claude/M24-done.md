# M24 — Dashboard P3 (closure report)

Branch: `milestone/M24` (local, off `milestone/M23`; **not pushed**). Implements P3 of
`docs/v0.3/dashboard-interaction-design.md` — Bar and Gauge panels.

## What shipped

| Deliverable | Where | Tests |
|---|---|---|
| `MeterView` — QPainter horizontal bar / 180° arc gauge with needle | `src/dashboard/meter_view.*` | (via meter_panel) |
| `MeterPanel` — single-signal Bar/Gauge; per-panel or observed range | `meter_panel.*` | meter_panel_test (3) |
| `PanelType::Bar` / `PanelType::Gauge` + name round-trip | `panel_types`, `panel.cpp` | panel_factory_test |
| `Dashboard::addBarPanel` / `addGaugePanel` + addPanel handling | `dashboard.*` | dashboard_test (+1) |
| MainWindow `+ Bar` / `+ Gauge` toolbar + `--auto-add-bar/-gauge` harness | `main_window.*`, `main.cpp` | full ctest + smoke |

Commits: `(plan)` → `613df75` S1 → `<S2>` → (this) close-out.

## Verification

- Debug + Release build green; meter_panel_test (3) + dashboard_test (5) green.
- GUI release smoke Tier A + Tier D green.
- Live visual (/tmp): Numeric + Bar + Gauge rendered together — bar track with fill, gauge arc
  + needle + readout, theme-aware; clean exit (rc=0).
- clang-format clean; clang-tidy matches module baseline.
- `00-empty-launch` rebaselined for the two new toolbar buttons (`+ Bar`, `+ Gauge`).

## Decisions (see M24-understanding.md)

- **D1** Bar/Gauge opt-in via toolbar (not auto-suggested — range unknown when a signal is
  first ticked); `+ Bar`/`+ Gauge` bind to the first registered signal (matching `+ Table` D1).
  Per-panel signal/type selection is a follow-up.
- **D2** Range = per-panel `rangeMin`/`rangeMax`, else observed min/max (design §2.1) — no
  frozen `SignalMetadata` change.
- **D3** One shared `MeterView` (Bar|Gauge style) hosted by `MeterPanel`.

No frozen interface modified.

## Status

P3 implemented and green on `milestone/M24` (local, unpushed). Chain:
`main → M21 → M22 → M23 → M24`. Next: **C1** — the M6 buffer's slow-signal publish latency.
