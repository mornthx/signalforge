# M17 — V0.3 Widget Rebuild (UI / UX Evolution)

| Field | Value |
|---|---|
| Milestone | M17 |
| Track | V0.3 (post-M14 GUI track, follows M16 Visual Identity Ownership) |
| Date drafted | 2026-05-20 |
| Author | CC, autonomous draft based on M16-done.md §12 + `docs/v0.3/widget-styling-guide.md` (M16 S8 "M17 foundation") |
| Effort estimate | 4–6 person-days (subtask plan: 6 substantive subtasks + closure) |
| Hard-stop type | Visual-diff certification + widget-styling-guide conformance review |
| Net diff target | ≤ 800 net added lines (CLAUDE.md §Required #4) |

---

## 1. Goal

Turn the M16 visual-identity infrastructure into **operator-visible UI/UX
evolution** by rebuilding the three widgets named in M16-done.md §12 to
consume the M16 token system through the patterns documented in
`docs/v0.3/widget-styling-guide.md`.

M16 delivered:

- Bundled fonts, Fusion-locked style, 18-role palette, `tokens.qss` global
  stylesheet, deterministic baselines.
- A widget-styling-guide that names the patterns (`class` property,
  `objectName`, panelHeader, semantic colours) and identifies the three
  starting-widget candidates: **SignalSelector**, **ConnectionListWidget**,
  **ConnectionStatusWidget**.

But **no widget currently sets a `class` property, and no widget uses
`objectName="panelHeader"`** — the semantic selectors in `tokens.qss`
(`status-idle`, `status-connected`, `status-error`, …) are dormant. The
operator sees the M16 colour palette only in chrome (buttons, header
sections, default text); they do not yet see status-coloured connection
indicators or any state-aware visual feedback.

M17 makes the M16 manifesto **visible in interaction**. After M17:

- The status-bar connection counter renders in green when all connections
  are connected, yellow during transitions, red when any connection has
  errored.
- The connection list shows a per-row state pill / coloured label so the
  operator can scan state at a glance instead of reading the trailing
  "— Idle / Connected / Error" text.
- The signal selector exposes a count badge ("18 / 47 signals" when
  filtered) and renders its groups in deterministic sorted order, closing
  the ADR-014 thread one widget further.
- Panel headers (signal-selector header, connection-list header, status-bar
  area) use the `panelHeader` objectName pattern so tests, accessibility
  tooling, and any future theme work can target them by name.

M17 closes when **the M16 12 baselines plus 2 new M17 baselines** (one for
"all connected" green status, one for "with errors" red status) certify
under the visual-diff contract, and the widget-styling-guide §9 anti-patterns
table flags **zero violations** in the rebuilt widgets.

---

## 2. Out of scope

M17 deliberately does **not** include:

- **ChartConfigDialog rebuild** (M16-done.md §12 mentioned it, but it's a
  modal-dialog rebuild that is best bundled with the broader M18 workflow
  rebuild). M17 stays focused on always-visible chrome widgets so the
  visual-baseline impact is concentrated.
- **Dark theme activation** (M20 scope per `widget-styling-guide.md` §7).
- **New tokens** beyond what `tokens.json` already exports. M17 consumes
  existing tokens; if a gap appears it is recorded as a M17 concern, not
  an in-flight `tokens.json` edit, to keep M17 PR ≤ 800 net lines.
- **QML chart-surface restyling** (chart QML lives outside the Fusion +
  QSS surface; chart styling evolves separately under the M8 / M12 perf
  track).
- **Layout changes** to the main-window dock geometry. M17 changes what
  widgets look like, not where they live.

---

## 3. Deliverables

### 3.1 Source

| File | Change |
|---|---|
| `src/connection/connection_status_widget.{hpp,cpp}` | Add `AggregateState` enum + computation + `setProperty("class", …)` driven by connect/error/transition counts; emit a stable `objectName="connectionStatusLabel"`. |
| `src/connection/connection_list_widget.{hpp,cpp}` | Split each row into name/type/state cells with per-cell `QLabel` and `class` property for state. Add `panelHeader` QFrame above list. |
| `src/chart/signal_selector.{hpp,cpp}` | Add `panelHeader` QFrame with title; add filter count label below filter; sort groups + leaves (`std::map` / sorted `QStringList`); leaf check-state retains ADR-014 deterministic order. |
| `src/app/main_window.cpp` | Apply `panelHeader` objectName pattern to status-bar separator surfaces; add stable `objectName` to ConnectionListWidget / SignalSelector docks. |

No interface signature changes (CLAUDE.md §Forbidden #9). All M17 changes
are additive: new public methods, new `objectName`s, new `class`
properties. Frozen-surface counter target **0**.

### 3.2 Tests

| File | Change |
|---|---|
| `tests/unit/connection/connection_widgets_test.cpp` | Add per-state `class` property assertions for both `ConnectionStatusWidget` and `ConnectionListWidget` (decoupled from visual rendering per widget-styling-guide §8.2). |
| `tests/unit/chart/signal_selector_tree_test.cpp` | Add deterministic-order assertion + filter-count label assertion. |
| `tests/visual/baselines/30-all-connected.png` + `.env.json` (+ optional mask) | New visual baseline: M17 status-bar green when 1+ connection is connected. |
| `tests/visual/baselines/31-with-errors.png` + `.env.json` (+ optional mask) | New visual baseline: M17 status-bar red when 1+ connection has errored. |
| `tests/visual/baselines/12-..29-..` | M16 baselines re-captured (R8 acceptance) where M17 cosmetic changes shift pixels above the visual-diff cluster threshold. R8 stamps in §10 of M17-done.md. |

### 3.3 Docs

| File | Change |
|---|---|
| `.claude/M17-understanding.md` | This milestone's understanding doc. |
| `.claude/M17-plan.md` | Subtask sequencing. |
| `.claude/M17-concerns.md` | Concerns log. |
| `.claude/M17-progress.md` | Running progress log. |
| `.claude/M17-done.md` | Closure report (mirrors M16-done.md structure). |
| `docs/v0.3/widget-styling-guide.md` | Append §12 "M17 lessons" subsection if any new pattern was needed (e.g., aggregate-state computation for status-bar widgets). No back-edit of §1–§11. |

### 3.4 No new dependencies

CLAUDE.md §Forbidden #1: no new entries in `docs/architecture/architecture.md §4.1`.
M17 uses only Qt 6.10 + existing token infrastructure.

---

## 4. Success criteria (the close gate)

M17 closes only when **all** hold:

1. **Build green** on all three presets (Debug, Release, Debug+ASan).
2. **All ctest targets pass** on all three presets — including the new
   M17 unit tests and the visual suite covering all 14 baselines (12 M16
   + 2 M17).
3. **Visual-diff contract** (`compare_with_contract`) passes for
   M17 baselines (`30-all-connected`, `31-with-errors`) — both the
   `< 1 %` percent gate and the `< 200 px` cluster gate.
4. **Token discipline preserved**: `grep -rn "setStyleSheet\|QColor(\|QFont(" src/`
   shows no new violations vs. the M16 baseline (`src/app/app_style.cpp`
   and `src/chart/chart*.cpp` are the only allowed locations — chart is
   QML-rendered and pre-dates the token system; allowed as-is per scope §2).
5. **QSS linter** (`tests/visual/lib/qss_linter.py`) reports zero
   violations on `resources/styles/tokens.qss` (must remain true; M17
   adds no new selectors unless §3.3 records the addition).
6. **Token-freshness gate** in CI passes (the M16 step that re-runs the
   generator and compares — must stay green; M17 does not edit
   `tokens.json` per scope §2).
7. **Frozen-surface counter** stays at **0** (no public interface change).
8. **Net diff** ≤ **800 added lines** (CLAUDE.md §Required #4).
9. **clang-format** dry-run clean; **clang-tidy** introduces no new
   warnings on changed files (CLAUDE.md §DoD #5).
10. **Coverage**: ≥ 70 % line coverage on the public surface of each
    rebuilt widget (CLAUDE.md §Required #1).

---

## 5. Hard-stop semantics

This milestone's hard stop is **visual-diff certification + widget-styling-guide §9 anti-pattern check**:

- Any of the 14 baselines failing the visual-diff contract after the
  guide-conformant implementation → root-cause investigation (per R12) →
  R8 acceptance stamp **or** code fix. No mask is accepted for non-dynamic
  diffs (widget-styling-guide §5 final warning).
- A `setStyleSheet("background: #…")` literal landing in any M17 widget
  → CC reviewer review of the M17 PR rejects on principle (table in
  widget-styling-guide §9 row 1).
- Failure to maintain the M16 12 baselines after M17 chrome changes →
  R8 acceptance stamp per state in M17-done.md §10, or revert the chrome
  change.

---

## 6. Key design constraints

### 6.1 Aggregate-state computation (ConnectionStatusWidget)

The status-bar widget shows one label summarising N connections. The
"class" must reflect a single aggregate state derived from the
per-connection states. Precedence (highest priority wins):

| Rank | Condition | Aggregate class |
|---|---|---|
| 1 | Any connection in `Error` | `status-error` |
| 2 | Any connection in `Connecting` or `Disconnecting` | `status-connecting` |
| 3 | All connections in `Connected` (and N ≥ 1) | `status-connected` |
| 4 | All connections in `Idle` (or no connections) | `status-idle` |

(`status-disconnecting` is not used at the aggregate level — disconnect
transitions are folded into `status-connecting` since both are "transient
ambiguity" colours from the operator's view.)

### 6.2 Per-row state representation (ConnectionListWidget)

Per-row state colour cannot be applied to a `QListWidgetItem` directly
(item text colour is set via `Qt::ForegroundRole`, not QSS). Two options:

- **Option A**: switch from `QListWidget` to `QTreeWidget` with one
  column ("connection") and per-row child label widgets (`setItemWidget`).
- **Option B**: keep `QListWidget`, set `Qt::ForegroundRole` from a
  `palette().color(...)` lookup mirroring the `status-*` tokens.

Option B is **selected** for M17 because it has lower visual-diff impact
(no layout change) and lower test impact (no list-widget API change).
The token mirror is hard-coded in `connection_list_widget.cpp` and a
comment links it to the canonical `tokens.qss` selectors. (If a token
schema change is later needed, the mirror is the one place to update.)

If M18 needs richer per-row chrome (icons, multi-line cells), the switch
to `QTreeWidget` + `setItemWidget` becomes the right call, but that's
M18 scope, not M17.

### 6.3 Deterministic group/leaf order (SignalSelector)

The widget currently uses `std::unordered_map<QString, QTreeWidgetItem*>`
for `groups` and `leaves`, which iterates in an unstable order. Per the
ADR-014 anti-pattern table (widget-styling-guide §9 row 6), this is a
non-deterministic surface even though the visual baseline doesn't yet
capture this widget. M17 fixes the iteration so:

- Groups are added in alphabetically-sorted label order.
- Leaves inside a group are added in alphabetically-sorted signal-id
  order.

This is consistent with ADR-014's SignalBufferRegistry deterministic
order — M17 closes the consumer side of that thread.

### 6.4 Filter-count label semantics (SignalSelector)

Below the existing filter `QLineEdit`, add a `QLabel` showing
"`N / M signals`" (N = visible leaves after filter, M = total leaves).
The label uses `class="caption"` so its style comes from `tokens.qss`.

When the filter is empty: shows just "`M signals`" (no slash).

When M = 0 (no signals registered): shows "`No signals available`".

### 6.5 panelHeader objectName placement

Per widget-styling-guide §4 + §10.4, `objectName="panelHeader"` is used
on a `QFrame` that hosts a panel title `QLabel`. M17 adds this in:

- Top of `ConnectionListWidget` — `QFrame#panelHeader` with `QLabel`
  "Connections".
- Top of `SignalSelector` — `QFrame#panelHeader` with `QLabel` "Signals"
  (replaces the implicit `QTreeWidget::header()` label which is currently
  set to "Signals" — the header view stays for column-resize affordance
  but is hidden visually since the panel header is the primary title).

The header `QFrame#panelHeader` styling (background, border-bottom,
padding) already exists in `tokens.qss` line 56 — M17 just opts into it
by naming the frame.

### 6.6 Aggregate-state pole-vault test surface

M17 unit tests use widget-styling-guide §8.2 pattern: assert
`label->property("class").toString() == "status-error"` after driving
the manager to the relevant state. The visual-diff layer is decoupled
from the class-assertion layer. Test must run with
`QT_QPA_PLATFORM=offscreen` (existing pattern at
`tests/unit/connection/connection_widgets_test.cpp` line 31).

---

## 7. Subtask breakdown

See `.claude/M17-plan.md` for the full subtask sequencing. Summary:

- **S0** — Bootstrap (this spec + understanding.md + plan.md + branch).
- **S1** — `ConnectionStatusWidget` aggregate state + QSS class + unit
  tests (the smallest, most-visible-impact subtask; opens M17).
- **S2** — `ConnectionListWidget` panelHeader + per-row state colour.
- **S3** — `SignalSelector` panelHeader + filter-count + deterministic
  order.
- **S4** — `MainWindow` `objectName` audit + status-bar chrome polish.
- **S5** — Visual baselines: 12 M16 re-capture where needed + 2 new
  M17 captures (`30-all-connected`, `31-with-errors`). R8 stamps.
- **S6** — Closure: done report + concerns final + PR.

Each subtask is one self-contained commit (or commit-cluster) per the
M16 pattern.

---

## 8. Risk register (initial)

| Risk | Mitigation |
|---|---|
| Adding chrome (panelHeader) shifts the layout of all 12 M16 baselines, blowing the visual-diff < 1 % gate | Per-baseline R8 acceptance stamp at S5 close; the M16 process for V0.2→M16 migration (`s7-baseline-migration.md`) is the precedent |
| Aggregate-state precedence ambiguity (e.g., 1 connecting + 1 errored = error or connecting?) | Spec §6.1 declares precedence explicitly. Unit test exercises all 4 ranks. |
| ADR-014's hashtable-iteration anti-pattern surfaces a second time when the SignalSelector groups are re-built | M17 S3 directly replaces the unordered_map with `std::map` (signal id and group label keys). |
| Status-bar widget already uses a mask (per M16 S6.6) — adding new visual states may need new masks | The status-bar mask covers the live-counter region only. The status-text colour change does not fall under the mask; mask remains as-is. |
| Class-property change does not actually re-render (Qt QSS polish required) | Test asserts both the property value AND `polish()` was called via `style()->polish(widget)` after property set (widget-styling-guide §3 example pattern). |
| `connection_list_widget.cpp` Option B (Qt::ForegroundRole) drift from tokens.qss values | A `static_assert`-style runtime check at startup compares the hard-coded mirror against `generated_style_tokens.hpp` token values (zero-cost lookup; runs once in DEBUG builds). |

---

## 9. Cross-references

- `docs/v0.3/widget-styling-guide.md` — implementation reference (M16 S8)
- `docs/v0.3/visual-identity.md` — manifesto (R10 / R11)
- `docs/v0.3/visual-diff-contract.md` — test algorithm (R12 / R14)
- `docs/v0.3/rendering-environment-lock.md` — env contract
- `docs/v0.3/s7-baseline-migration.md` — V0.2 → M16 migration precedent
  (M17 S5 will mirror the R8 acceptance pattern)
- `docs/architecture/decisions/ADR-014-signal-buffer-registry-deterministic-order.md`
  — M17 S3 closes the consumer-side thread
- `.claude/M16-done.md` §12 — the M17 hand-off that authored this spec
- `resources/styles/tokens.json` `semantic_classes` — the dormant class
  selectors M17 wakes up
- `resources/styles/tokens.qss` — already contains the `status-*` /
  `mode-*` / `severity-*` selectors that M17 consumes

---

## 10. Authorization note

Per session prompt 2026-05-20 the human granted CC blanket session
authorization to handle all M17 documents and execution autonomously
("所有文档你自行处理，及时留档记录即可。整个goal实现之前无需再审批"
— "Handle all documents yourself; keep records as you go; no further
approvals before the goal is met"). Phase 2 / Phase 4 of the milestone
closure flow are explicitly waived for this milestone via session
authorization. The "hold" / "stop" runtime escape per CLAUDE.md §4
remains in effect.

This authorization extends to:

- Drafting this spec without prior human review.
- Drafting `.claude/M17-understanding.md` + `.claude/M17-plan.md` and
  proceeding straight to execution.
- Pushing `milestone/M17` to origin.
- Opening the M17 PR.
- Merging the M17 PR once CI is green.
- Tagging the closure.

Recorded so the audit trail in M17-done.md §11 captures the deviation
from the standard 5-phase protocol.
