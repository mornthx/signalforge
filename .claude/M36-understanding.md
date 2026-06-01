# M36 — Understanding (video tooling)

## Context
M36 is the second half of the owner-defined video feature (see `.claude/M35-*.md`). M35 delivered the
core: dedicated UDP RGB24 ingest → Video page (display, stall handling, stream stats, screenshot,
recording MP4+raw). M36 stacks **interaction + color** on that stable trunk. Branched locally from the
M35 HEAD; both milestones are committed locally for one combined review (per the session `/goal`).

## Scope (owner-confirmed in the M35 chat)
1. **Pause / freeze frame** — freeze the displayed frame for inspection; the receiver keeps running and
   recording continues live.
2. **Zoom + pixel probe** — magnify a region and read a source pixel's RGB value (diagnose the camera's
   off-color/dark RAW).
3. **Software color correction** — standard set: white-balance per-channel gain, brightness, contrast,
   gamma, saturation. Applied **to display + screenshot**; recording offers **raw / corrected** (default
   corrected). Owner flagged this as the first of a possibly larger color toolset.
4. **Color presets** — save/load the calibration to/from disk.

## Architecture
- The M35 `VideoUdpReceiver` keeps emitting **raw** frames (unchanged) so the raw stream stays available
  for raw recording and faithful capture.
- A new `ColorCorrector` (per-channel 256-entry LUTs for wb/brightness/contrast/gamma + a saturation
  pass) transforms a raw frame → corrected. Identity params are a no-op fast path. Applied in
  `VideoPage` on frame arrival: the **corrected** frame feeds the view + screenshot; the recorder is fed
  raw or corrected per the record-source selector. Cost is a few ms/frame on the GUI thread only when
  correction is active — benchmarked (CLAUDE.md §5).
- `ColorParams` ⇄ JSON via `nlohmann/json` (already in `architecture.md §4.1`) — **no new dependency.**
- `VideoView` gains zoom/pan (wheel + drag) and a pixel-probe (mouse-move → source-pixel mapping),
  plus a frozen mode (pause).

## Dependencies
**Zero new dependencies.** All Qt Gui/Widgets + `nlohmann/json` (§4.1). No frozen interfaces touched.

## Status
Plan in `.claude/M36-plan.md`. Executing locally P0→P5; final stop requests authorization for all
deferred remote git operations across M35 + M36.
