# M26 — DR-001 IA rewrite (closure report)

Branch: `milestone/M26` (local, off `milestone/M25`; **not pushed**). Resolves the remaining
DR-001 findings: **#4 menu-bar** and **#5 status-bar accretion**.

## What shipped

### #4 — Menu bar (owned, conventional)
One `buildMenuBar()` creates the bar in a single place:
**`File | Connections | Session | View | Help`** (was `Connections | Session | File | View`,
File 3rd, built piecemeal across four `buildXxxUi`). Changes:
- `File`: Quit. `Connections`: Add… / — / Connect all / Disconnect all. `Session`: Record… +
  **Open Session…** (co-located — they were split across Session and File). `View`: Theme.
  `Help`: **About SignalForge** (new).
- Menu names kept stable so the visual harness's `autoOpenMenu` still matches.

### #5 — Status bar (de-doubled)
- Dropped the redundant caption for cells whose value already names itself
  (`Config`/`Recording`/`Replay`/`Buffer`): "Recording Record idle" → "Record idle",
  "Buffer Buffer 0%" → "Buffer 0% 0 MiB", etc. One-line change in `makeStatusCell` (skip empty
  title) + four call-sites — no churn to value strings or the M18/M19 visual-state hooks.
- Hid the always-zero "Drops" cell (the dashboard doesn't drop frames; it read as a standing
  warning at idle).

Commits: `b793b6f` S1 (menu) → `<S2>` (status) → (this) close-out.

## Verification

- Debug + Release build green. **Full ctest 674/674** (only `00-empty-launch` rebaselined for
  the chrome change; all other visual states stayed within tolerance).
- GUI smoke green. clang-format clean.
- Live visual: menu bar `File | Connections | Session | View | Help`; status bar
  "Config ready · Record idle · Replay idle · Mode Live · Chart idle · Buffer 0% 0 MiB";
  `autoOpenMenu("Session")` opens the Record + Open Session menu.

## Notes / decisions

- **D1** menu order/co-location as above; names unchanged for the harness.
- **D2** de-duplication via dropping captions (not editing value strings) — lowest-risk; the
  value strings (and their visual-state hooks) are untouched.
- Pre-existing harness limitation observed (not introduced here): the menu-open visual states
  (30/31/32) capture the main window but not the popup (a separate top-level window), so menu
  *content* changes aren't pixel-tested. The menu bar + actions were verified live + via
  `autoOpenMenu`.

## Status — DR-001 fully addressed

With #4 + #5 done, **all DR-001 findings are resolved**: value/state display (P0–P3), readable
plot (P2), slow-signal latency (C1), teardown crash (C2), and now menu + status-bar IA.
Remaining nice-to-haves (not DR-001 blockers): drag-to-add signals, per-panel signal/type
selection UI. Chain: `main → M21 → M22 → M23 → M24 → M25 → M26`, all local/unpushed.
