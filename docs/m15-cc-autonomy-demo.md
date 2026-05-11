# M15 S6 — CC vision-driven self-test demonstration

**Date**: 2026-05-11
**Branch**: `milestone/M15`
**Subtask spec**: `docs/milestones/M15-vision-infrastructure.md` §S6;
`docs/V0-series-charter.md` §1.

## Premise

V0.2 charter §1 promises an end-to-end perception loop:
**operator change → AI captures GUI → AI describes GUI → AI
diagnoses → AI proposes fix**, without operator GUI dogfood.
This document is the recorded demonstration that the loop
actually closes for a real (deliberately-introduced) visual
regression.

The demonstration also surfaces a structural lesson the
operator's M15 R7 review already anticipated:

> "Pixel-diff measures two captures matching each other,
> not the capture matching the documented semantic state.
> **stable ≠ correct.**"
> — operator, M15 S3 reclassification briefing

For the regression here — a 4-letter text swap in one menu
item — pixel-diff at the V0.2 5 % default threshold **does
not catch it**. The vision-LLM (CC's Read tool) does. That's
the headline value-add of the V0.2 perception loop.

## Setup

- **Target file**: `src/app/main_window.cpp`, line 1086.
- **Target baseline**: `tests/visual/baselines/30-menu-file-open.png`
  (operator-approved at `1f4524b`).
- **Test path**: `tests/visual/tests/test_states_production_fidelity.py`
  → `test_baseline_30_menu_file_open` → captures
  `tests/screenshots/30-menu-file-open.png` and pixel-diffs at
  5 % tolerance.

## Step 1 — Reference state (pre-regression)

CC Read tool against the operator-approved baseline at
`tests/visual/baselines/30-menu-file-open.png`:

> File menu, opened:
> - "Open Session…" with shortcut "Ctrl+O"
> - "Quit" with shortcut "Ctrl+Q"
>
> Underlying MainWindow chrome: menubar (Connections / Session
> / File), toolbar (● Live / 10 sec preset / + Chart),
> Connections panel (empty), Signals panel (empty header),
> status bar ("FPS: ~30 / chart  Dropped: 0  buffer 0%% (0
> MiB)  0/0 connected  Idle").

This is the canonical state V0.2 measures against.

## Step 2 — Introduce a regression

Single-line change at `src/app/main_window.cpp:1086`:

```diff
- auto* quitAction = fileMenu->addAction(tr("&Quit"));
+ auto* quitAction = fileMenu->addAction(tr("&Exit"));
```

This is intentionally subtle: a 4-letter text replacement in
one menu item. The keyboard shortcut binding stays
`QKeySequence::Quit` so the shortcut hint will still render
as "Ctrl+Q" — internal inconsistency.

Rebuild:

```
$ cmake --build --preset debug
[5/5] Linking CXX executable src/app/signalforge
$ strings build/debug/src/app/signalforge | grep -E "^&(Quit|Exit)$"
&Exit                              # ← regression baked into the binary
```

## Step 3 — Run the visual test

```
$ rm -f tests/screenshots/30-menu-file-open.png
$ SIGNALFORGE_BINARY=$(pwd)/build/debug/src/app/signalforge \
    xvfb-run --auto-servernum \
    python3 tests/visual/tests/test_states_production_fidelity.py
PASS  test_baseline_02_conn_udp_idle
PASS  test_baseline_12_multi_2_drivers
PASS  test_baseline_13_multi_5_drivers
PASS  test_baseline_24_dialog_add_serial
PASS  test_baseline_25_dialog_add_udp
PASS  test_baseline_26_dialog_edit
PASS  test_baseline_30_menu_file_open          ← !! PASSED despite the regression
PASS  test_baseline_31_menu_connections_open
PASS  test_baseline_32_menu_session_open
PASS  test_baseline_33_status_buffer_normal

summary: 10 pass, 0 fail
```

Direct pixel-diff at multiple thresholds:

```
$ python3 -c "from lib.compare import compare_baseline; ..."
threshold=5.0%   → matched=True  diff=0.0190%
threshold=1.0%   → matched=True  diff=0.0190%
threshold=0.5%   → matched=True  diff=0.0190%
threshold=0.1%   → matched=True  diff=0.0190%
threshold=0.05%  → matched=True  diff=0.0190%   (clamp floor)
```

The text swap produces a **0.019 % pixel difference** — three
orders of magnitude below the V0.2 5 % CI gate. Pixel-diff alone
**does not catch** this regression at any sane threshold.

This is exactly the V0.1-era escape pattern the operator
flagged: a verification gate that runs green on a real fault.

## Step 4 — CC vision-LLM inspection (Read tool)

CC reads the captured PNG at
`tests/screenshots/30-menu-file-open.png`:

> File menu, opened:
> - "Open Session…" with shortcut "Ctrl+O"
> - **"Exit"** with shortcut "Ctrl+Q"   ← differs from reference
>
> Inconsistency observed: the menu item label reads "Exit"
> but the shortcut hint still reads "Ctrl+Q". A standalone
> "Exit" item paired with "Ctrl+Q" is internally inconsistent
> (`Ctrl+Q` is the canonical Qt shortcut for "Quit", not
> "Exit").

Diagnosis (CC, ≤ 30 s after running the Read tool):

> The reference baseline has **"Quit"** as the second item;
> the regression capture has **"Exit"**. The shortcut binding
> is unchanged. This is a text-string swap, almost certainly
> a one-line change in the menu construction. Likely site:
> wherever the `&Quit` action is added to the File menu.

Grep confirms:

```
$ grep -rn 'tr("&Quit\|tr("&Exit' src/app/
src/app/main_window.cpp:1086: auto* quitAction = fileMenu->addAction(tr("&Exit"));
```

## Step 5 — CC proposes the fix

```diff
--- a/src/app/main_window.cpp
+++ b/src/app/main_window.cpp
@@ -1086,1 +1086,1 @@
-    auto* quitAction = fileMenu->addAction(tr("&Exit"));
+    auto* quitAction = fileMenu->addAction(tr("&Quit"));
```

Justification:

- The operator-approved canonical baseline shows "Quit".
- The `QKeySequence::Quit` binding gives the shortcut "Ctrl+Q",
  which Qt's HIG aligns with the label "Quit", not "Exit".
- Internal consistency restored.

## Step 6 — Apply the fix and verify

```
$ # Apply diff above
$ cmake --build --preset debug
[5/5] Linking CXX executable src/app/signalforge

$ rm -f tests/screenshots/30-menu-file-open.png
$ SIGNALFORGE_BINARY=$(pwd)/build/debug/src/app/signalforge \
    xvfb-run --auto-servernum \
    python3 tests/visual/tests/test_states_production_fidelity.py
PASS  test_baseline_30_menu_file_open    ← still PASS (was a false-PASS before)
summary: 10 pass, 0 fail

$ python3 -c "from lib.compare import compare_baseline; ..."
post-fix diff: 0.0000%   matched=True
```

CC re-reads the post-fix capture and confirms:

> File menu, opened:
> - "Open Session…" with shortcut "Ctrl+O"
> - **"Quit"** with shortcut "Ctrl+Q"   ← matches reference
>
> Reference + capture agree at the pixel level (0.0000 % diff)
> AND at the semantic level (label + shortcut + position
> consistent). Loop closed.

## Loop diagram

```
       ┌────────────────────────────────────────────────────┐
       │                                                    │
       │   1. Operator change                               │
       │      (e.g. branch with a refactor of menu strings) │
       │                                                    │
       └─────────────────────────────┬──────────────────────┘
                                     │
                                     ▼
       ┌────────────────────────────────────────────────────┐
       │   2. CC builds + runs visual ctest                 │
       │      cmake --build --preset debug                  │
       │      ctest --preset debug -L visual                │
       └─────────────────────────────┬──────────────────────┘
                                     │
                                     ▼
       ┌────────────────────────────────────────────────────┐
       │   3. ctest result vs ground truth                  │
       │      PASS (pixel-diff < 5 %) ─┐                    │
       │      FAIL (pixel-diff ≥ 5 %)  │                    │
       │                               │                    │
       │      Either way → Step 4 ─────┘                    │
       │      (CC never trusts pixel-diff alone — that      │
       │       is the V0.2 graduation lesson.)              │
       └─────────────────────────────┬──────────────────────┘
                                     │
                                     ▼
       ┌────────────────────────────────────────────────────┐
       │   4. CC Read tool inspects                         │
       │      tests/screenshots/<state>.png                 │
       │      (multimodal vision; ADR-008-grade ground      │
       │       truth on what the user actually sees)        │
       └─────────────────────────────┬──────────────────────┘
                                     │
                                     ▼
       ┌────────────────────────────────────────────────────┐
       │   5. CC diff vs operator-approved baseline         │
       │      Read tool on tests/visual/baselines/<state>   │
       │      then narrate what changed semantically.       │
       └─────────────────────────────┬──────────────────────┘
                                     │
                                     ▼
       ┌────────────────────────────────────────────────────┐
       │   6. CC diagnoses → proposes fix                   │
       │      grep / git blame to find the source file      │
       │      diff against the inferred root cause          │
       └─────────────────────────────┬──────────────────────┘
                                     │
                                     ▼
       ┌────────────────────────────────────────────────────┐
       │   7. CC applies fix + re-runs steps 2–5            │
       │      verify both pixel-diff and vision agree on    │
       │      the post-fix state                            │
       └────────────────────────────────────────────────────┘
```

## Headline lessons (V0.2 graduation)

1. **Pixel-diff is necessary but not sufficient.** The CI gate
   catches 10 % theme rotations, font-AA jitter regressions,
   and big-pane layout breakage. It does NOT catch single-line
   menu text swaps, recoloured buttons within their hue band,
   or shortcut-hint reformat. Vision-LLM closes that gap.

2. **CC's Read tool is the primary vision-LLM**, per
   `M15-concerns.md` C2 + C7. It runs in CC's session,
   handles SignalForge PNGs reliably (validated empirically
   at S0; re-validated in this demo on the captured menu
   PNG), and never makes a network call from CI (per
   Phase 5 public-repo security amendment).

3. **The R7 production-fidelity criterion is operationalised**
   in this demo. The captured `30-menu-file-open.png` comes
   through `autoOpenMenu("File")` → Qt `menu->popup(pos)` →
   QScreen full-screen grab. Mechanism C-fullscreen, no
   fixture-mock mutation of the menu state. The capture
   reflects what a user opening the File menu would see.

4. **Demo is single-commit reversible.** The diff applied in
   Step 2 was applied and reverted in this session; the
   working tree at this commit holds the post-fix state
   (label = "Quit", binary verified via `strings`). The
   demonstration is reproducible by any future CC session by
   re-running Step 2's diff + the Step 3 command.

## Reproduction recipe

```bash
# Position
git checkout milestone/M15
cmake --preset debug

# Apply regression
sed -i 's/tr("&Quit")/tr("\&Exit")/' src/app/main_window.cpp
cmake --build --preset debug

# Run visual test
rm -f tests/screenshots/30-menu-file-open.png
SIGNALFORGE_BINARY=$(pwd)/build/debug/src/app/signalforge \
    xvfb-run --auto-servernum \
    python3 tests/visual/tests/test_states_production_fidelity.py
# Expected: PASS (false-PASS — pixel-diff insufficient)

# Read tool
# (in CC's session)
#   Read tests/screenshots/30-menu-file-open.png
#   → expect: "Exit  Ctrl+Q" instead of "Quit  Ctrl+Q"

# Apply fix
sed -i 's/tr("&Exit")/tr("\&Quit")/' src/app/main_window.cpp
cmake --build --preset debug

# Verify post-fix
rm -f tests/screenshots/30-menu-file-open.png
SIGNALFORGE_BINARY=$(pwd)/build/debug/src/app/signalforge \
    xvfb-run --auto-servernum \
    python3 tests/visual/tests/test_states_production_fidelity.py
python3 -c "import sys; sys.path.insert(0, 'tests/visual'); \
            from lib.compare import compare_baseline; \
            from pathlib import Path; \
            r = compare_baseline(Path('tests/screenshots/30-menu-file-open.png'), \
                                 Path('tests/visual/baselines/30-menu-file-open.png'), \
                                 max_diff_percent=5.0); \
            print(f'diff={r.diff_percent:.4f}% matched={r.matched}')"
# Expected: diff=0.0000% matched=True
```

## Cross-references

- M15 spec: `docs/milestones/M15-vision-infrastructure.md`
- Concerns: `.claude/M15-concerns.md` (C2 vision-LLM choice,
  C7 public-repo security, R7 fidelity reclassification)
- Progress: `.claude/M15-progress.md` §S3 (12 PF baselines),
  §S4 (test framework integration)
- V0 charter: `docs/V0-series-charter.md` §1 (AI vision loop)
- Operator R7 briefing: in-session prompt that introduced
  the production-fidelity criterion + classification (d)
  fixture-mock.
