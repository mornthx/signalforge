# M17 Plan — V0.3 Widget Rebuild

This is the operational plan for executing M17 per the spec at
`docs/milestones/M17-widget-rebuild.md` and the understanding at
`.claude/M17-understanding.md`. Subtasks are sequential — each commits
on `milestone/M17` and leaves the tree green for the next.

## Subtask sequencing

### S0 — Bootstrap (this commit)

**Deliverables**

- `docs/milestones/M17-widget-rebuild.md` — M17 spec.
- `.claude/M17-understanding.md` — understanding doc.
- `.claude/M17-plan.md` — this file.
- `.claude/M17-concerns.md` — empty concerns log (seeds C1–C3 below).
- `.claude/M17-progress.md` — empty progress log (table seed).
- `milestone/M17` branch created locally and pushed to origin.

**Concerns seeded**

- **C1**: Aggregate-state precedence rule (spec §6.1) is CC-authored;
  no human review yet. Flag if M17 PR review surfaces a different
  operator preference.
- **C2**: ConnectionListWidget per-row colour via `Qt::ForegroundRole`
  (Option B in spec §6.2) is a token mirror, not a token consumer.
  Drift risk if `tokens.json` `status-*` colours change without
  updating the mirror. Mitigated by DEBUG-only runtime check.
- **C3**: New `QFrame#panelHeader` chrome adds ~28 px to dock heights.
  M16 baseline layouts shift accordingly; R8 re-acceptance required
  at S5. Volume depends on how many of the 12 states capture the
  affected docks (worst case: 12 of 12).

**Hard-stop check**

