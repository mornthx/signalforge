# SignalForge Widget Styling Guide (V0.3 M16 → M17 foundation)

This document is the engineering reference for **anyone
implementing or refactoring a SignalForge widget** under
V0.3 M17's "widget rebuild" milestone and the M18 workflow
rebuild that follows. It is **not** a tutorial — the
patterns described are already proven in production code
(`src/chart/signal_selector.cpp`,
`src/app/main_window.cpp` panel-header styling,
`src/connection/` status-label dynamic properties); this
guide names them so M17 work can apply them consistently.

It assumes familiarity with `docs/v0.3/visual-identity.md`
(manifesto), `docs/v0.3/visual-diff-contract.md` (test
algorithm), and `docs/v0.3/rendering-environment-lock.md`
(env contract).

## 1. The token-consumption rule

**Every widget colour, font role, and reusable style value
comes from `resources/styles/tokens.json`** via one of the
three generated consumers. Direct `QColor(0x0A, ...)`,
literal hex strings, hard-coded `QFont("Inter", 12)`,
or QSS literal `#3b7ddd` calls in widget code are forbidden.

The three consumers are:

| Consumer | Generated location | Use case |
|---|---|---|
| `tokens.qss` | `:/styles/tokens.qss` (qrc-bundled) | applied once by `SignalForgeStyle::applyGlobalStylesheet`; covers `QPushButton`, `QHeaderView::section`, `QLabel[class=...]`, dialog padding, focus rings |
| `generated_style_tokens.hpp` | `src/app/generated_style_tokens.hpp` (committed; CI-checked for freshness) | C++ widget code reads palette colours / font sizes / spacing constants by symbolic name |
| `generated_tokens.py` | `tests/visual/scripts/generated_tokens.py` (visual-test helper) | test fixtures that need to reference token values (e.g., assert "this label is rendered in `palette.status.connected`") |

Adding or modifying a token: edit `resources/styles/tokens.json`
(the **only** authoritative source), then run
`python3 tools/generate_style_assets.py`. CI gates the
freshness of the three generated files via the M16 token-
freshness step in `.github/workflows/ci.yml`; never edit
generated files directly.

## 2. Where styling lives

Three layers, with non-overlapping responsibilities:

### 2.1 Global stylesheet (`tokens.qss`)

Applied once at app startup by `SignalForgeStyle::
applyGlobalStylesheet`. Covers:

- **All `QPushButton`s** in the app — padding, border-radius, focus
  ring, disabled state. Don't override per-widget.
- **All `QHeaderView::section`s + any `QFrame#panelHeader`**
  — panel-header colour + spacing.
- **`QLabel[class="..."]` dynamic-property selectors** for
  semantic state colours (see §3 below).
- **Focus rings** for keyboard-navigation visibility (manifesto
  §accessibility).
- **`QDialog`** chrome (button bar spacing, close button position).

You can read the active rules by inspecting
`resources/styles/tokens.qss` — that file is the canonical
source for "what every widget gets for free".

### 2.2 C++ palette + per-role widget palette overrides

`SignalForgeStyle::applyLightPalette` populates the 18-role
`QPalette` (Window / Base / Text / Button / Highlight /
HighlightedText / etc.) with values from
`generated_style_tokens.hpp`. Every widget that inherits
from `QWidget` and uses `palette().color(QPalette::...)`
gets the manifesto-tokenised colour automatically.

For per-widget overrides (e.g., a chart pane that wants
`QPalette::Base` to be the chart's plot-area background
colour instead of the default surface colour), call
`widget->setPalette(p)` with the modified palette in the
widget's constructor. Do **not** call
`widget->setStyleSheet("background:#XYZ")` — that defeats
the manifesto and breaks the cross-environment determinism
contract (the captured pixels would no longer be a function
of `tokens.json` alone).

### 2.3 Per-widget QSS class strings (§3)

Reserved for **state-dependent styling** that can't be
expressed in the static stylesheet because the state
changes at runtime.

## 3. The QSS-class pattern (state-dependent styling)

When a widget's appearance depends on a runtime state
(`Connected` / `Disconnected` / `Recording` / `Replay` /
…), use the Qt dynamic property + QSS selector pattern:

```cpp
// C++ side — set the property as the state changes.
void ConnectionStatusLabel::setState(ConnectionState s) {
    const char* cls;
    switch (s) {
        case ConnectionState::Idle:          cls = "status-idle";        break;
        case ConnectionState::Connecting:    cls = "status-connecting";  break;
        case ConnectionState::Connected:     cls = "status-connected";   break;
        case ConnectionState::Disconnecting: cls = "status-disconnecting"; break;
        case ConnectionState::Error:         cls = "status-error";       break;
    }
    setProperty("class", cls);
    style()->unpolish(this);  // force QSS re-evaluation
    style()->polish(this);
    update();
}
```

