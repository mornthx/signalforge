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

**Status**: open (resolution at S5).
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

**Resolution**: Tabulated at S5 close with per-baseline status in
`.claude/M17-done.md` §10.

---

(Concerns C4+ added as M17 execution surfaces them.)
