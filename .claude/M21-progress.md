# M21 — Dashboard P0 (progress)

Branch: `milestone/M21` (local; not pushed). Baseline: `main` @ a647ecf, debug build green.

| Subtask | Status | Commit |
|---------|--------|--------|
| Planning (understanding/plan/concerns) | done | (this commit) |
| S1 — Panel abstraction + factory | done | panel base + factory; 4 test cases / 26 assertions green (Debug+Release) |
| S2 — Numeric + State panels | done | numeric+state panels + value_format; 4 cases/22 assertions (Debug+Release); surfaced concern C1 (slow-signal publish latency) |
| S3 — PlotPanel (wrap legacy Chart) | done | hosts QQuickWidget+Chart, detaches on dtor (no double-free); 2 cases/13 assertions (Debug+Release) |
| S4 — Dashboard container | done | reflow grid + 15Hz refresh + auto-suggest addSignal + plot/chart lifecycle; 3 cases/16 assertions (Debug+Release) |
| S5a — dashboard-aware signal list | done | SignalListPanel + showsSignal; 2 cases/11 assertions |
| S5b — MainWindow integration | done | dashboard mounted in central splitter, selector→dashboard routing, +Plot, hooks repointed, QPointer chart-safety, 00-empty-launch rebaselined; full ctest green (662/662); GUI smoke green; found pre-existing teardown UAF (C2) |
| S6 — Close-out (format/docs/full ctest) | done | clang-tidy fix (bugprone-optional), M21-done.md, full ctest 662/662 |

## Log

- 2026-05-29: State observed (clean, ahead 5), baseline debug build green, branch created.
  Plan written. Decisions D1–D4 in M21-concerns.md.
