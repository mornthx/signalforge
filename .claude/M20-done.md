# M20 Done

M20 closes the charter items in `docs/V0-charter-amendment-v0.3.md` §3:

1. Dark theme palette: `tokens.json` v1.1 + generated dark QSS/C++/Python
   consumers.
2. High-contrast accessibility variant: `high_contrast` theme + generated
   high-contrast QSS/C++/Python consumers.
3. Keyboard navigation completeness: primary chrome tab order is explicit
   across connection list, signal selector, live toggle, time preset, replay
   seek, and replay speed controls.
4. Tab order + focus ring visibility: QSS focus rings cover buttons, tool
   buttons, edits, combos, sliders, list/tree widgets; baseline
   `42-m20-focus-live-toggle` verifies the focus state.
5. Theme runtime toggle: View -> Theme menu and `Ctrl+Alt+1/2/3` shortcuts
   call `SignalForgeStyle::setActiveTheme`.

Verification evidence is the current command output from the M20 close run.
