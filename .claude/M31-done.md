# M31 — Wireshark raw-packet view (closure report)

Phase C of the three-tier recovery ([DR-002](direction-review/DR-002-2026-05-30-three-tier-workbench.md)).
Branch `milestone/M31` (local, off M30; **not pushed**). Completes the three-tier workbench.

## Delivered

- **S1 — `RawFrameTap`** (`3f0ce07`): a thread-safe `pipeline::FrameSink` ring-buffering recent
  `RawFrame`s from every pipeline (one shared tap → all drivers' packets together). Concurrent
  producers+reader stress test (5 cases / 10020 assertions).
- **S2 — `RawPacketView`** (`93874e0`): Tier-1 packet list (No · Time · Source · Proto · Len · Info) over
  a hex pane, with a display-filter bar reusing `signalforge_query` over `no/source/proto/len/seq/hex/
  ascii`. Selecting a packet shows a Wireshark-style hex dump. 3 cases / 15 assertions.
- **S3 — Raw tab** (`4439c18`): `main_window` creates the tap, registers it on each pipeline via
  `PipelineManager::pipelineAttached`, and mounts the view as workspace tab 0 (**Raw | Parsed |
  Dashboard**).

## Three-tier workbench — complete

| Tier | Tab | Module |
|---|---|---|
| 1. 原报文 (raw packets, Wireshark-style) | **Raw** | `inspect::RawPacketView` + `RawFrameTap` |
| 2. 解析数据 (filtered signal table) | **Parsed** | `inspect::ParsedSignalsView` |
| 3. Dashboard (cards/charts) | **Dashboard** | `dashboard::Dashboard` |

All three observation surfaces the architecture (§3.2/§7.2) always intended are now built. The
`signalforge_query` filter engine is shared by the Raw and Parsed tiers ("尤其是筛选").

## Deferred (documented, not silently dropped)

- **Schema-driven dissection tree** (Wireshark's middle pane: protocol layers → fields → byte ranges) is
  a large sub-feature requiring decode-rule→byte-range introspection. Deferred to **M32**. M31 ships the
  packet list + hex + filter, which is the raw-inspection + filtering core the owner asked for.
- Default landing tab stays **Parsed** (tier 2, most useful working view); Raw is one tab left. Owner may
  prefer literal raw-first — easy to change.

## Verification

- 705/705 ctest Debug + Release. One visual baseline (`00-empty-launch`) re-accepted (now 3 tabs).
  clang-format clean. No frozen interface or schema touched — `FrameSink` is *implemented* (read-only
  tap), not modified; main_window wiring is additive.

## Status

Local on `milestone/M31`. Chain: `main → M21 … → M30 → M31`. DR-002 Phases A+B+C all landed locally.
