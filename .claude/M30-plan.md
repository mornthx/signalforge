# M30 — plan (Phase B)

Each subtask = one local commit after build+ctest+clang-format green.

## S1 — filter engine (`src/query/`)

Files: `src/query/CMakeLists.txt`, `filter_expr.{hpp,cpp}`; `tests/unit/query/filter_expr_test.cpp`.
Wire `add_subdirectory(src/query)` before `src/dashboard` in root `CMakeLists.txt`.

- `FieldValue = std::variant<double, bool, QString>`; `FieldLookup = std::function<std::optional<FieldValue>(const QString&)>`.
- `FilterExpr::parse(QString) -> ParseResult { std::optional<FilterExpr> expr; QString error; int errorPos; }`.
- `FilterExpr::matches(const FieldLookup&) const -> bool`; empty source → matches-all.
- Tokenizer + recursive-descent parser → AST (And/Or/Not/Compare nodes). Numeric vs string compare by
  coercion; `contains` = case-insensitive substring; missing field → comparison false.
- Tests: literals, all operators, &&/||/! precedence, parentheses, quoted strings, numeric coercion,
  missing field, parse errors (position reported). ≥70% public-surface coverage.

## S2 — parsed-signals table (`src/inspect/`)

Files: `src/inspect/CMakeLists.txt`, `parsed_signals_view.{hpp,cpp}`; test
`tests/unit/inspect/parsed_signals_view_test.cpp`. Depends on buffer + query + Qt Widgets; includes
`dashboard/value_format.hpp` (header-only).

- A `QWidget`: filter `QLineEdit` on top + a `QTableView`/`QTableWidget` of all signals
  (Name · Source · Value · Unit · Type · Age). Rebuilds rows from the registry; refresh timer updates
  values + age; freshness from `LatestValue.age`.
- Filter: parse on text change; if valid, hide non-matching rows; if invalid, mark the field (no crash,
  rows unchanged). Field names exposed to the engine: `name`, `source`, `unit`, `type`, `value`, `id`.
- Tests (simulate real use): seed registry + push values; type `unit == kPa` → only matching rows
  visible; type a bad expression → all rows stay + error state; clear → all visible.

## S3 — tabbed workspace shell (`src/app/main_window`)

- Wrap the center (currently `chartContainer_` holding the dashboard) and the new parsed view in a
  `QTabWidget`: tab 0 **Parsed** (default selected), tab 1 **Dashboard**. Keep the signal-list dock and
  toolbar. Grab/screenshot paths still resolve the dashboard's PlotView.
- A harness flag (e.g. `--start-tab parsed|dashboard`) for deterministic visual capture if needed.
- Tests: main_window test asserts both tabs exist and Parsed is default; existing app tests still pass.
  Re-baseline any visual states that now show the tab bar (expected; via `scripts/accept-baseline.sh`).

## Close-out

`.claude/M30-progress.md` per subtask; `.claude/M30-done.md` at the end. No push.
Then proceed to **M31 (Phase C)**: Wireshark raw-packet view reusing the S1 engine.
