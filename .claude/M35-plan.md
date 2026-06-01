# M35 — Plan (video core)

Scope: dedicated UDP video ingest → Video page display → stall handling → stream stats → screenshot →
recording (MP4 default + raw). Enhancements (pause, zoom/pixel-probe, color correction, presets) are
**M36**. Each phase satisfies the per-task Definition of Done (CLAUDE.md): builds Debug/Release/ASan,
ctest green, ≥70% coverage on public surface, Doxygen on public decls, clang-format/clang-tidy clean,
conforming commit, progress file updated. PRs kept ≤800 net lines (CLAUDE.md §4) — split if needed.

## P0 — Protocol + dedicated receiver (no UI)
- **New** `src/video/video_protocol.hpp` — `ZvidHeader` (`#pragma pack`, LE), `kZvidMagic`,
  `parseHeader()`, constants. Doxygen on all public decls.
- **New** `src/video/video_frame.hpp` — `VideoFrame { QImage image; quint32 frameNumber; }` (or carry
  the `QImage` directly) — the unit emitted to the UI.
- **New** `src/video/video_receiver.{hpp,cpp}` — `VideoUdpReceiver`: `QUdpSocket` on a worker `QThread`,
  `SO_REUSEADDR`, `SO_RCVBUF` ~16 MB, bind `0.0.0.0:<port>`. Inline reassembly by `offset`; deliver on
  `got == frame_bytes`; drop incomplete (count it). Validate `magic`; read `w`/`h` from header; warn
  once via `SF_LOG_WARN` if `bpp != 3`. Emit `frameReady(QImage)` and `statsUpdated(...)` over
  `Qt::QueuedConnection`. Start/stop/rebind API.
- **Config:** `UdpConfig` (`src/drivers/driver_configs.hpp`) += `quint16 videoPort{5004}`,
  `bool videoEnabled{false}`.
- **Tests** (`tests/unit/video/`): synthetic ZVID sender util (Python-reassembly logic as oracle).
  Assert: full frame byte-exact; dropped datagram → incomplete frame dropped + drop count up; resolution
  read from header; foreign-magic filtered. **TSan** test on the receiver worker (CLAUDE.md §6).
  **Throughput benchmark** (fps/Mbps/drop) for the commit body (CLAUDE.md §5).
- Commit: `video: add ZVID reassembly + dedicated UDP receiver`

## P1 — Video page + display + stall handling
- **New** `src/video/video_page.{hpp,cpp}` — `VideoPage` (the mode content): display widget + a control
  bar (stub buttons filled in later phases) + a "waiting for stream" placeholder.
- **New** `src/video/video_view.{hpp,cpp}` — custom `QWidget`, aspect-preserving `paintEvent`, repaint
  on `frameReady` (own cadence), latest-wins.
- **Wire-up** in `src/app/main_window.cpp` (~`:614`): `workbench_->addMode("video", tr("Video"), …)`;
  construct/own `VideoUdpReceiver`; start/stop it from the connection's `videoEnabled`/`videoPort`;
  auto-rebind on config change; show placeholder when no frame for N seconds.
- **Tests:** GUI-interaction (QTest) — switching to the Video mode, placeholder ↔ live transition driven
  by the synthetic sender, rebind on port change (per `memory/feedback_simulate_real_interaction`).
  No live-content pixel baselines (concern C5).
- Commit: `video: add Video page with live display and stall placeholder`

## P2 — Stream stats overlay + rmem hint
- Stats from the receiver (fps = delivered complete frames/s, Mbps = bytes/s, dropped count, `WxH`),
  rendered as a toggleable overlay on `VideoView`. When measured drop rate is high, surface a one-line
  hint to raise host `net.core.rmem_max` (concern C4).
- **Tests:** stats math unit-tested against a scripted sender (known frame/drop counts); overlay
  toggle GUI test.
- Commit: `video: add stream stats overlay and rmem hint`

## P3 — Screenshot
- Control-bar "Screenshot" → save the current frame via `QImage::save` (PNG; default path under a
  sensible captures dir, with a file dialog). Disabled when no frame.
- **Tests:** GUI test clicks Screenshot with a synthetic frame present → file written + decodes back to
  the expected size.
- Commit: `video: add frame screenshot (PNG)`

## P4 — Recording (MP4 default + raw option)
- **New** `src/video/video_recorder.{hpp,cpp}` — `VideoRecorder`: MP4 path spawns `ffmpeg` via
  `QProcess` and feeds raw RGB24 to stdin
  (`-f rawvideo -pix_fmt rgb24 -s WxH -r 25 -i - -c:v libx264 -pix_fmt yuv420p out.mp4`); raw path writes
  RGB24 to a `.raw` file. Detect `ffmpeg` at startup; if absent, disable MP4 with a clear message, keep
  raw (concern C2). Control bar: Record ●/Stop, elapsed time, mode (MP4/raw) selector.
- **Tests:** recorder unit test with a fake/stubbed encoder sink (no real ffmpeg in CI) verifying frames
  are framed + forwarded and start/stop lifecycle; ffmpeg-absent path → MP4 disabled, raw still works.
  GUI test for the Record/Stop control.
- Commit: `video: add recording (ffmpeg MP4 default, raw fallback)`

## P5 — Milestone closure (CLAUDE.md milestone flow, Phase 1)
- `.claude/M35-progress.md` final; `.claude/M35-done.md` (PR #, CI status, ffmpeg runtime-tool note,
  deviations). Push `milestone/M35`; CI green; **PR to main, do not merge**; report; announce
  "M35 ready. Awaiting approval to merge M35 and begin M36 bootstrap."
- All git push / PR operations require per-operation authorization from the session prompt (CLAUDE.md
  §4 / Git operation protocol).

## Cross-cutting
- All new code under `src/video/`; matching tests under `tests/unit/video/` (CLAUDE.md Required §1).
- `#pragma once`, no `using namespace` in headers, `SF_LOG_*` only, no swallowed exceptions, modern C++
  (`std::unique_ptr`, `std::optional`, `std::span`/`std::string_view` where apt).
- Cross-thread → `Qt::QueuedConnection` + a TSan test for any threaded component.
- Performance-sensitive commits carry before/after numbers (CLAUDE.md §5).
- HALT on: a new link dependency, a frozen-interface change, a perf target missed after one pass, or any
  ambiguity that blocks progress (CLAUDE.md HALT triggers).
