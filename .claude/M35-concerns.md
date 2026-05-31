# M35 — Concerns

## C1 — Codec dependency (the load-bearing decision)
`architecture.md §4.1` permits no Qt Multimedia / FFmpeg / codec library. Per CLAUDE.md §1 a new
dependency is a HALT / owner-approval event.
- **Image + Motion-JPEG**: decoded by `QImage::loadFromData` (Qt Gui, already linked) → **no new dep.**
- **H.264 / H.265**: needs FFmpeg or Qt Multimedia → **§4.1 amendment + owner approval.**

**Recommendation:** ship MJPEG-only first (whole pipeline, zero dep risk); keep the decoder behind an
interface so H.264 is a later, owner-gated add-on. Do not pull a codec dependency without explicit
approval.

## C2 — Transport / source ambiguity
Whether the source is a **custom device** (→ simple custom chunk protocol, MJPEG) or **off-the-shelf
IP cameras** (→ RTP/RTSP, likely a dependency) changes most of the design. Needs an owner answer
before the transport layer is fixed. Do not guess.

## C3 — Perf + memory (new highest-bandwidth path)
30 fps × multi-MB frames is a different cost class from scalar refresh: decode must be off the UI
thread (CLAUDE.md §8), the media buffer must be explicitly memory-bounded (QImages are large), and the
path needs before/after benchmarks (CLAUDE.md §5). Reuses the deferred per-component-cadence design
(`memory/heterogeneous_frame_rates.md`).

## Resolution
Exploration (`docs/v0.4/media-playback-exploration.md`) lays out the options. Await the owner's 4
decisions (exploration §5) before writing `M35-plan.md`; then plan → Phase-4 execute approval.
