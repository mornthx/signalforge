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

---

## S4 — 30 Hz tick + LOD queryRange + live cursor (start)

**Goal**: per plan §S4 (5 h estimate), wire the 30 Hz redraw
timer, populate per-signal geometry from
`bufferFor(id)->queryRange(start, end, target=width())`, render
the live time cursor with the per-user-note "subpixel-alternate
as primary" pattern (since at long visible_duration the cursor
moves <1 px/frame, so alternation is the hard guarantee against
RHI frame coalescing — not a fallback), and capture FrameStats.

### S4 — close

`Chart::onTick` runs every 33 ms (Qt::PreciseTimer):

1. Increment `stats_.totalRedraws`. If we have a previous tick,
   compute `dt` and increment `stats_.droppedFrames` if `dt > 50 ms`.
2. Read `axisStart` / `axisEnd` from `timeAxis_->visibleStart() /
   visibleEnd()`. Pixel width = `width()` (clamped to ≥ 1).
3. For each visible signal, call
   `registry_->bufferFor(id)->queryRange(start, end, pixelWidth)`.
   Stash the result in `Impl::latestSamples`. Track global Y min/max
   across all visible signals' samples.
4. Auto-scale Y: pad ±5% (or ±0.5 if min==max). Default to [-1, 1]
   if no data.
5. Capture `lastRenderUs` and update `peakRenderUs` under the
   stats mutex.
6. Call `update()` to schedule the paint.

`updatePaintNode` (render-thread sync stage):

1. Tear down nodes whose ids are in `pendingRemovals`.
2. For each visible signal: ensure node exists; map samples to
   (x, y) via `xScale = w / axisDurationNs` and Y auto-scale.
3. Live cursor: a 2-vertex line node (always present after first
   tick). Per-frame the cursor's `y0` alternates between 0 and 1
   (`cursorYAlternate` flag). This is the **primary** mechanism
   that defeats RHI frame coalescing per spec §4.5 + the user's
   S4 note (at 1-hour-visible / 1500-px width, x moves < 1
   px/frame; alternation is the guarantee, not a fallback).

Constructor starts the timer; destructor stops it. In unit tests
without a `QCoreApplication`, the timer's `start()` is a no-op
in practice (no event loop dispatches `timeout`), so the lifecycle
tests stay event-loop-free.

**Build**: clean on debug + release + debug-asan.
**Tests**: 425/425 release (unchanged — onTick + cursor rendering
is exercised in S9 integration tests where a Qt event loop runs).
**Format**: clang-format clean on changed files.
**Frozen-file diff**: empty (only `src/chart/chart.cpp` modified).
**Effort**: ~1.0 h (plan estimate 5 h).
