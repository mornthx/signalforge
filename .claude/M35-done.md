# M35 — Done report (video core)

**Milestone:** M35 — dedicated UDP RGB24 video ingest + Video page (display · stall handling · stream
stats · screenshot · recording). Tooling (pause, zoom/pixel-probe, color correction, presets) is M36.

## Status: LOCAL-READY — remote git ops deferred (awaiting per-op authorization)
All M35 work is committed locally on `milestone/M35`. **Not pushed; no PR; not merged** — `git push`,
`gh pr create`, `gh pr merge`, and tags require per-operation authorization from the session prompt
(CLAUDE.md §4; session-level blanket auth is not valid). This report is produced under the `/goal`
directive to keep working through M36 locally and present the whole stack for one review.

## Scope delivered (P0–P4)
- **P0** `4ea866f` — ZVID protocol parse + dedicated `VideoUdpReceiver` (own IO thread, 16 MB SO_RCVBUF,
  inline reassembly, complete-frame `QImage` over queued connection, drop counting, stats).
- **P1** `8405bb0` — Video page parallel to Connect/Inspect: `VideoView` + `VideoPage` display state
  machine + stall watchdog; `UdpConfig` gains `videoPort`/`videoEnabled` (additive), connection dialog
  UDP page + YAML round-trip; `MainWindow` starts/stops the receiver from the connection-state hook.
- **P2** `90ae529` — stats overlay (fps/Mbps/dropped/resolution) + amber "raise rmem_max" hint + Stats
  toggle.
- **P3** `2f26ebb` — frame screenshot (PNG), gated on a live frame.
- **P4** `aaf7084` — recording: ffmpeg-CLI MP4 (default) + raw RGB24 fallback; Record/Stop + format
  combo + REC elapsed indicator.

## Verification
- **Build:** Debug ✅ and Release ✅ (full app links `signalforge_video`). ASan: blocked locally
  (`/etc/ld.so.preload`); **CI `debug-asan` is the authoritative sanitizer gate.**
- **Tests:** full suite **774/774 pass on Debug, Release green** (`ctest -LE visual`, offscreen). New
  `video_test`: 24 cases / 102 assertions (protocol, receiver reassembly/drop/rebind, page state +
  overlay + screenshot + recording; recorder raw round-trip + MP4-via-real-ffmpeg guarded by
  `ffmpegAvailable()`). Visual baselines excluded (non-blocking per `memory/ci_visual_baseline_divergence`;
  no live-video pixel baselines added — concern C5).
- **Perf (§5):** reassembly bench 84.3 fps / 1865.7 Mbps, 0 drops (≫ board 25.5 fps / 570 Mbps).
- **clang-format:** clean. **clang-tidy:** only the codebase's idiomatic-Qt baseline (`new X(this)`
  ownership, `public slots:`, signal-forwarding param names, enum-size) — same categories as
  `udp_driver.cpp`; `WarningsAsErrors: ''`.

## Frozen interfaces
None changed. `UdpConfig` gained two fields **additively** (defaults; guard-on-read YAML keeps old
configs loading) — the M9-frozen `DriverConfig` variant alternatives/ordering and `ConnectionConfig`
field set are untouched.

## Footprint
`origin/main...HEAD`: **+1424 production lines** (src/), **+834 test lines**, +305 docs. 5 code commits.

## Deviations and concerns
- **§4 (≤800 net lines / PR):** M35 production is ~1424 lines, over the 800 target. Precedent: M34 was
  +21,684 in one PR (#32). The owner-approved M35/M36 split already halved the original scope. **Option
  at authorization time:** accept as one PR (per M34 precedent) or split into two stacked PRs
  (P0–P1 / P2–P4). Recommend one PR for review locality, matching M34.
- **ffmpeg = runtime tool, not a §4.1 dependency:** MP4 recording shells out to the `ffmpeg` binary via
  `QProcess` (no link/FetchContent dep; `architecture.md` untouched). Owner-authorized. Raw fallback
  covers ffmpeg-absent hosts.
- **Single video stream:** the last-connected `videoEnabled` UDP connection drives the one receiver
  (mirrors the existing "last connected schema wins" dissector model). Multi-stream video is out of
  scope.

## PR / CI / merge
- PR: _not created — awaiting push authorization._
- CI: _n/a until pushed._
- Merge SHA: _placeholder._

## Next
Per the session `/goal`, M36 (video tooling) proceeds locally on `milestone/M36` (branched from this
HEAD), committed for the same single review. Final stop will present both milestones and request
authorization for all deferred remote operations (push `milestone/M35` + `milestone/M36`, PRs, merges,
tags).