```css
/* tokens.qss — generated from tokens.json */
QLabel[class="status-idle"]         { color: #5a5d63; }
QLabel[class="status-connecting"]   { color: #d4a72c; }
QLabel[class="status-connected"]    { color: #2d8a3e; }
QLabel[class="status-disconnecting"]{ color: #d4a72c; }
QLabel[class="status-error"]        { color: #c8392a; }
```

The full set of approved class strings is generated into
`tokens.qss` from `tokens.json`'s `semantic_classes` key.
The current set:

| Class string | Used for |
|---|---|
| `status-idle` / `status-connecting` / `status-connected` / `status-disconnecting` / `status-error` | Connection state badges |
| `mode-live` / `mode-recording` / `mode-replay` | Top-bar mode badge |
| `severity-info` / `severity-warning` / `severity-error` | Diagnostic message text colour |

**Don't invent new class strings ad-hoc**. If M17 widget
rebuild needs a new state colour (e.g., `mode-replay-paused`
distinct from `mode-replay`):

1. Add the role to `resources/styles/tokens.json`'s
   `semantic_classes` map (one short, lowercase,
   hyphen-separated name).
2. Add the colour role to the same `tokens.json` (under
   the appropriate semantic palette).
3. Add a sentence in
   `docs/v0.3/visual-identity.md` describing the role
   (manifesto-first per R11).
4. Run `python3 tools/generate_style_assets.py`.
5. CI's token-freshness gate confirms the three generated
   files are in sync; the visual-diff gate ensures the new
   class doesn't break existing baselines (state captures
   that already used the affected role will need
   re-baselining via `scripts/accept-baseline.sh` per R8).

## 4. The `objectName` pattern (layout-stable widgets)

When a widget needs styling that QSS can target by **name**
(rather than by class), use `setObjectName`:

```cpp
panelHeader_ = new QFrame(this);
panelHeader_->setObjectName(QStringLiteral("panelHeader"));
```

```css
/* tokens.qss */
QHeaderView::section, QFrame#panelHeader {
    background: #ECEEF1;
    color:      #1A1B1D;
    padding:    4px 8px;
    border:     none;
    border-bottom: 1px solid #D8DBE0;
}
```

When to use `objectName` vs `class`:

| Use `objectName` | Use `class` |
|---|---|
| Widget identity is fixed by its position in the layout (the connections-panel-header, the main-status-bar) | Widget identity is fixed but its **appearance** changes per state (a label that's `status-idle` then `status-connected`) |
| One-off targeting in QSS or in tests | Multi-state targeting |
| No state changes that affect styling | State-dependent styling |

`objectName` also helps tests + tooling: `xdotool`, AT-SPI,
visual-test framework, and forensic debugging can target a
widget by name without grovelling through the widget tree.
The existing convention in `src/app/main_window.cpp` is to
use camelCase for `objectName` (matches Qt's own convention:
e.g. `connectionsDock`, `panelHeader`).

## 5. Mask conventions (for visual baselines that include
dynamic regions)

If an M17 widget displays a runtime live counter, current
timestamp, FPS, scrubber position, or any other content that
**legitimately differs across captures** (cross-host or
cross-configuration), the V0.3 baseline contract requires a
per-baseline `<state>.mask.json` covering the dynamic region.

The current universal status-bar mask (per S6.6 + S7
follow-up) at `tests/visual/baselines/<state>.mask.json`
covers `x=615, y=778, w=320, h=22` (the 9 baselines where
the status-bar bottom-right area is visible). Schema:

```json
{
  "regions": [
    {
      "x": 615, "y": 778, "w": 320, "h": 22,
      "rationale": "Status-bar live counters — runtime
                    throughput-dependent values (FPS /
                    Dropped / throttled / buffer % / MiB)…",
      "approved_by": "operator@2026-05-12 (S6.6 Phase 4 —
                      universal R8 single-approval)",
      "review_at": "V0.4 keystone or status-bar
                    architecture redesign milestone"
    }
  ]
}
```

If M17 adds a new dynamic region:

1. **Capture the baseline first**, observe the diff
   bounding box (the same workflow we used at S6:
   `compare_with_contract(emit_diff_image=True)` then
   inspect the `.diff.png`).
2. **Author the mask file** with `x/y/w/h` bounding the
   diff region with a small safety margin. Include
   `rationale` (concrete, names the dynamic mechanism),
   `approved_by`, and `review_at`.
3. **Get R8 per-state operator approval** before
   commit. For widely-applicable patterns (like the
   status-bar mask), a single universal approval can
   cover the pattern across multiple states (see
   `docs/v0.3/s6-cross-env-verification.md` §11 for
   the universal-pattern precedent).
