# M32 — IA realignment (closure report)

Owner PM-level feedback: control placement must match scope (recovering architecture §7.1/§7.3, never
built). Branch `milestone/M32` (local, off M31; **not pushed**). Memory: `ia_control_scope_principle`.

## Delivered

- **S1 — promote-from-Parsed; drop the global Signals dock** (`a9c3550`). Removed `SignalListPanel`
  (class + test + mount). The dashboard's signal picker no longer sits globally. In the Parsed tab,
  right-click a signal → "Add to dashboard ▸ Numeric/State/Plot/Bar/Gauge"; `Dashboard::addSignalAs`
  creates the panel (records intent) and switches to the dashboard. Parsed is now the single global
  signal browser. (Owner #1.)
- **S2 — app-level onboarding** (`a9c3550`). The "Start a SignalForge workflow" page moved out of the
  empty-Dashboard into a workspace-level `QStackedWidget`: onboarding when no connection exists, the
  Raw/Parsed/Dashboard tabs once connected. (Owner #2.)
- **S3 — modal per-panel config dialog** (`50d0297`). Right-click panel → "Configure…" opens
  `PanelConfigDialog` editing the full config — type, signals, title, value range, unit override,
  decimals (the finer "细化" fields, previously unreachable). "Remove panel" stays on the menu; the old
  "Show as ▸ / Signals ▸" submenus are gone. (Owner #3.)

## Deviation from architecture (owner-approved)

§7.3 #4 says complex configuration belongs in a **right-hand property inspector, not a modal editor**.
The owner explicitly chose a **modal dialog**. Executed per the owner override and recorded here per the
disagreement-handling rule. (If revisited, the dialog's widgets port cleanly into a right-dock inspector.)

## Out of scope (flagged for next)

- The dashboard toolbar (+Plot/+Table/+Bar/+Gauge, Live, time-axis) is the same mis-placement class as the
  Signals dock; relocating it into the Dashboard tab is the next consistency step.
- Schema-driven Wireshark dissection tree still pending (M33).

## Verification

- 704/704 ctest Debug + Release. `00-empty-launch` visual re-accepted (app-level onboarding, no Signals
  dock). clang-format clean. No frozen interface/schema touched.
- Fixed a Qt 6.10 xcb `qt_call_post_routines` teardown crash (a QDialog triggers it) by leaking the test
  QApplication — see memory `qt_xcb_teardown_crash`.

## Status

Local on `milestone/M32`. Chain: `main → M21 … → M31 → M32`.
