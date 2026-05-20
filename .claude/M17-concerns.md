# M17 Concerns Log

This file tracks open and resolved concerns during M17 execution.
Concerns are appended as they surface; resolution status updates inline.

## Format

Each entry has:

- **ID** (`C<n>`) — stable identifier across the milestone.
- **Status** (`open` / `resolved` / `carry-forward`).
- **Surfaced at** — subtask + date.
- **Body** — what / why / two best interpretations / impact.
- **Resolution** — if applicable.

---

## C0 — Local ASan preset blocked by `/etc/ld.so.preload`

**Status**: open (carry-forward from prior milestones).
**Surfaced at**: S0 (2026-05-20).

Per saved memory `host_asan_preload.md`, the development host has
`/etc/ld.so.preload` injecting another ASan runtime, which prevents
the `debug-asan` CMake preset from running locally. CI is the
authoritative gate for ASan + UBSan certification per CLAUDE.md
§Required #2.

**Resolution**: CI remains the authoritative ASan gate. M17 closure
in §S6 verifies CI run includes the debug-asan job green.

---

## C1 — Aggregate-state precedence is CC-authored

**Status**: open (carry to PR review).
**Surfaced at**: S0 (2026-05-20).

M17 spec §6.1 defines a 4-rank precedence:

1. any Error → status-error
2. any Connecting/Disconnecting → status-connecting
3. N≥1 && all Connected → status-connected
4. otherwise → status-idle

Two-best-interpretations:

- **Interpretation A** (spec choice): Error precedence is highest —
  the operator should see red the moment any connection breaks, even
  if 9 of 10 connections are healthy.
- **Interpretation B**: Connected precedence is highest when N is
  large — show green if a majority are connected, with a small "K
  errored" sub-indicator separately styled. This mirrors the existing
  text content "X/N connected · errors: K".

A's rationale: red is an attention-grabber; in a bring-up workbench
the operator usually wants to know immediately if anything is wrong.
B's rationale: visual screaming when 1 of 10 is bad is overkill for
production fleets.

**Implementation**: Interpretation A is chosen for M17 (matches the
manifesto's "alarming red for genuine errors" intent in
`visual-identity.md` §accessibility). M17 PR review may surface a
preference for B; if so, flip the precedence in S1 and re-capture
`31-with-errors`.

**Resolution**: pending PR review.

---

## C2 — `connection_list_widget.cpp` token mirror is duplication

**Status**: resolved at S2 (2026-05-21).
**Surfaced at**: S0 (2026-05-20).

**Original concern**: Per spec §6.2 Option B, the per-row state colour
is set via `Qt::ForegroundRole` directly from a `QColor`. At
spec-drafting time I assumed the colour values would be mirrored from
`tokens.json` `palette.light.status.*` in C++ code in
`connection_list_widget.cpp`, creating drift risk.

**Resolution at S2**: M16's `src/app/generated_style_tokens.hpp`
already exposes per-status QColor accessors
(`tokens::light::statusConnected()`, `statusError()`, etc.) generated
from `tokens.json`. The header is `#include`-only — no link
dependency on `signalforge_app`. `ConnectionListWidget::colorForState`
consumes the generated accessors directly, so the row colour and the
QSS class colour share a single source of truth. No mirror, no
DEBUG assertion needed.

The S2 unit test `M17 S2: colorForState returns token-consistent QColors per state`
locks the binding in place: any token regeneration that changes a
status hex without updating the test target will fail the assertion.

---

## C3 — Visual-baseline shift due to new panelHeader chrome

**Status**: confirmed at S4 (2026-05-21); resolution at S5.
**Surfaced at**: S0 (2026-05-20).

Both SignalSelector (S3) and ConnectionListWidget (S2) gain a new
`QFrame#panelHeader` row at the top of their layouts. Per
`tokens.qss` line 56 the panel header is ~28 px tall (4 px top + 4
px bottom padding + 1 px border-bottom + label baseline). Any of the
12 M16 baselines that capture either dock will have their post-header
content shifted down by ~28 px.

**Affected M16 baselines** (worst-case enumeration; verified at S5):

| Baseline | Captures SignalSelector dock? | Captures Connections dock? | Likely affected? |
|---|:---:|:---:|:---:|
| 00-empty-launch | yes | yes | yes |
| 01-…-32 | per-state | per-state | per-state |

At S5 each affected baseline gets either:

- A pixel-exact re-capture + R8 acceptance, with rationale "M17 chrome
  shift — new panelHeader row" (R8 §s7-baseline-migration precedent).
- Or, if the shift is < 1 % per the visual-diff contract, no
  re-acceptance needed.

**S4 confirmation (2026-05-21)**: Running the visual ctest suite after
S4 build, `00-empty-launch` reports:

- diff = 3.135 % (gate: < 1 %)
- max cluster = 15 906 px (gate: < 200 px)
- 9 clusters across 31 885 differing pixels
- masked region: 7 040 px (the universal status-bar mask is in effect
  and not affected)
- env_drift: [] (env contract clean)

