# M30 — Parsed-signals view + filter engine (understanding)

Phase B of the three-tier recovery ([DR-002](direction-review/DR-002-2026-05-30-three-tier-workbench.md),
design `docs/v0.4/three-tier-workbench-design.md`). Branch `milestone/M30` (local, off M29).

## Goal

Build **Tier 2 (解析数据)** — a live, zero-config table of every decoded signal — plus the **reusable
filter engine** that is the core of "向 Wireshark 看齐 … 尤其是筛选". Introduce the tabbed center
workspace (architecture §7.1/§7.2) so the parsed table is the **default landing surface** and the
dashboard becomes a tab on top — structurally satisfying report 4 ("默认应该首先显示原始数据；dashboard
是在原始数据基础上的页面").

## Delivers

- **S1 — filter engine** (`src/query/`, leaf module, Qt6::Core only): a display-filter expression
  parser + evaluator over an abstract field lookup, so Tier 1 (frames, M31) reuses it verbatim.
  Grammar: `field OP value` (`== != < <= > >= contains`), combined with `&& || !` and parentheses.
  Empty filter matches all. Returns a parse error (message + position) for bad input. Pure logic,
  unit-tested in isolation — no UI.

- **S2 — parsed-signals table** (`src/inspect/`): a `QWidget` listing every registered signal — name ·
  source(driver) · value · unit · type · freshness(age) — refreshed on a timer, with a filter bar
  driving the S1 engine (invalid expressions flagged, not applied). Reuses `dashboard/value_format.hpp`
  (header-only). QTest interaction tests: typing a filter narrows the visible rows.

- **S3 — tabbed workspace shell** (`src/app/main_window`): a `QTabWidget` center with **Parsed**
  (default) and **Dashboard** tabs; the signal-list dock stays. Additive — M31 adds a **Raw** tab.

## Decisions (decide-and-log)

- New leaf module `src/query/` keeps the engine reusable and dependency-free; `FieldValue =
  variant<double,bool,QString>` (engine-local, decoupled from `decoder::SignalValue`).
- Parsed view in a new `src/inspect/` module (tier-2 ≠ dashboard); avoids bloating the dashboard module.
- Freshness is derived from `LatestValue.age` (fresh/stale threshold) — the buffer exposes age, not a
  quality enum; a coarse fresh/stale is enough for v1 (architecture's GOOD/STALE/UNCERTAIN/BAD is richer,
  noted for later).

## DoD

Build Debug+Release green; ctest green on both; ≥70% on the engine's public surface; QTest interaction
tests for the parsed view (real filter typing); clang-format clean; local commits, no push.
No frozen interface is touched (all-new modules + additive main_window wiring).