4. `tests/visual/baselines/<state>.mask.json` is
   auto-discovered by `compare_with_contract`
   when the test runs against `tests/visual/baselines/
   <state>.png`. No code change needed.

**Don't mask to make a test pass.** The mask exists to
exclude inherently-dynamic content; if a baseline diff
isn't in a dynamic region, masking it is a R12 governance
violation. Investigate the root cause first; mask only
when the diff is in a runtime live-counter or
known-dynamic region.

## 6. Env-sidecar contract (R14 inheritance)

Every M16+ visual-baseline capture **automatically**
emits a 4-tier env sidecar alongside the PNG when the
binary is invoked with `--capture-screenshot-path` or
`--capture-fullscreen-path` (M16 S5 wiring in
`src/app/main.cpp` and `SignalForgeStyle::
dumpEnvironmentJson`). No code change required for M17
captures.

The env-sidecar fields the visual-diff contract enforces
(per `tests/visual/lib/compare.py:ENV_CONTRACT_REQUIRED_KEYS`):

```
tier_1_font_cascade.app_default_family   = "Inter"
tier_1_font_cascade.app_default_size_pt  = 12
tier_1_font_cascade.app_mono_family      = "JetBrains Mono"
tier_2_qt_rendering.qt_version_major_minor   = "6.10"
tier_2_qt_rendering.qpa_platform             = "xcb"
tier_2_qt_rendering.style_object_introspection = "Fusion"
tier_2_qt_rendering.wayland_disallowed       = true
tier_2_qt_rendering.gpu_rasterization_disallowed = true
tier_3_geometry.device_pixel_ratio = "1.0"  (string, not number)
tier_3_geometry.screen_geometry    = "1280x800"
tier_3_geometry.locale             = "C"   (per S6 R14 locale pin)
```

If M17 changes any of these (e.g., introduces HiDPI
support in M20 raising `device_pixel_ratio` to `2.0`),
the contract changes too: `ENV_CONTRACT_REQUIRED_KEYS`
in `compare.py` updates; all baselines re-capture; old
baselines move to a new archive directory parallel to
`tests/visual/baselines-v0.2-archive/`.

## 7. Theme switching (light → dark, M20 slot)

`SignalForgeStyle` exposes a `Theme` enum
(`Theme::Light`, `Theme::Dark`) and `setActiveTheme(Theme)`.
At M16 only Light is wired (Dark palette role values exist
in `tokens.json` but the activation path is M20 scope per
V0.3 charter).

M17 widget code should:

- Read colours via `palette().color(QPalette::Role)`, not
  by directly indexing `tokens.json`'s `palette.light` /
  `palette.dark`. The `QPalette` re-populates when
  `setActiveTheme` is called; widgets that read from the
  palette automatically pick up the new theme.
- For QSS-class-styled labels, the theme switch will
  re-apply the global stylesheet (loaded from a different
  `:/styles/tokens-dark.qss` if M20 adds one), causing the
  semantic-class label colours to follow theme.
- **Avoid caching colours**. If a widget calls
  `palette().color(...)` in its constructor and stores the
  `QColor` for later use, a theme switch won't update it.
  Read the palette every paint.

The Light/Dark switch hook is `applyAtStartup` reading the
saved user preference; M17 widgets don't need to wire to
the switch event — they re-paint on `QApplication::
paletteChanged` which the framework emits automatically.

## 8. Widget testing patterns

### 8.1 Visual regression test

For an M17 widget that renders a visual state worth
locking in, follow the M15 / M16 pattern:

1. Add a state to `tests/visual/scripts/capture_baselines.py`
   `specs_phase_*()` (state name + `launch_args` describing
   how to drive the widget into the state + `fullscreen`
   flag if the capture needs modal / menu content).
2. Run the capture: `python3 tests/visual/scripts/
   capture_baselines.py <state-name>`.
3. Manually inspect the resulting PNG at
   `tests/screenshots/baseline-candidate/<state>.png`. If
   correct, promote: `scripts/accept-baseline.sh
   <state-name>`.
4. Add a Python test under `tests/visual/tests/test_*.py`
   that calls `compare_with_contract(actual, baseline,
   require_env_sidecar=True)` against the new baseline.

### 8.2 QSS class assertion (unit-test surface)

If a widget's class-property logic is non-trivial (e.g.,
multi-state with subtle transitions), unit-test the
property directly instead of via visual diff:

```cpp
TEST_CASE("ConnectionStatusLabel reflects state in class property", "[connection]") {
    ConnectionStatusLabel label;

    label.setState(ConnectionState::Connected);
    REQUIRE(label.property("class").toString() == "status-connected");

    label.setState(ConnectionState::Error);
    REQUIRE(label.property("class").toString() == "status-error");
}
```