S0 is documentation only. Building / testing is not required at S0
(CLAUDE.md §Required #2 exception for docs-only commits).

**Estimated diff**: ~1 200 lines added (specs + plans). Excluded from
the 800-line net cap per the same exception (no code changes).

---

### S1 — ConnectionStatusWidget aggregate state + QSS class

**Goal**: Status-bar widget renders in green/yellow/red/grey based on
aggregate connection state.

**Touches**

- `src/connection/connection_status_widget.hpp` — add `AggregateState`
  enum, `aggregateState()` getter (test surface).
- `src/connection/connection_status_widget.cpp` — implement aggregate
  computation in `refresh()`; call `setProperty("class", …)` +
  `unpolish/polish/update`.
- `tests/unit/connection/connection_widgets_test.cpp` — add 5 new
  test cases (one per aggregate state) asserting class property.

**Aggregate precedence** (spec §6.1):

```
1. any Error          → status-error
2. any Connecting OR
   any Disconnecting  → status-connecting
3. N≥1 && all Connected → status-connected
4. otherwise           → status-idle
```

**Test pattern** (widget-styling-guide §8.2):

```cpp
TEST_CASE("S1: StatusWidget class=status-error when any connection errors") {
    /* ... drive manager to error state ... */
    REQUIRE(status.label()->property("class").toString() == "status-error");
}
```

**Hard-stop check**: Build all 3 presets; `ctest` covers new + existing
tests. No new warnings. clang-format clean.

**Estimated diff**: ~80 added lines source + ~120 added lines tests.

---

### S2 — ConnectionListWidget panelHeader + state-coloured rows

**Goal**: Each row's text colour reflects the connection's state; a
panel header titles the list.

**Touches**

- `src/connection/connection_list_widget.hpp` — no signature change.
  Add private helper `colorForState(Connection::State)` returning a
  `QColor` mirrored from token values.
- `src/connection/connection_list_widget.cpp` —
  - Add a `QFrame* header_` with `objectName="panelHeader"` and a
    `QLabel` "Connections" inside.
  - Add `header_` to the layout as the first row.
  - In `rebuild()` / `updateRow()` / `onConnectionAdded()`: after
    setting the row text, set `item->setForeground(QBrush(colorForState(state)))`.
  - DEBUG-only one-shot runtime assertion that the colour mirror
    matches `generated_style_tokens.hpp` `kStatusConnectedHex` etc.
- `tests/unit/connection/connection_widgets_test.cpp` — add test
  cases asserting:
  - `header_->objectName() == "panelHeader"`.
  - Row foreground colour matches expected `kStatus*` value per
    state.

**Hard-stop check**: Build all 3 presets; ctest; no warnings.

**Estimated diff**: ~70 added lines source + ~80 added lines tests.

---

### S3 — SignalSelector panelHeader + filter-count + deterministic order

**Goal**: Header titled "Signals", caption-styled "N / M signals" count,
deterministic group + leaf order.

**Touches**

- `src/chart/signal_selector.hpp` — no signature change. Optionally add
  `[[nodiscard]] int visibleLeafCount() const noexcept` test surface
  (additive).
- `src/chart/signal_selector.cpp` —
  - Replace `Impl::groups` / `Impl::leaves` from `std::unordered_map`
    to `std::map<QString, QTreeWidgetItem*>`.
  - Add `QFrame* header_` with `objectName="panelHeader"` and `QLabel`
    "Signals" at the top of the layout.
  - Hide `QTreeWidget::header()` (no longer needed for title role).
  - Add `QLabel* countLabel_` with `setProperty("class", "caption")`
    below the filter line edit.
  - In `setFilter()` and `refresh()`, recompute visible / total counts
    and update `countLabel_->setText(...)`.
- `tests/unit/chart/signal_selector_tree_test.cpp` — add:
  - Group order assertion (build a manager with shuffled-input signal
    ids → expect alphabetical-output groups).
  - Filter-count assertion (set filter → assert label text).

**Hard-stop check**: Build all 3 presets; ctest; no warnings.

**Estimated diff**: ~100 added lines source + ~80 added lines tests.

---

### S4 — MainWindow objectName audit + status-bar polish

**Goal**: Stable `objectName`s on widget chrome surfaces for testing /
AT-SPI / visual-tooling.

**Touches**

- `src/app/main_window.cpp` —
  - Add `objectName="connectionsDock"` (likely already present per
    Explore agent report line 270; verify).
  - Add `objectName="signalSelectorDock"` to the SignalSelector dock
    parent if it lives in a dock.
  - Add `objectName="statusBar"` to the status bar's container if not
    already inherited from Qt.
  - Add `objectName="connectionStatusLabel"` to the
    `ConnectionStatusWidget`'s inner label (already accessed via
    `label()`, but `objectName` makes it visual-test-friendly).
- `tests/unit/connection/connection_widgets_test.cpp` — assert
  `label->objectName() == "connectionStatusLabel"` (new test case).

**Hard-stop check**: Build all 3 presets; ctest; no warnings.

**Estimated diff**: ~30 added lines source + ~30 added lines tests.

---

### S5 — Visual baselines refresh + new state captures

**Goal**: 14 visual baselines green; 2 new (`30-all-connected`,
`31-with-errors`) added; R8 acceptance for any M16 baseline whose
chrome shifted.

**Touches**

- `tests/visual/scripts/capture_baselines.py` — add 2 new state specs:
  - `30-all-connected`: launch with a replay fixture loaded, drive to
    Connected, capture status bar with `--capture-screenshot-path`.
  - `31-with-errors`: launch with a bad-path replay config, drive to
    Error, capture.
- `tests/visual/baselines/30-all-connected.png` + `.env.json` (+ mask
  if needed for transient regions).
- `tests/visual/baselines/31-with-errors.png` + `.env.json`.
- For each of the 12 M16 baselines, run the visual-diff job locally
  (or on CI) and either:
  - Confirm < 1 % / < 200 px (no action needed), or
  - R8 re-accept via `scripts/accept-baseline.sh <state>` with a
    rationale entry in `.claude/M17-done.md` §10.

**Hard-stop check**: All 14 baselines pass the visual-diff contract on
all 3 presets in CI.

