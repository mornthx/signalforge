# M20 Spec — Interactive States + Theme Variants

Source: `docs/V0-charter-amendment-v0.3.md` §3, M20.

## Goal

Ship the V0.3 theme and keyboard-interaction layer on top of the
M16-M19 light-theme UI foundation:

1. Dark theme palette built through the M16 token generator.
2. High-contrast accessibility variant.
3. Keyboard navigation completeness for primary chrome.
4. Visible tab-order focus rings.
5. Runtime theme toggle.

## Acceptance

- `resources/styles/tokens.json` contains `light`, `dark`, and
  `high_contrast` themes and `tools/generate_style_assets.py --check`
  proves generated consumers are fresh.
- `SignalForgeStyle` can apply Light / Dark / HighContrast at startup
  and runtime without bypassing the palette + QSS rendering contract.
- `MainWindow` exposes a View -> Theme menu and keyboard shortcuts for
  the three variants.
- Primary keyboard path has explicit tab order and visible focus rings.
- Visual states capture dark, high-contrast, and focused-control variants
  under the existing M15/M16 visual harness.