This is the SignalSelector + ConnectionListWidget panelHeader chrome
shift exactly as predicted. The other baselines (02/04/12/13/24/25/26/30/31/32/33)
have cached screenshots from M16 captures that survived the build, so
their pytest cases pass without re-capturing — they will need fresh
capture + R8 review at S5.

**S5 resolution (2026-05-21)**: All 12 M16 baselines invalidated by
clearing `tests/screenshots/*.png`, then full visual ctest captured
fresh under the M17 build. Per-state pre-acceptance diff vs M16
baseline:

| State | Pre-accept diff | Max cluster | Verdict |
|---|---:|---:|---|
| 00-empty-launch | 3.135 % | 15 906 px | shifted (panelHeader chrome) |
| 02-conn-udp-idle | 3.282 % | 15 905 px | shifted (panelHeader chrome + grey idle label) |
| 04-conn-udp-connected | 4.180 % | 15 904 px | shifted (panelHeader chrome + green connected label) |
| 12-multi-2-drivers | 5.272 % | 15 868 px | shifted (panelHeader chrome + green labels + per-row colours) |
| 13-multi-5-drivers | 6.499 % | 15 870 px | shifted (panelHeader chrome + green labels + per-row colours) |
| 24-dialog-add-serial | 1.297 % | 12 586 px | shifted (panelHeader visible behind modal) |
| 25-dialog-add-udp | 1.297 % | 12 586 px | shifted (panelHeader visible behind modal) |
| 26-dialog-edit | 1.470 % | 12 584 px | shifted (panelHeader visible behind modal) |
| 30-menu-file-open | 3.135 % | 15 906 px | shifted (panelHeader visible behind menu popup) |
| 31-menu-connections-open | 2.891 % | 13 422 px | shifted (panelHeader visible behind menu popup) |
| 32-menu-session-open | 3.135 % | 15 906 px | shifted (panelHeader visible behind menu popup) |
| 33-status-buffer-normal | 4.188 % | 15 904 px | shifted (panelHeader chrome + green labels) |

All 12 baselines R8-accepted via `scripts/accept-baseline.sh <state> ""`.
Acceptance promotes the new PNG + env.json (and any existing mask.json).

Plus 1 new M17 baseline:

| State | Coverage |
|---|---|
| 34-conn-replay-error | Replay-driver pointing at missing path → Error state → ConnectionStatusWidget renders class="status-error" red + ConnectionListWidget row foreground red |

Total post-M17 baseline matrix: **13 baselines** (12 M16 + 1 new M17),
all green under the visual-diff contract on debug + release presets.

Scope reduction from spec §3.2: original plan called for 2 new M17
baselines (`30-all-connected` + `31-with-errors`). The "all-connected"
case is already captured by re-accepted `04-conn-udp-connected` (now
showing M17's green status label); only the genuine new state
(`34-conn-replay-error`) needed a fresh capture. Spec state numbers
30/31 are already in use for menu states; M17's new state uses 34.

---

## C4 — New baseline 34-conn-replay-error lacks universal status-bar mask

**Status**: resolved at S6 follow-up (2026-05-21).
**Surfaced at**: S6 CI run `26176356451` (push trigger on commit
`798e705`, debug-asan job).

**Symptom**: CI debug-asan job failed at
`M15-visual-test_states_production_fidelity` with:

```
FAIL test_baseline_34_conn_replay_error: visual regression:
  state='34-conn-replay-error' diff=0.253% max_cluster=1067px
  note=diff=0.253% threshold=1.0% max_cluster=1067px threshold=200px
```

Diff percentage 0.253 % (well under 1 % gate) but max cluster 1 067 px
exceeded the 200 px gate.

**Root cause**: state `34-conn-replay-error` was a new M17 capture and
did not carry the universal status-bar live-counter mask that all 12
M16 baselines have (per `docs/v0.3/s6-cross-env-verification.md` §11,
mask region `x=615, y=778, w=320, h=22` covering FPS / Dropped /
throttled / buffer-budget labels which drift sub-percent cross-host /
cross-runtime-config). Note the cluster size **1067 px** matches the
M16 S6.5 ADR-014 measurement exactly — same physical pattern.

The same commit's PR-trigger run (`26176372155`) passed because the
debug-asan timing happened to land the live-counter values in
positions whose cluster fit under 200 px; this is *flake*, not
*fix*. The mask is the deterministic fix.

**Why the local capture didn't see this**: development host runs
without ASan, with stable rendering throughput. The live-counter
labels render identically on consecutive captures and identically at
the baseline-capture moment. Under ASan + CI timing, the counters
drift, exposing the cluster.

**Resolution**: added
`tests/visual/baselines/34-conn-replay-error.mask.json` mirroring the
M16 universal pattern (same rectangle, same rationale lineage, R8
single-approval). Concerns C3 + C4 both close at this commit.

**Lesson for M18 / widget-styling-guide §12.5 follow-up**: any new
state that captures the status bar inherits the universal mask
requirement. The mask should be authored alongside the baseline, not
in response to a CI failure.

(Concerns C5+ added as future M17 execution surfaces them; this
milestone closes at C4.)