**Estimated diff**: 0 source LOC, ~50 lines in `capture_baselines.py`,
12+2 PNG / env / mask files (binary + small JSON — not counted toward
the 800-line code cap per CLAUDE.md §Required #4 "excluding generated
files and test fixtures").

---

### S6 — Closure: done report + PR + CI green

**Goal**: M17 closed per CLAUDE.md §DoD + milestone-closure flow.

**Touches**

- `.claude/M17-progress.md` — final state.
- `.claude/M17-concerns.md` — final state (resolved + carry-forward).
- `.claude/M17-done.md` — closure report mirroring `M16-done.md`
  structure (§1 outcome, §2 subtask deliverables, §3 freezes (none),
  §4 metrics, §5 tests, §6 CI, §7 deviations, §8 scope vs. delivery,
  §9 cross-references, §10 R8 acceptance stamps, §11 PR + merge
  details, §12 hand-off to M18).
- `docs/v0.3/widget-styling-guide.md` — append §12 "M17 lessons" if
  any new pattern emerged.
- `git push -u origin milestone/M17`.
- `gh pr create` to main.
- Wait for CI green on all 3 jobs.
- `gh pr merge --merge --delete-branch=false` (per session
  authorization).
- `git tag -a v0.3.1-m17 -m "M17 widget rebuild close"`.
- `git push origin v0.3.1-m17`.

**Hard-stop check**: PR merged + tag pushed + CI green on main.

**Estimated diff**: ~800 added lines docs (excluded from 800-line code
cap per docs exception).

---

## Tag scheme note

M16 used tag `v0.3.0-m16`. M17 follows with `v0.3.1-m17` (V0.3 minor
bump). This deviates from CLAUDE.md §Phase-3 step 9.b `v0.0.<n>.1`
scheme (which was for the original M0-M13 V1 path); the V0.3 track has
its own `v0.3.x` series since M14 (per the M14+done.md tagging).

## Estimated total diff

| Section | Source LOC | Test LOC | Doc LOC | Baseline files |
|---|---:|---:|---:|---:|
| S0 | 0 | 0 | ~1 200 | 0 |
| S1 | ~80 | ~120 | 0 | 0 |
| S2 | ~70 | ~80 | 0 | 0 |
| S3 | ~100 | ~80 | 0 | 0 |
| S4 | ~30 | ~30 | 0 | 0 |
| S5 | ~50 (capture) | 0 | 0 | 14 PNG + 14 JSON |
| S6 | 0 | 0 | ~800 | 0 |
| **Total** | **~330** | **~310** | **~2 000** | **~28 files** |

Code-only net diff: ~640 lines — under the 800-line cap (CLAUDE.md
§Required #4). Test / doc / baseline files outside the cap per the
exclusion clause.

## Build / CI strategy

Every subtask except S0 must pass:

- `cmake --build build/debug --preset debug && ctest --test-dir build/debug`
- `cmake --build build/release --preset release && ctest --test-dir build/release`
- `cmake --build build/debug-asan --preset debug-asan && ctest --test-dir build/debug-asan`
  (or CI authoritative gate per `host_asan_preload.md` memory)
- `clang-format --dry-run -Werror <changed files>`

If the local ASan preset is blocked by `/etc/ld.so.preload` (per
saved memory `host_asan_preload.md`), CI is the authoritative gate
— note the local block in `.claude/M17-concerns.md` C0.

## Branch / push cadence

S0 is committed and pushed at end of bootstrap (this commit-cluster).
Subsequent subtasks commit on every green checkpoint; push at the
end of each subtask cluster to keep CI cycles fresh.

## Roll-back plan

If a subtask blows the visual-diff gate beyond R8-acceptable bounds,
roll back the chrome change of that subtask only; the QSS class /
class property change (the colour code path) is independent and
proceeds. Worst-case rollback target: keep only S1 (status-bar colour
change) which has the lowest layout impact.
