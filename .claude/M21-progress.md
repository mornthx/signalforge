# M21 — Dashboard P0 (progress)

Branch: `milestone/M21` (local; not pushed). Baseline: `main` @ a647ecf, debug build green.

| Subtask | Status | Commit |
|---------|--------|--------|
| Planning (understanding/plan/concerns) | done | (this commit) |
| S1 — Panel abstraction + factory | done | panel base + factory; 4 test cases / 26 assertions green (Debug+Release) |
| S2 — Numeric + State panels | done | numeric+state panels + value_format; 4 cases/22 assertions (Debug+Release); surfaced concern C1 (slow-signal publish latency) |
| S3 — PlotPanel (wrap legacy Chart) | todo | |
| S4 — Dashboard container | todo | |
| S5 — MainWindow integration | todo | |
| S6 — Close-out (format/docs/full ctest) | todo | |

## Log

- 2026-05-29: State observed (clean, ahead 5), baseline debug build green, branch created.
  Plan written. Decisions D1–D4 in M21-concerns.md.
