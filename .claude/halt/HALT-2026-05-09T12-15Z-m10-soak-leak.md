# HALT — M10 SessionWriter soak: H5 trigger fired

| Field | Value |
|---|---|
| Timestamp (UTC) | 2026-05-09T12:15Z |
| Milestone | M13 |
| Subtask | S4 (30-min memory soaks) |
| Trigger | H5 — `30-min soak fails (memory leak detected; > 10 % VmRSS growth)` |
| Source | M13 plan §3 / spec §3.5 V / spec §5.3 |
| Scope | M10 SessionWriter soak (`bench_session_writer --soak 1800`) |

---

## What happened

Per M13 plan §S4 + spec §3.5 V, the M10 30-min memory soak is
a release prerequisite. CC started the soak in background at
2026-05-09T11:50Z (with `--memory-snapshot 30` for 30-second
sampling).

VmRSS sampled at every 30-second tick exhibited **linear,
unbounded growth** (~14 MB / 30 s = ~470 KB / s sustained):

```
{"sec":30,  "vmrss_kb":25724,  "events_recorded":1800060}
{"sec":60,  "vmrss_kb":39788,  "events_recorded":3600120}
{"sec":90,  "vmrss_kb":53852,  "events_recorded":5400180}
{"sec":120, "vmrss_kb":67912,  "events_recorded":7200180}
{"sec":150, "vmrss_kb":81976,  "events_recorded":9000240}
{"sec":180, "vmrss_kb":96040,  "events_recorded":10800300}
{"sec":210, "vmrss_kb":110292, "events_recorded":12600300}
{"sec":240, "vmrss_kb":124356, "events_recorded":14400360}
{"sec":270, "vmrss_kb":138420, "events_recorded":16200360}
```

(Soak killed at sec ≈ 270 once the linear pattern was
unambiguous; full 9-snapshot data preserved at
`tests/benchmark/results/M13-soak-data/m10-soak.jsonl`.)

### Growth analysis

| Metric | Value |
|---|---:|
| Initial VmRSS (sec 30) | 25 724 KB |
| Final VmRSS (sec 270) | 138 420 KB |
| Absolute growth | **+112 696 KB (+110 MB)** in 240 s |
| Per-second rate | **+469 KB / s** |
| Per-event rate | ~7.8 bytes / event leaked |
| Projected 30-min final VmRSS | ~870 MB |
| Projected growth vs initial | **~3 300 %** |
| Spec §5.3 HALT bar | **> 15 %** |
| Spec §5.3 target | < 10 % |

The growth is **two orders of magnitude past the HALT
threshold**.

### Suspicious observations

- `bytes_written` reports `0` throughout the soak. The bench
  reads `writer.bytesWritten()` correctly per
  `tests/benchmark/bench_session_writer.cpp:144`. A zero
  byte-count + sustained 60 k events/sec input + 0 dropped
  + linear VmRSS growth is consistent with **events
  accumulating in memory rather than being flushed to disk**.
- `dropped = 0` throughout: no backpressure relief firing.
- Linear growth (constant +14 MB / 30 s) suggests an
  unbounded data structure, not a one-time spike.

