# M22 — Dashboard P1 (progress)

Branch: `milestone/M22` (local, off `milestone/M21`; not pushed).

| Subtask | Status | Commit |
|---------|--------|--------|
| Planning (understanding/plan) | done | (this commit) |
| S1 — TablePanel + Panel base virtuals | done | TablePanel + virtuals; 2 cases/13 assertions (Debug+Release); plot test still green |
| S2 — Dashboard integration (no downcasts) | done | addTablePanel + Table case + polymorphic removePanel/removeSignalEverywhere; 2 downcast warnings eliminated; dashboard_test 4 cases/25 assertions |
| S3 — MainWindow + Table + close-out | done | +Table toolbar action + --auto-add-table hook; 00-empty-launch rebaselined for +Table; full ctest 665/665; smoke incl Tier D green |

## Log

- 2026-05-29: Branched M22 off M21 (P0 + C2 fix). Plan written.
