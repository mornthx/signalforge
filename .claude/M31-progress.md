# M31 — progress (Phase C)

## S1 — RawFrameTap  ✅
- `pipeline::FrameSink` ring-buffering recent `RawFrame`s (one tap shared across all driver pipelines).
  Mutex-guarded deque, capped; `onFrame` on pipeline threads, `snapshot()/since(index)/totalCaptured()`
  on the UI thread. `CapturedFrame` = flattened {index, source, protocol, recvAt, sequence, payload}.
- Cross-thread coverage (rule #6, no TSan preset): concurrent stress test — 4 producers × 5000 frames +
  a hammering reader; asserts total count + strictly-increasing indices, no crash/tear.
- Tests: 5 cases / 10020 assertions. Debug + Release green. clang-format clean.

## S2 — RawPacketView (packet list + hex + filter)  ✅
- QSplitter(vertical): packet list (No·Time·Source·Proto·Len·Info) over a read-only hex pane; filter bar
  over no/source/proto/len/seq/hex/ascii (reuses signalforge_query). Polls the tap; selecting a row
  shows a Wireshark-style hex dump. 3 cases / 15 assertions; clean exit under xcb (no clear-button).
- Debug + Release green; clang-format clean.
## S3 — wire the Raw workspace tab  ⏳
## Deferred: schema-driven dissection tree → M32 (documented).
