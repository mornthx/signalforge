# V0.2-era Baseline Archive (INDEX)

This directory archives the 12 V0.2 production-fidelity visual
baselines that were canonical at V0.2 close (milestone M15). They
are preserved here for **R12 governance trail** purposes: a
historical record of the rendering snapshot that the V0.2 close
gate signed off, against which V0.3 M16's "Visual Identity
Ownership" milestone improved.

These files are **frozen**. Do not modify them. If a future
forensic question arises ("did baseline X drift between V0.2
and V0.3?"), this archive is the canonical V0.2 reference.

## Provenance

| Field | Value |
|---|---|
| Era | V0.2 (M15 close baseline set) |
| Last-modified commit | `6ab0e34` — "fix: M15 S3 — re-baseline against CI xvfb Qt rendering (R8/R9)" |
| Originally accepted commit | `1f4524b` — "M15 S3 — operator-accepted 12 production-fidelity baselines" |
| V0.2 close report | `.claude/M15-done.md` §3 (final baseline inventory) |
| Rendering stack | Qt 6.10.2 + OS-fallback theme + xvfb software RHI (no SignalForgeStyle) |
| Capture host | CI Azure runner (per R9 cross-env coupling: CI capture canonical because operator-local rendering at V0.2 close differed from CI) |
| Replaced by | V0.3 M16 deterministic baselines at `tests/visual/baselines/` (commit at S7) |

## Why archived (not deleted)

Per V0.3 charter §6 R12 (baseline regression discipline) +
the M16 close-gate empirical-validation discipline:

1. **R12 governance trail** — every baseline transition is
   recorded. A future R12 application that asks "did the
   M16-to-MXX transition preserve cross-environment determinism?"
   can compare MXX captures against M16 baselines AND against
   these V0.2-era baselines to track the multi-milestone arc.
2. **Cross-V-series forensic** — V1.0 ships V0.3 M16's
   visual identity. If a V1.0 user reports "the UI used to look
   different in earlier versions, did something regress?", the
   archived V0.2 baselines are the V0.2-era reference.
3. **R9 cross-env coupling lesson** — V0.2 baselines were
   accepted against CI rendering specifically because operator
   local was misaligned with CI under V0.2's OS-fallback
   rendering stack. V0.3 M16 eliminated that misalignment by
   bringing SignalForge under SignalForgeStyle (deterministic
   cross-environment). The archive preserves the empirical
   evidence of the pre-M16 OS-coupling.

## Why these specific 12 baselines?

The 12 listed below are the V0.2 production-fidelity subset
accepted by the operator at M15 S3 close after R7/R8/R9
review. Other V0.2 captures (e.g., state 05 chart-with-signal,
state 01 multi-chart) were rejected at V0.2 close as
NON-FIDELITY (`d` verdict per M15-done.md §3) and have no
V0.2-era baseline.

| # | State | Captured behaviour at V0.2 |
|---|---|---|
| 1 | `00-empty-launch.png` | MainWindow chrome, no fixture, no signals, no chart line |
| 2 | `02-conn-udp-idle.png` | One UDP driver registered, NOT connected (`--auto-no-connect`) |
| 3 | `04-conn-udp-connected.png` | One UDP driver auto-connected via m14_smoke fixture |
| 4 | `12-multi-2-drivers.png` | 2 UDP drivers (m15_multi_2 fixture) both Connected |
| 5 | `13-multi-5-drivers.png` | 5 UDP drivers (m15_multi_5 fixture) all Connected |
| 6 | `24-dialog-add-serial.png` | Connection-add dialog with Serial driver pre-selected |
| 7 | `25-dialog-add-udp.png` | Connection-add dialog with UDP driver pre-selected |
| 8 | `26-dialog-edit.png` | Connection-edit dialog populated from m14_smoke fixture |
| 9 | `30-menu-file-open.png` | File menu open (no fixture) |
| 10 | `31-menu-connections-open.png` | Connections menu open (no fixture) |
| 11 | `32-menu-session-open.png` | Session menu open (no fixture) |
| 12 | `33-status-buffer-normal.png` | UDP fixture connected + signal selected; buffer < 80% |

All 12 baselines were CI-captured under V0.2's xvfb + OS-
fallback Qt rendering. They are byte-identical to the
`tests/visual/baselines/*.png` files as of commit `6ab0e34`
(pre-S7); S7 replaces those canonical paths with V0.3 M16
deterministic captures.

## Verification

To confirm a file in this archive is the V0.2-era baseline
unmodified, compare its sha256 against the
`tests/visual/baselines/<state>.png` snapshot at commit
`6ab0e34`:

```bash
git show 6ab0e34:tests/visual/baselines/00-empty-launch.png \
  | sha256sum
sha256sum tests/visual/baselines-v0.2-archive/00-empty-launch.png
```

Both digests should match exactly.

## Migration trail

| Milestone | Baseline state |
|---|---|
| V0.2 close (M15) | These 12 captures committed as canonical at `tests/visual/baselines/*.png`. |
| V0.3 M16 S6 | Cross-environment verification re-captured all 12 under SignalForgeStyle on operator-local + CI. Surfaced `SignalBufferRegistry::signalIds()` non-determinism (ADR-014) and status-bar live-counter drift (S6.6 mask). M16 close gate empirically validated 12/12 PASS. |
| **V0.3 M16 S7 (this commit)** | **V0.2 baselines moved to this archive. M16 deterministic captures + S5 env sidecars + S6.6 masks installed as new canonical at `tests/visual/baselines/`.** |
| V0.3 M17/M18 (future) | If widget/workflow rebuilds change visual output, R12 + R8 disciplines apply: M17/M18 captures must clear `compare_with_contract` cross-env gate before this archive's frame of reference is superseded. The M16 baselines at `tests/visual/baselines/` will then themselves be archived. |

## Frozen status

This directory is **append-only**: no file is ever modified
post-creation. The archive is the historical record. New
archived eras (M16-era, M17-era, ...) will live in sibling
directories (e.g. `baselines-m16-archive/`) when their time
comes; this one stays untouched.

If a future operator finds these files in their way (search
results, IDE indexing), the answer is "yes, they're meant to
be here — they're the V0.2 historical record."

## Cross-references

- `.claude/M15-done.md` §3 (V0.2 close baseline inventory)
- `docs/v0.3/s6-cross-env-verification.md` (the M16 cross-
  environment verification report this archive enables)
- ADR-014 (signal-tree determinism fix that improved on
  V0.2-era rendering)
- V0.3 charter §6 R12 (baseline regression discipline)
- V0.3 charter §amendment (visual identity ownership goal)