These are **observations**, not diagnoses. Per the user's S4
failure-handling guidance ("Do NOT silently fix; do NOT
proceed to S5 until decision"), CC has not investigated the
M10 SessionWriter or SessionFileWriter source files.

---

## What's been completed before this HALT

| Subtask | Status | Commit |
|---|---|---|
| Pre-S0 (understanding + plan) | ✅ pushed | `de012a6` + `080fe28` (merge with M13 spec) |
| S0 (concerns) | ✅ pushed | `d7be620` |
| S1 (CMake CPack + install rules) | ✅ pushed | `8c6c255` |
| S2 (postinst / prerm / desktop entry / icon) | ✅ pushed | `f3fc467` |
| S3 (release notes / install / spec list / HW protocol) | ✅ pushed | `c0f36f1` |
| **S4 (M10 + M11 soaks)** | ⛔ **HALT (H5)** — M10 soak shows leak; M11 soak NOT started | partial data captured |
| S5 (integration tests) | ✅ written + tested locally; **not yet committed** | 602/602 ctest pass |
| S6 (done.md + freeze verify) | not started | — |

S5 was authored in parallel during the M10 soak (per concerns
C2 background-soak + parallel-doc plan). Both M13 integration
test files (`test_m13_release_artifacts.cpp` + `test_m13_deb_package.cpp`)
build clean and pass on Debug + Release. They are committable
as a stand-alone "S5 — integration tests landed pre-HALT"
commit if the user wants the work preserved before escalation.

---

## Decision options (per user S4 guidance)

CC awaits user input on disposition:

### Option A — File ADR-008 + `.cpp`-only hotfix in M13

- File `docs/architecture/decisions/ADR-008-m10-soak-leak.md`
  documenting the leak + the fix scope.
- Investigate the M10 SessionWriter / SessionFileWriter
  internals to identify where the unbounded growth
  originates.
- Apply a `.cpp`-only fix (no frozen-`.hpp` modification).
- Re-run the soak; verify < 10 % growth.
- This is the spec §2.2 #1 exception path — strictly,
  "no new code in V1 module surfaces", but ADR-008 is the
  authorised exception mechanism.
- Estimated effort: 2-4 hours investigation + fix + re-soak.

### Option B — Defer the leak to V1.0.1 patch milestone

- Document the leak as a known V1.0 limitation in
  release-notes / install.md.
- Mark spec §3.5 V acceptance as **partial** (CC reports
  "release prerequisite #1 failed; deferred to V1.0.1").
- M13 still ships V1.0.0 with the leak; users on
  long-running recordings will hit it.
- V1.0.1 patch milestone re-opens the soak gate after the fix.
- Estimated effort: ~30 min of doc-update only.
- Risk: ships V1.0 with a known leak; user-visible on
  long sessions.

### Option C — Re-run with different fixture parameters

- Re-run with reduced rate / different signal count to
  confirm the leak is real and not a fixture artefact (the
  `bytes_written = 0` observation is suspicious enough to
  warrant this).
- Outcomes:
  - Leak confirmed → escalate to A or B.
  - Fixture artefact → re-run with corrected fixture; soak
    passes; resume S5.
- Estimated effort: 30-40 min.

### Option D — Investigate what `bytes_written = 0` means first

- The `bytes_written` reading 0 throughout is the most
  suspicious data point. Check the M10
  `SessionWriter::bytesWritten()` implementation: does it
  return the worker's running byte count, or only the final
  count after `stop()`? If the latter, the soak harness
  isn't actually exercising disk I/O (events accumulating
  in queue).
- This is the cheapest investigation (5-10 min reading
  M10 source).
- May reveal that the "leak" is bench-fixture-side, not
  M10 SessionWriter-side.
- Estimated effort: 10 min.

CC's recommendation (subjective; for user judgment):
**Option D first** (cheap, may reveal it's not actually a
leak). If Option D confirms the leak is real, then Option A
(within-M13 fix via ADR-008) keeps V1.0 ship target on
schedule.

---

## Files preserved

- `tests/benchmark/results/M13-soak-data/m10-soak.jsonl`
  (9 snapshots; ~270 seconds of soak data)
- `tests/integration/test_m13_release_artifacts.cpp` (S5,
  uncommitted)
- `tests/integration/test_m13_deb_package.cpp` (S5,
  uncommitted)
- `tests/integration/CMakeLists.txt` (S5, uncommitted)
- `.claude/M13-progress.md` (will be updated with S4 H5 +
  S5 completion + this HALT reference at next commit)

---

## What CC will NOT do without user direction

- Investigate `src/session/session_writer.cpp` /
  `session_file_writer.cpp` internals
- Modify any M10 file
- Run the M11 soak (S4 second half)
- Proceed to S6 (done.md)
- Open the M13 PR
- Push any further commits without a clear decision

CC will commit the partial S4 + completed S5 work + this
HALT report as a single commit on `milestone/M13` (so the
state is preserved on origin) and stop.

---

## Awaiting

User decision on A / B / C / D (or alternative directive).
