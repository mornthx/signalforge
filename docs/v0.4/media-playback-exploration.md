# M35 exploration — image & video playback (preliminary)

**Status: thinking, not a spec.** Owner set M35 = add image + video playback, likely over UDP. This
doc explores **communication, architecture, feature, UI** and surfaces the decisions to make before
a plan. It does not commit scope.

---

## 0. The one constraint that shapes everything: the dependency allow-list

`docs/architecture/architecture.md §4.1` permits Qt 6.10 **Core / Widgets / Quick / QuickWidgets /
Network / SerialPort** — and **no Qt Multimedia, no FFmpeg, no codec library.** Per CLAUDE.md §1, a
new dependency is a **HALT / owner-approval** event. So the codec choice is the first fork:

| Media kind | Decode path | New dependency? |
| --- | --- | --- |
| **Still image** (JPEG/PNG) | `QImage::loadFromData` (Qt Gui, already linked) | **None** |
| **Motion-JPEG video** (stream of JPEG frames) | per-frame `QImage::loadFromData` | **None** |
| **H.264/H.265 video** | FFmpeg, or Qt Multimedia (FFmpeg backend) | **Yes — needs §4.1 amendment + owner OK** |

**Implication.** Image + **MJPEG** video is achievable **with zero new dependencies** — Qt already
decodes JPEG. Compressed video (H.264) buys ~10–50× bandwidth but costs a heavyweight dependency.
The first decision is therefore: **MJPEG-only (no deps) now, or commit to H.264 (FFmpeg) up front?**
Recommendation: **start MJPEG-only** — it ships the whole pipeline (transport → reassembly → decode →
buffer → panel) with no dependency risk, and H.264 can be added later as a pluggable decoder once the
architecture exists and the owner approves the dep.

---

## 1. Communication (UDP transport)

A camera/device emits image frames. The realities of UDP:
- A datagram is ≤ 65507 B, but only ~1400 B is safe without IP fragmentation. A JPEG frame is
  ~20–150 KB (640×480 → 720p) — so **one frame spans many datagrams → application-level chunking +
  reassembly is mandatory.**
- UDP is **unordered and lossy**. Live video favours **latency over completeness**: drop an
  incomplete frame rather than wait/retransmit.

**Two transport options:**
- **(a) Custom chunking protocol** (we control the device). Per-datagram header:
  `magic | frame_id u32 | chunk_idx u16 | chunk_count u16 | flags | device_ts | payload`. Receiver
  buffers chunks by `frame_id`; emits the assembled encoded frame when `chunk_count` arrive; drops on
  a missing chunk past a short deadline. Simple, matches the existing custom-frame ethos, **no deps.**
- **(b) Standard RTP/RTSP** (interop with off-the-shelf IP cameras). RFC 3550 + MJPEG (RFC 2435) or
  H.264 (RFC 6184), with RTSP session setup + a jitter buffer. Interoperable but a **big protocol
  surface**, and a real RTSP/H.264 stack likely pulls a dependency (live555 / GStreamer / FFmpeg).

**Decision needed:** *what is the real source?* A **custom embedded device** we define the protocol
for → (a) + MJPEG, no deps. **Real IP cameras** → (b) RTP/RTSP/H.264, deps. This single answer
collapses most of the other forks. The existing `UdpDriver` already receives datagrams and stamps a
`RawFrame` per datagram — option (a) reuses that verbatim; the reassembler is a new sink.

---

## 2. Software architecture (how media fits a pipeline built for scalar frames)

Today: `Driver → FramePipeline → SchemaDecoder → SignalBufferRegistry → views`. Frames are small;
the registry holds scalar samples. Media is large, high-rate, and binary — it should **branch off the
shared transport layer into its own path**, not go through the scalar decoder:

```
UdpDriver (datagrams) ─┬─▶ SchemaDecoder ─▶ SignalBufferRegistry ─▶ Parsed/Dashboard (scalars)
                       └─▶ MediaReassembler ─▶ MediaDecoder ─▶ MediaFrameBuffer ─▶ VideoPanel
                            (per source, by frame_id)   (QImage)   (ring of recent frames)
```

