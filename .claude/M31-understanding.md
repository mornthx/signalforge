# M31 — Wireshark raw-packet view (understanding)

Phase C of the three-tier recovery ([DR-002](direction-review/DR-002-2026-05-30-three-tier-workbench.md),
design `docs/v0.4/three-tier-workbench-design.md`). Branch `milestone/M31` (local, off M30).

## Goal

**Tier 1 (原报文)** — a Wireshark-style raw-packet view: a packet list + hex pane + display filter,
fed by a read-only tap on the frame pipeline, reusing the M30 `signalforge_query` engine. Added as the
leftmost **Raw** workspace tab.

## Scope (honest MVP + explicit deferral)

Wireshark's three panes are list / **dissection tree** / hex. The schema-driven dissection tree (mapping
decode rules to byte ranges per protocol layer) is a large sub-feature; **M31 ships packet-list + hex +
filter** (the raw-inspection + "尤其是筛选" core) and **defers the dissection tree to a follow-up (M32)**,
documented. This is stated up front per the no-silent-caps rule.

## Delivers

- **S1 — `RawFrameTap`** (`src/inspect/`): a `pipeline::FrameSink` that ring-buffers recent `RawFrame`s
  from every attached pipeline (aggregated across drivers). Thread-safe: pipeline threads push via
  `onFrame`, the UI polls `snapshot()`/`since(index)`. Mutex-guarded deque, capped. Cross-thread →
  concurrent stress test (rule #6; no TSan preset exists, so a concurrent writer/reader test like
  `signal_buffer_concurrent_test`, TSan being the CI gate per the ASan-in-CI convention).
- **S2 — `RawPacketView`** (`src/inspect/`): packet list (No · Time · Source · Proto · Len · Info) + a
  read-only hex pane for the selected frame + a filter bar over `no/source/proto/len/seq/hex/ascii`.
  Polls the tap on a timer. QTest interaction: typing a filter narrows rows; selecting a row shows hex.
- **S3 — wire the Raw tab** (`src/app/main_window`): construct the tap, register it on every pipeline via
  `PipelineManager::pipelineAttached`, mount `RawPacketView` as workspace tab 0 (Raw | Parsed |
  Dashboard). Re-accept any changed visual baseline.

## Decisions (decide-and-log)

- One shared tap across all pipelines → the Raw view shows every driver's packets together (Wireshark
  model). Lifetime: `main_window` holds the `shared_ptr`; pipelines hold weak references via `addSink`.
- Tab order Raw | Parsed | Dashboard (pipeline order); **default landing stays Parsed** (tier 2, the most
  generally-useful working view) — Raw is one tab left. Flagged for the owner if they want literal
  raw-first default.
- Filter exposes `hex` (payload as a hex string) + `ascii` (latin1) so `hex contains ff` works.

## DoD

Build Debug+Release green; ctest green both; concurrent test for the tap; QTest interaction for the view;
clang-format clean; local commits, no push. No frozen interface changed (FrameSink is implemented, not
modified; additive main_window wiring).
