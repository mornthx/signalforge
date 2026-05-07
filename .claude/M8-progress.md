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
