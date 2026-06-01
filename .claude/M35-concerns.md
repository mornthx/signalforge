# M35 — Concerns (re-scoped 2026-06-02)

## C1 — Codec dependency — **RESOLVED**
The old M35 framing's load-bearing worry. The source is **raw RGB24**, so display needs no decoder:
a complete frame is `QImage(buf, w, h, QImage::Format_RGB888)`. **No new dependency for display.**
The MJPEG/H.264 fork and any `§4.1` amendment are off the table for M35.

## C2 — ffmpeg as a runtime tool (recording only)
Recording to MP4 needs an encoder. Decision: **invoke the `ffmpeg` binary via `QProcess`** (pipe raw
RGB24 to stdin), *not* link a codec library. This is a **runtime tool dependency**, not a `§4.1`
link/FetchContent dependency, so it does **not** require an `architecture.md` amendment (and CLAUDE.md
forbids editing that file regardless). Owner-authorized this chat (ffmpeg 6.1.1 present).
**Mitigation:** detect `ffmpeg` at runtime; if absent, disable MP4 recording with a clear message and
keep the dependency-free **raw** dump available. Document the tool requirement in `M35-done.md`.

## C3 — Performance & memory (new highest-bandwidth path)
~570 Mbps, ~25 fps, 2.76 MB/frame, ~48k datagrams/s — a different cost class from scalar refresh.
- Reassembly + recv off the UI thread (CLAUDE.md §8); UI thread only blits the latest `QImage`.
- Dedicated `VideoUdpReceiver` keeps this load **out of** the scalar `FramePipeline`.
- Bounded memory: hold only the latest complete frame (+ the in-progress assembly buffer); latest-wins.
- Before/after throughput benchmark required (CLAUDE.md §5): reassembly fps / Mbps / drop rate.
- ThreadSanitizer test for the receiver worker (CLAUDE.md §6).

## C4 — Host receive-buffer (operator step, surfaced in UI)
Without `sudo sysctl net.core.rmem_max=33554432`, 25 fps bursts overflow the socket buffer and drop
packets (~6 fps of complete frames). This is a **host** setting, not a board issue. M35 raises
`SO_RCVBUF` on its socket and **surfaces a UI hint** when the measured drop rate is high, rather than
failing silently.

## C5 — Visual baseline tests vs live video
The Video page renders non-deterministic live content. Do **not** add full-window pixel baselines of
live frames (they would false-red like the M15 full-window baselines already do on CI —
`memory/ci_visual_baseline_divergence.md`). Test reassembly/stats/controls with logic + GUI-interaction
tests (synthetic ZVID sender as oracle) and, if a visual check is wanted, render a fixed test-pattern
frame rather than the live stream.

## C6 — Sanitizer gate is CI-authoritative
Local ASan is blocked by `/etc/ld.so.preload` (`memory/host_asan_preload`). Sanitizer verification for
the new receiver/recording paths routes through the `debug-asan` CI gate, not the local host.

## Resolution
All concerns have an agreed mitigation; none blocks M35. Decisions captured in `M35-understanding.md`
and `M35-plan.md`. Proceed pending Phase-4 execute approval.
