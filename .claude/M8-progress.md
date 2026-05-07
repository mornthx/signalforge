# M8 — Progress

Per `.claude/M8-plan.md`. Branch: `milestone/M8` from `352f9972`.

## S1 — Scaffolding + freeze-surface headers + CMake wiring (start)

**Goal**: per plan §S1 (4 h estimate), land the freeze-surface
headers and a buildable static lib so subsequent subtasks have
a place to write code.

1. `src/chart/CMakeLists.txt` adding `signalforge_chart` static
   lib. PUBLIC: Qt6::Core, Qt6::Quick, Qt6::Widgets,
   signalforge_buffer, signalforge_decoder. PRIVATE:
   signalforge_observability. AUTOMOC ON.
2. `src/chart/chart.hpp` matching spec §4.1 verbatim.
3. `src/chart/chart_manager.hpp` matching spec §4.2 verbatim.
4. `src/chart/time_axis_manager.hpp` matching spec §4.3 verbatim
   (TimePreset enum included).
5. `src/chart/signal_selector.hpp` — QWidget subclass; public API:
   `SignalSelector(SignalBufferRegistry&, ChartManager&, QWidget*)`;
   emits `signalToggled(QString, bool)`.
6. `src/chart/{chart, chart_manager, time_axis_manager, signal_selector}.cpp`
   — ctor/dtor stubs.
7. Top-level `CMakeLists.txt` adds `add_subdirectory(src/chart)`.
8. `tests/unit/chart/CMakeLists.txt` + a placeholder smoke test.

**Verification**:
- M7 virtualDriverId default is `"expression-engine"` (confirmed
  via `grep` in `src/expression/expression_engine.hpp:28`). The
  S6 SignalSelector "Derived" group string matches.

**HALT triggers active**: #1 (frozen `.hpp`) — pre-commit `git diff`
against M2-M7 freeze list.

### S1 — close

**Deviation**: ChartConfig field `signals` (spec §4.1 literal)
renamed to `signalConfigs` to avoid Qt `signals` macro collision.
Documented in `.claude/M8-concerns.md` C1. yaml key stays
`signals` for users.

**Build**: clean on debug + release + debug-asan.
**Tests**: 400 / 400 release (was 396 → +4 chart smoke). 4 / 4
chart smoke cases under `[chart][s1][smoke]`.
**Format**: clang-format clean on changed files.
**Frozen-file diff**: empty against M2-M7 inherited freeze list
(verified by `git diff 352f9972 -- 'src/buffer/*.hpp'
'src/decode/*.hpp' 'src/expression/*.hpp' …` — no matches).
**Effort**: ~1.5 h (plan estimate 4 h).

---

## S2 — TimeAxisManager full implementation + unit tests (start)

**Goal**: per plan §S2 (4 h estimate), implement the state machine
(visible range + live/paused) with `rangeChanged` / `liveModeChanged`
Qt signals, plus a unit-test file with ≥ 90% line coverage.

### S2 — close

State machine: visible range is `[visibleEnd - duration,
visibleEnd]` where `visibleEnd()` is dynamic (`now()`) in live
mode and the captured `pausedAt_` in paused mode. Pan / zoom from
live transitions to paused (anchoring the current `visibleEnd`).
`pause()` / `resume()` / `setRange()` / `setPreset()` round-trip
through the same machine.

Defensive guards:
- `zoom(factor, ...)` with `factor <= 0` is ignored.
- `setRange(start, end)` with inverted or empty range is ignored.
- `pause()` while paused / `resume()` while live are no-ops.

Unit test cases (14): default state, pan-from-live, pan-when-paused,
zoom-from-live, zoom-around-reference, zoom-non-positive-factor,
pause/resume roundtrip, pause-when-paused, resume-when-live,
setRange, setRange-inverted, setPreset, all-presets, visibleStart-
tracks-now.

**Build**: clean on debug + release + debug-asan.
**Tests**: 414/414 release (was 400 → +14 TimeAxisManager).
**Format**: clang-format clean on changed files.
**Frozen-file diff**: empty (only src/chart/ + tests/unit/chart/
modified).
**Effort**: ~1.0 h (plan estimate 4 h).

---

## S3 — Chart core: SG node management + per-signal renderers (start)

**Goal**: per plan §S3 (8 h estimate; HALT at >150% = 12 h),
implement Chart's public lifecycle API (addSignal / removeSignal /
setDisplayMode / setSignalVisible / config / setConfig) backed by
a Chart::Impl PIMPL holding a per-signal SG-node map, plus the
auto-display-mode lookup (spec §3.4 / §4.8). Data population is
deferred to S4; here we land the lifecycle + node-allocation
plumbing.

Test target: unit tests cover addSignal/removeSignal round-trip,
auto-display-mode by signal type, setDisplayMode override,
setSignalVisible toggle. These tests don't need an event loop —
just exercise the public API and read back via config().

### S3 — close

`Chart::Impl` PIMPL holds the per-signal `QSGGeometryNode*` map +
a `pendingRemovals` set. `updatePaintNode` consumes the pending
set (deleting nodes whose signals were removed / hidden /
mode-changed), then ensures every visible signal has a node
allocated with the appropriate drawing mode (Line/Step →
DrawLineStrip; Point → DrawPoints) and material color. Vertex
data is left empty in S3 — S4 populates it from
`bufferFor(id)->queryRange(start, end, target=width())`.

Auto-display-mode lookup (spec §3.4 / §4.8): `addSignal(id)`
without explicit mode reads the registry's metadata and selects
Step (Bool), Line (Int64 / Double), or Point (QString); falls
back to Line when no buffer is registered for `id`.

Default-color palette: 8-entry rotating palette, looping for
larger sets. Theming is V1.5+ per spec §2.2-9.

Unit test cases (11): addSignal/removeSignal round-trip;
duplicate-id no-op; empty-id no-op; missing-id removeSignal
no-op; auto-mode for each of Bool/Int64/Double/QString;
explicit-mode override; missing-buffer fallback to Line;
setDisplayMode + missing-id no-op; setSignalVisible toggle
(visibility flag, not removal); setConfig replacement; stats()
returns zeros pre-tick.

**Build**: clean on debug + release + debug-asan.
**Tests**: 425/425 release (was 414 → +11 chart_signal_lifecycle).
**Format**: clang-format clean on changed files.
**Frozen-file diff**: empty (only src/chart/ + tests/unit/chart/
modified).
**Effort**: ~1.0 h (plan estimate 8 h; HALT threshold at 12 h).
