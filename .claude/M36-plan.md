# M36 — Plan (video tooling)

Each phase meets the per-task Definition of Done (Debug/Release build, ctest, ≥70% coverage on new
public surface, Doxygen, clang-format/clang-tidy clean, conforming commit, progress update). All work is
local on `milestone/M36`; remote ops are deferred to one authorization after P5.

## P0 — Pause / freeze frame
- `VideoView`: `setFrozen(bool)` — when frozen, ignore `setFrame` (keep the displayed image).
- `VideoPage`: a Pause/Resume button; while paused the view freezes but the recorder still receives live
  frames (recording is unaffected). Screenshot/zoom/probe act on the frozen image.
- Tests: paused view ignores new frames; resume re-syncs; recording continues while paused.
- Commit: `video: add pause/freeze frame`

## P1 — Zoom + pixel probe
- `VideoView`: zoom factor (wheel toward cursor, +/− , reset-to-fit), pan by drag when zoomed; the
  image→widget transform is the single source of truth. Mouse-move maps to a source pixel and emits
  `pixelProbed(QPoint, QColor)`.
- `VideoPage`: shows the probed pixel `(x,y) RGB(r,g,b)` in the control bar.
- Tests: widget↔image coordinate mapping at fit and zoomed; probe returns the expected pixel color.
- Commit: `video: add zoom and pixel probe`

## P2 — Software color correction
- `color_correction.{hpp,cpp}` — `ColorParams { rGain,gGain,bGain, brightness, contrast, gamma,
  saturation }` (+ `isIdentity()`), and `ColorCorrector` (rebuilds 3 LUTs on `setParams`; `apply(QImage)`
  returns the corrected RGB888 image, identity = passthrough).
- `VideoPage`: applies the corrector to each frame for display + screenshot; a collapsible slider panel
  edits the params live.
- Benchmark: corrector throughput at 1280×720 (CLAUDE.md §5).
- Tests: per-channel transforms (gain/brightness/gamma), saturation extremes (grayscale at sat=0),
  identity passthrough.
- Commit: `video: add software color correction (LUT)`

## P3 — Color preset save/load (JSON)
- `ColorParams` ⇄ `nlohmann/json`; `VideoPage` Save/Load preset buttons (file dialog) + non-dialog
  `saveColorPreset(path)` / `loadColorPreset(path)` seams.
- Tests: JSON round-trip (all fields), load applies to the corrector + sliders, malformed file rejected.
- Commit: `video: add color-correction presets (JSON save/load)`

## P4 — Recording raw/corrected linkage
- A record-source selector (Corrected default / Raw); `VideoPage` feeds the recorder the raw or corrected
  frame accordingly. Default corrected = WYSIWYG; raw = faithful capture.
- Tests: record-source selection routes the expected pixels to the recorder (raw vs corrected differ
  under a non-identity correction).
- Commit: `video: link recording to raw/corrected source`

## P5 — Closure
- `.claude/M36-progress.md` + `.claude/M36-done.md`; full suite green Debug+Release. **Stop** and present
  M35 + M36 for one review; request authorization for all deferred remote operations (push both
  milestone branches, PRs, merges, tags).

## Cross-cutting
All new code under `src/video/` + tests under `tests/unit/video/`; `#pragma once`, `SF_LOG_*`, modern
C++, queued cross-thread signals. HALT on a new link dependency, frozen-interface change, missed perf
target after one pass, or blocking ambiguity.
