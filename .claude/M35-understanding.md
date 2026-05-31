# M35 — Understanding (bootstrap)

## Where we are
- **M34 merged** (PR #32 → `main`, merge `7289cd7`, tag `v0.0.34.1`). `milestone/M35` created from
  `main`. M34 delivered the entire v0.4 UI redesign (P0–P5).
- CI gate: build + ~750 unit/logic tests green on debug / release / **debug-asan** (sanitizers clean).
  Full-window visual baselines are non-blocking (see `memory/ci_visual_baseline_divergence.md`).

## M35 direction (owner-set, 2026-05-31)
**Add image & video playback support, likely over UDP.** The owner asked to **think first** about
communication / architecture / feature / UI before defining concrete M35 content — so this milestone
opens with a **design-exploration phase**, not a fixed plan.

**Deliverable produced:** `docs/v0.4/media-playback-exploration.md` — explores transport, architecture,
feature surface, and UI, and lists the decisions to make. Key findings:
- **Codec is the load-bearing fork** (`architecture.md §4.1` allows Qt Core/Widgets/Quick/Network/
  SerialPort — **no Multimedia, no FFmpeg**). **Image + Motion-JPEG decode with `QImage` needs ZERO
  new dependencies**; **H.264 needs FFmpeg/Qt-Multimedia → a §4.1 amendment + owner approval (HALT).**
  → recommend MJPEG-only first, H.264 behind a pluggable decoder later.
- **Transport:** UDP needs app-level chunking + reassembly (a JPEG frame spans many datagrams). Custom
  chunk protocol (we own the device) vs RTP/RTSP (real IP cameras, more deps). The existing `UdpDriver`
  + `FrameSink` model is reused; a new `MediaReassembler` sink branches off the scalar decode path.
- **Architecture:** a media path parallel to the scalar pipeline (Reassembler → Decoder(worker thread)
  → bounded MediaFrameBuffer → VideoPanel), on its **own cadence** (push-on-arrival, not the 60 Hz
  signal timer) — lands the deferred `heterogeneous_frame_rates` design.
- **UI:** a new `VideoPanel`/`ImagePanel` dashboard panel type (fits `makePanel` + PanelConfig +
  inspector); open question whether media also deserves a first-class tier.

## Decisions pending from the owner (see exploration §5)
1. Source type → codec/transport: **custom device (MJPEG, no deps)** vs IP cameras (RTP/H.264, deps).
2. M35 P1 scope: still image + live MJPEG only, or also replay/scrub + signal-time sync?
3. H.264 in M35 or deferred? (the only part forcing a new dependency)
4. UI home: dashboard panel only, or a first-class media tier?

## Status
Exploration delivered; **awaiting the owner's answers to the 4 decisions** before writing
`.claude/M35-plan.md`. No code yet.
