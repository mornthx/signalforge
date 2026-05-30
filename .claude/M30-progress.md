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

## S2 — parsed-signals table (`src/inspect/`)  ⏳ (next)
## S3 — tabbed workspace shell  ⏳
