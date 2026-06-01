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

## P1–P5 — pending
See `.claude/M35-plan.md`.
