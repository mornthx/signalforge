# M20 Progress

- Expanded `resources/styles/tokens.json` to v1.1 with `dark` and
  `high_contrast` themes.
- Updated token schema and generator to emit:
  - `resources/styles/tokens.qss`
  - `resources/styles/tokens-dark.qss`
  - `resources/styles/tokens-high-contrast.qss`
  - multi-theme `src/app/generated_style_tokens.hpp`
  - multi-theme `tests/visual/lib/generated_tokens.py`
- Implemented `SignalForgeStyle::Theme::{Light,Dark,HighContrast}` with
  runtime palette/QSS reapplication and startup `--theme` support.
- Added View -> Theme menu with `Ctrl+Alt+1/2/3` shortcuts.
- Added explicit primary tab order and focus-ring visual hook.
- Made `QQuickWidget` chart clear color and chart grid/border colors follow
  the active palette.
- Added `tests/visual/tests/test_states_m20_themes.py` and baselines:
  - `40-m20-dark-theme`
  - `41-m20-high-contrast`
  - `42-m20-focus-live-toggle`