Visual regression tests then ensure the QSS-side actually
**renders** the right colour for each class value. The two
layers are decoupled: unit tests verify the class string;
visual tests verify the colour rendering.

## 9. Prohibited patterns

A non-exhaustive list of anti-patterns that **fail M16
R10/R11/R12/R14/R15 discipline** and must be flagged in
M17 code review:

| Anti-pattern | Why prohibited | Right way |
|---|---|---|
| `setStyleSheet("background: #3b7ddd;")` per widget | Defeats manifesto + breaks cross-env determinism (capture is no longer a function of `tokens.json` alone) | Set `objectName` or `class` property + style via `tokens.qss` |
| Literal `QColor(0x0A, 0x0C, 0x10)` in C++ | Hard-coded; not manifesto-derived | Read from `generated_style_tokens.hpp` or `palette().color(QPalette::...)` |
| Per-widget `QFont("Inter", 12)` construction | Bypasses `SignalForgeStyle::applyAtStartup` font cascade | Read `QApplication::font()` (already populated with the bundled Inter 12pt) |
| Inventing a new `class` string without a `tokens.json` update | Token system has no record of the role; CI freshness check won't catch the mismatch; future readers don't know what `class="foo-bar"` is supposed to look like | Add the class to `tokens.json` `semantic_classes` + add a one-sentence rationale to `visual-identity.md` + regenerate |
| Modifying any of the three generated files directly | Next generator run overwrites the change silently | Edit `tokens.json`; run generator; CI check catches drift |
| Using `QHash` / `std::unordered_map` iteration order for any user-visible output | ADR-014 root cause; non-deterministic cross-host | Sort the result before display (ADR-014 pattern) or use `std::map` if cost is acceptable |

## 10. Frequently-needed M17 patterns (anticipated)

The M17 widget rebuild will likely need these patterns;
documented here so the first M17 work doesn't re-derive them:

### 10.1 Splitter handle colour

`QSplitter::handle()` styling via QSS:

```css
QSplitter::handle:horizontal {
    background: #D8DBE0;
    width: 1px;
}
QSplitter::handle:vertical {
    background: #D8DBE0;
    height: 1px;
}
```

(Source: M16 baselines show splitter handles as 1-px lines
in `tokens.divider` colour. ADR-014 follow-up surfaced
sub-pixel splitter-handle position drift at 1-px scale —
state 04 residual; documented in `s7-baseline-migration.md`
§7.6.)

### 10.2 Tree widget item padding (for SignalSelector or any
M17 tree widget)

```css
QTreeWidget::item { padding: 2px 0; }
QTreeWidget::item:selected { background: #3b7ddd; color: #FFFFFF; }
```

### 10.3 Dialog button bar

`QDialogButtonBox` is already styled by `tokens.qss`
button-bar rules; no per-dialog override should be needed.
If a dialog needs a custom action button, add it via
`QDialogButtonBox::addButton(QPushButton*, ButtonRole)` so
the standard layout applies.

### 10.4 Dock widget title bar

`QDockWidget::title` styled via QSS at panel-header colour
+ padding. To make a dock's title invisible (e.g., for an
inline panel), use
`dock->setTitleBarWidget(new QWidget())` — never QSS-hide
since that breaks accessibility.

## 11. Cross-references

- **Manifesto**: `docs/v0.3/visual-identity.md`
- **Token source**: `resources/styles/tokens.json`
- **Token schema**: `resources/styles/tokens.schema.json`
- **Generator**: `tools/generate_style_assets.py`
- **CI freshness gate**: `.github/workflows/ci.yml` step
  "M16 token-freshness gate"
- **Style application**: `src/app/app_style.{hpp,cpp}` —
  `SignalForgeStyle::applyAtStartup` + helpers
- **Visual-diff contract**: `docs/v0.3/visual-diff-contract.md`
- **Env contract**: `docs/v0.3/rendering-environment-lock.md`
- **Mask precedent**: `docs/v0.3/s6-cross-env-verification.md`
  §11 (universal status-bar mask, R8 single-approval pattern)
- **Migration precedent (V0.2 → M16)**:
  `docs/v0.3/s7-baseline-migration.md`
- **ADR-014** (signal-tree deterministic order; the kind of
  non-determinism this guide's §9 anti-pattern catches):
  `docs/architecture/decisions/ADR-014-signal-buffer-registry-deterministic-order.md`
- **QSS linter** (`tests/visual/lib/qss_linter.py` per
  M16 S3) — flags forbidden selectors (`*`, deep
  descendants) at CI time
- **Generated style tokens header**:
  `src/app/generated_style_tokens.hpp`