- **MediaReassembler** — a `FrameSink` (reuses the pipeline's sink model, like `RawFrameTap`):
  collects chunks per `frame_id`, emits a complete encoded buffer. Per-source state.
- **MediaDecoder** — `QImage::loadFromData` for JPEG (pluggable interface so an H.264 decoder can drop
  in later behind the owner's dep decision). **Runs on a worker thread** (decode is CPU-heavy); hands
  the `QImage` to the UI thread via `Qt::QueuedConnection` (CLAUDE.md §8 — cross-thread to UI).
- **MediaFrameBuffer** — bounded ring of recent frames. QImages are large (720p RGBA ≈ 3.5 MB), so the
  bound is **memory-driven** (e.g. last N frames for live; a longer window only if replay/scrub is in
  scope). Separate from `SignalBufferRegistry`.
- **Cadence — the heterogeneous-rate concern** (see `memory/heterogeneous_frame_rates.md`): a video
  panel must run on **its own cadence (the stream's fps, push-on-arrival), NOT the dashboard's 60 Hz
  signal timer**, and on a **separate perf path**. This was explicitly flagged as a future need; M35
  is where it lands. The dashboard's refresh-rate is already configurable, but media should repaint on
  frame arrival, not poll.
- **Perf**: decode + upload of N MB/frame at 30 fps is a different cost class from scalar refresh
  (§ CLAUDE.md: perf-sensitive paths need before/after benchmarks). Keep decode off the UI thread;
  consider a `QQuickWidget`/scene-graph path for GPU upload if `QPainter::drawImage` can't hold 30 fps.

---

## 3. Feature surface (what playback means here)

- **Still image** — show the latest received frame (a snapshot widget). Simplest; ships first.
- **Live video** — continuous MJPEG playback at the source fps; drop late/incomplete frames; show a
  "frames dropped / fps" health readout (parallels the Buffer/Chart status strip).
- **Replay / scrub** (optional, bigger): align media frames to the signal timeline (`TimeAxisManager`)
  so a video panel scrubs together with plots in a recorded session — a real differentiator for
  bring-up ("what did the camera see when this signal spiked?"), but it needs media in the session
  recording + a deeper media buffer. **Likely a later slice, not M35 P1.**
- **Sync with signals** — media frames carry `device_ts`; mapping onto the shared time axis is what
  enables the scrub story. For live, latest-frame is enough.

---

## 4. UI (where media lives)

The natural home is a **new dashboard panel type** — `VideoPanel` / `ImagePanel` — alongside
Plot/Bar/Gauge/Numeric/State/Table. It fits the existing `makePanel()` switch + `PanelConfig` +
drag/resize/inspector machinery (Serial Studio similarly has video widgets). It binds to a **media
source id** the way a plot binds to signals.

- **Render**: `QPainter::drawImage` for simplicity; escalate to a `QQuickWidget` if fps demands GPU.
- **Cadence**: repaint on frame arrival (push), independent of the dashboard timer — the per-component
  rate design above.
- **Panel inspector** (M34 P5): media-specific properties — source, fit/stretch, fps readout, freeze.
- **Identity/colour** don't apply; the panel shows an image, not a signal trace.
- Open question: is media **only** a dashboard panel, or also a **first-class tier/mode** (its own rail
  entry like Raw/Parsed)? For one or few streams, a panel is right; many streams might warrant a tier.

---

## 5. Decisions the owner needs to make (before a plan)
1. **Source type → codec/transport:** custom device (→ custom-chunk MJPEG, **no deps**) vs real IP
   cameras (→ RTP/RTSP/H.264, **needs a dependency + §4.1 amendment**). *This is the load-bearing one.*
2. **Scope of M35 P1:** still image + live MJPEG only, or also replay/scrub + signal-time sync?
3. **Compressed video (H.264) in M35, or deferred?** (It's the only part that forces a new dependency.)
4. **UI home:** dashboard panel only, or a first-class media tier?

## 6. Risks / notes
- **Dependency creep** — H.264 is the one path that breaks the no-new-deps rule; keep the decoder
  behind an interface so MJPEG ships dep-free and H.264 is an owner-gated add-on.
- **Perf** — 30 fps × multi-MB frames is the highest-bandwidth path in the app; needs its own thread +
  benchmarks (CLAUDE.md §5).
- **Memory** — bound the media buffer explicitly; QImages are large.
- **Schema reuse** — media frames likely need their own descriptor (resolution/encoding/source),
  separate from the scalar decode schema.
- A real **device emulator** (Python, like the UDP signal feeder) that streams chunked MJPEG will be
  needed for live testing — stdlib `socket` + a canned JPEG sequence, no deps (mirrors
  `tests/integration/gui/helpers/udp_high_rate_feeder.py`).
