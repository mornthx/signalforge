# M35 — Understanding (re-scoped, owner-clarified 2026-06-02)

## Roll-back note
The earlier M35 framing (commits `f60d49f`, `43f60f6`) built the whole milestone around a
**codec fork** — MJPEG vs H.264, a possible `architecture.md §4.1` amendment, HALT. That premise is
**now obsolete and has been rolled back**: `docs/v0.4/media-playback-exploration.md` is deleted and
this file + `M35-concerns.md` are rewritten against the clarified requirement below. The source sends
**raw RGB24**, so there is **no codec dependency for display** — the load-bearing concern evaporates.

## Owner-clarified requirement (2026-06-02)
Implement **UDP video display** in SignalForge. The video port lives in the **connection** config; the
video display is a **new top-level page parallel to Connect and Inspect**. Functionality first.
Confirmed feature set and design decisions (this chat):

- **Source / transport** — dedicated video stream on its own UDP port (not routed through the scalar
  pipeline).
- **Recording** — default to compressed **MP4 (via ffmpeg)**; **raw** frame dump as an option.
- **Screenshot** — save the current frame.
- **Stream stats** — fps / Mbps / dropped-frames / resolution overlay.
- **Stall handling** — "waiting for stream" placeholder + auto-rebind; hint to raise host `rmem_max`
  when drop rate is high.
- **Pause / freeze frame.**
- **Zoom + pixel probe** (read RGB value — diagnose the camera's off-color/dark RAW).
- **Software color correction** — in-app adjustable color (standard set: white-balance per-channel
  gain, brightness, contrast, gamma, saturation), applied live; **save/load** calibration presets.
  Scope: color applies to **display + screenshot**; recording offers **raw / corrected** (default
  corrected). The owner flagged this as item "1." of a possibly larger color toolset — later sub-features
  stack in a future milestone.

## The source (board protocol — `视频UDP流-应用接入.md`)
XCZU15EG + OV5640, **1280×720 RGB24**, **~25.5 fps**, UDP **broadcast** `169.254.255.255:5004`
(receiver binds `0.0.0.0:5004`, no IP config needed). Each frame = **1920 datagrams**, each
**24B little-endian header + ≤1440B pixels**:

| off | type | field | note |
|----|------|-------|------|
| 0  | u32 | `magic`       | `0x5A564944` ('ZVID') — filter foreign packets |
| 4  | u32 | `frame`       | frame number, +1 per frame |
| 8  | u32 | `offset`      | byte offset of this chunk in the full RGB24 frame |
| 12 | u32 | `frame_bytes` | full frame size (= w*h*3 = 2,764,800) |
| 16 | u16 | `w` (1280) / 18 u16 `h` (720) / 20 u16 `bpp` (3) / 22 u16 `chunk_len` | |

Reassembly: validate `magic`; copy `chunk_len` bytes to `buf[offset:offset+chunk_len]`; deliver the
frame only when `got == frame_bytes`; **drop incomplete frames** (drop only lowers fps, never tears).
Read `w`/`h` from the header (adapt resolution); **warn (not crash) if `bpp != 3`** — RGB565 is out of
M35 scope. Receiver must raise `SO_RCVBUF` (~16 MB); host also needs `net.core.rmem_max` raised
(a `sudo sysctl` operator step) or 25 fps bursts overflow the socket buffer and drop packets.

## How it lands in the existing code (verified)
- **Ingest (new, dedicated):** `VideoUdpReceiver` — own `QUdpSocket` on a worker thread, inline
  reassembly, emits **complete frames** as `QImage(Format_RGB888).copy()` over `Qt::QueuedConnection`.
  *Not* the scalar `UdpDriver`/`FramePipeline` (which wraps one `RawFrame` per datagram ≈ 48k/s and
  would mix video into scalar decode at 570 Mbps). This realizes the deferred per-component-cadence
  design (`memory/heterogeneous_frame_rates`).
- **Display:** custom `QWidget::paintEvent`, aspect-preserving scale, repaint **on frame arrival**
  (own cadence, not the 60 Hz dashboard timer). Latest-wins, bounded to the latest complete frame.
- **Page:** `workbench_->addMode("video", tr("Video"), …)` — third mode beside connect/inspect
  (`src/app/main_window.cpp:614`).
- **Config:** `UdpConfig` (`src/drivers/driver_configs.hpp:43`) gains `videoPort` + `videoEnabled`;
  `ConnectionDialog::buildUdpPage()` (`src/connection/connection_dialog.cpp:168`) gains a "Video port"
  spinbox + "Enable video" checkbox.
- **Logging / threads:** `SF_LOG_*` (`src/observability/logging.hpp`); cross-thread delivery uses
  `Qt::QueuedConnection` (CLAUDE.md §8); receiver worker gets a ThreadSanitizer test (CLAUDE.md §6).

## Dependencies
- **Display / screenshot / stats / color** — **zero new dependency** (Qt Gui/Widgets/Network, plus
  `nlohmann/json` already in §4.1 for color presets).
- **Recording (MP4)** — **ffmpeg invoked as an external CLI via `QProcess`** (pipe raw RGB24 to
  `ffmpeg -f rawvideo -pix_fmt rgb24 -s WxH -r 25 -i - -c:v libx264 -pix_fmt yuv420p out.mp4`). This is
  **not** a `§4.1` link/FetchContent dependency — it is a **runtime tool**. Owner-authorized this chat
  (ffmpeg 6.1.1 confirmed installed). Degrades gracefully: if `ffmpeg` is absent, MP4 recording is
  disabled with a clear message and **raw** dump stays available. `architecture.md` is **not** modified.

## Milestone split (owner-approved 2026-06-02)
The full feature set exceeds CLAUDE.md §4 (≤800 net lines / PR). Split into two milestones:

- **M35 — video core (this milestone):** dedicated ingest + video port · Video page + display ·
  stall placeholder + auto-rebind · stream-stats overlay · screenshot · recording (MP4 default + raw).
- **M36 — video tooling (next milestone):** pause/freeze · zoom + pixel probe · software color
  correction (standard set) · color-preset save/load · recording raw/corrected linkage.

Boundary rationale: M35 closes the "acquire → display → see health → capture" trunk as an
independently shippable, verifiable loop; M36 stacks interaction + color on the stable trunk.

## Status
Spec re-scoped. `M35-plan.md` written for the core phases. **Awaiting Phase-4 execute approval**
("approved, execute M35"). No code yet.
