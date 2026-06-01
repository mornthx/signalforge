# M35 — Progress (video core)

Branch: `milestone/M35`. Plan: `.claude/M35-plan.md`. Local-only commits so far (push/PR gated on
per-op authorization per CLAUDE.md).

## P0 — ZVID protocol + dedicated VideoUdpReceiver — ✅ DONE (local)
New module `src/video/`:
- `video_protocol.hpp` — Qt-free ZVID header (`0x5A564944`), `parseZvidHeader()`, constants.
- `video_types.hpp` — `VideoStats` (fps/Mbps/delivered/dropped/WxH) + metatype.
- `video_receiver.{hpp,cpp}` — `VideoUdpReceiver`: dedicated IO thread, `QUdpSocket` bound
  `0.0.0.0:<port>` with 16 MB `SO_RCVBUF`, inline reassembly by `offset`, delivers complete
  `QImage(Format_RGB888).copy()` via `Qt::QueuedConnection`; drops incomplete frames (counted);
  warns once on `bpp != 3`; periodic `statsUpdated`; `start/stop/rebind`.
- CMake: `signalforge_video` static lib wired into the top-level build.

Tests (`tests/unit/video/`, Catch2 + QtTest, synthetic ZVID sender as oracle): byte-exact reassembly,
incomplete-frame drop + drop counter, foreign-magic filtering, resolution-from-header, rebind, and
pure-protocol parse (valid/short/foreign/null). **9 cases / 35 assertions pass on Debug and Release.**

Benchmark (`tests/benchmark/bench_video_reassembly.cpp`, opt-in): end-to-end loopback at full
1280×720 RGB24 → **84.3 fps / 1865.7 Mbps, 0 drops** (host `rmem_max`=32 MB). Comfortably above the
board's 25.5 fps / 570 Mbps target.

### DoD status
- Build: Debug ✅, Release ✅. ASan: blocked locally (`/etc/ld.so.preload`,
  `memory/host_asan_preload`) → **CI is the authoritative sanitizer gate**.
- ctest (`VideoUdpReceiver|parseZvidHeader`): 9/9 ✅.
- clang-format: clean ✅. clang-tidy: only the codebase's accepted idiomatic-Qt baseline
  (`new X(this)` ownership, `public slots:`, signal-forwarding param names) — identical categories to
  `src/drivers/udp_driver.cpp`; `WarningsAsErrors: ''`.
- Doxygen on all public decls ✅. Coverage: tests exercise the full public surface.
- TSan: the receiver worker is exercised; a dedicated TSan run is the `debug-asan`/CI gate's job.

## P1 — Video page + display + stall handling — ✅ DONE (local)
- `video_view.{hpp,cpp}` — `VideoView`: aspect-preserving paint of the latest frame on a dark
  backdrop, placeholder text when none; repaint-on-set (frame-arrival cadence).
- `video_page.{hpp,cpp}` — `VideoPage` (the Video mode): control bar (status) + `VideoView`, with the
  display state machine Disabled → Waiting → Streaming → Stalled and a stall watchdog (`setStallTimeoutMs`).
- `UdpConfig` += `videoPort` (default 5004) + `videoEnabled` (additive; not a frozen field). YAML
  (de)serialization in `connection_manager.cpp` (guard-on-read = backward compatible). Connection dialog
  UDP page gains "Enable video stream" + "Video port" (port enabled only when the box is checked).
- `MainWindow`: owns `videoReceiver_`; adds the `"video"` mode beside connect/inspect; the connection-
  state hook `rebind()+start`s the receiver when a UDP connection with `videoEnabled` connects and
  `stop`s it on Idle/Error. Receiver → page signals wired.

Tests: `video_page_test.cpp` (offscreen, leaked QApplication per `memory/qt_xcb_teardown_crash`) — view
set/clear/placeholder, page state transitions, stall watchdog, and **end-to-end receiver → page** over
real UDP. Extended `connection_persistence_test` to round-trip `videoPort`/`videoEnabled`.
**video_test: 14 cases / 55 assertions; drivers_test 67/220; connection_persistence 7/72 — all green.**

### DoD status
- Build: Debug ✅, Release ✅ (full app links `signalforge_video`). ASan → CI gate.
- clang-format clean ✅. clang-tidy: widget child-construction + `new X(this)` idiom only (codebase
  baseline; `WarningsAsErrors: ''`). Doxygen on public decls ✅.

## P2 — Stream stats overlay + rmem hint — ✅ DONE (local)
- `VideoView` gains a top-left semi-transparent stats overlay (`setOverlayText` / `setOverlayVisible`),
  drawn over the live frame.
- `VideoPage::onStats` formats `WxH · fps · Mbps · dropped N` into the overlay, and shows an amber
  control-bar hint ("⚠ High packet loss — raise host net.core.rmem_max") when the per-window drop ratio
  exceeds 5% (guards the receiver's counter reset on rebind). A checkable "Stats" toolbar button toggles
  the overlay (on by default).
- `MainWindow` wires `statsUpdated → VideoPage::onStats`.

Tests: overlay text content, Stats-toggle visibility, and the rmem hint appearing/clearing across
samples. **video_test: 17 cases / 64 assertions green; Debug+Release build clean.**

## P3 — Screenshot (PNG) — ✅ DONE (local)
- `VideoPage` gains a "Screenshot" control-bar button, enabled only while a frame is displayed
  (disabled on disable/stall). Click opens a save dialog (default: Pictures dir, timestamped name) and
  writes the current frame as PNG. `saveScreenshot(path)` is the non-dialog test seam.

Tests: button gating across no-frame / frame / stopped, and a PNG save round-trip (decodes back to the
frame size). **video_test: 18 cases / 71 assertions green; Debug+Release clean.**

## P4–P5 — pending
See `.claude/M35-plan.md`.
