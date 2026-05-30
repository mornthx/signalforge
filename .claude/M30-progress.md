# M30 — progress (Phase B)

## S1 — filter engine (`src/query/`)  ✅
- New leaf module `signalforge_query` (Qt6::Core only). `FilterExpr::parse` (tokenizer +
  recursive-descent) → `matches(FieldLookup)`. Grammar: `field OP value` (`== != < <= > >= contains`)
  with `&& || !` + parens; empty filter matches all; parse errors carry message + position.
- `FieldValue = variant<double,bool,QString>`; engine-local (decoupled from decoder), reusable by M31.
- Decisions: string compares case-insensitive (friendlier for a filter box); numeric literal forces
  numeric coercion; `contains` always case-insensitive substring; missing field → non-match.
- Tests: 10 cases / 83 assertions (operators, precedence, coercion, quotes, negation, parse errors).
  Debug + Release green. clang-format clean.

## S2 — parsed-signals table (`src/inspect/`)  ✅
- New module `signalforge_inspect`: `ParsedSignalsView` — a `QTableWidget` of every signal
  (Name · Source · Value · Unit · Type · Age), 10 Hz refresh from the registry, with a filter bar
  driving the S1 engine over fields `id/name/source/unit/type/value`.
- Invalid filter → flagged (`invalid` property + tooltip), all rows stay visible; valid → narrows rows.
- Reuses `dashboard/value_format.hpp` (header-only). Tests simulate typing into the filter bar.
- **Teardown fix:** `QLineEdit::setClearButtonEnabled(true)` crashed `qt_call_post_routines` at exit
  under xcb (Qt 6.10) — dropped it (would have crashed the real app's exit too). Caught only because
  the test ran under the real X platform, not offscreen.
- Verified: 697/697 ctest Debug + Release; parsed_signals_view_test 3 cases / 12 assertions; fmt clean.

## S3 — tabbed workspace shell  ⏳ (next)
