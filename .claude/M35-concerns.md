# M35 — Concerns

## C1 — M35 has no defined scope (blocks the plan)
The v0.4 redesign plan (P0–P5) is fully delivered by M34, and no `docs/milestones/M35` spec exists.
Per CLAUDE.md ("when the spec is ambiguous, do not proceed; never guess silently"), I am **not**
writing a speculative `M35-plan.md`. Two best interpretations, with their implication differences:

- **Interpretation A — Control mode (the reserved capability).**
  Build the command/control path: send commands to the device (handshake / polling / manual dispatch
  / macros / ACK matching), with the `≈ Control` rail slot becoming a first-class mode.
  *Implications:* large, new outbound-IO surface (drivers must support write/command framing — check
  `DriverInterface::write` state machine, already present); new UI tier; new tests + benchmarks
  (perf-sensitive outbound path). Highest product value (advances the pipeline) but biggest scope.

- **Interpretation B — Polish & harden the redesign.**
  Clear the `M34-progress.md` backlog (two-style highlight, rename→panel titles, strict cascade, Raw
  filter autocomplete, Raw selection observer) + re-tighten the visual CI gate (CI-env baselines) +
  general hardening.
  *Implications:* small, low-risk, fast green; no new core capability. Good if the owner wants to
  stabilise before the next big feature.

These are mutually exclusive as a *primary* M35 theme (B could also be folded into A's early slices).

## Resolution
Await the owner's pick (A / B / other). On decision, write `M35-plan.md` and present for the
Phase-4 execute approval.
