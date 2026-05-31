# M26 — DR-001 IA rewrite (understanding)

Fixes the remaining DR-001 findings: **#4 menu-bar** (built piecemeal, order
`Connections|Session|File|View` with File 3rd; Record vs Open-Session split across menus) and
**#5 status-bar accretion** (per-milestone insertWidget; redundant "label: value" cells like
"Recording Record idle", "Buffer Buffer 0%"). Branched off `milestone/M25`.

## Delivers
- One owned `buildMenuBar()`: `File | Connections | Session | View | Help`; Record + Open Session
  co-located under Session; Help → About. Menu names kept stable for the visual harness.
- One owned `buildStatusBar()`: cells in a deliberate order with concise values (no doubled
  words), no idle false-warnings.

## Decisions (autonomous, logged)
- D1 Menu order File|Connections|Session|View|Help; Open Session moved File→Session to fix the
  split. Names unchanged (harness `autoOpenMenu` matches by name).
- D2 Status values drop the redundant prefix already in the cell title.

## DoD
Build+ctest green (visual states rebaselined for the chrome change); clang-format clean; local
commits (no push).
