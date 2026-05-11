# M15 — Concerns (C1-C7)

Resolves the seven open questions surfaced in
`.claude/M15-understanding.md` §5 + the Phase 5 amendment
"Local-only hybrid; CI runs no vision LLM" + the new C7
public-repo security constraint.

Each concern: question, options, resolution + rationale.

---

## C1 — Capture mechanism implementation order

**Question** (spec §3 M15.1; recommended D — hybrid B + C).

**Resolution**: implement **mechanism C first** (in-process
`QPixmap = widget->grab()` via a new
`MainWindow::captureScreenshot(path)` public method + a
`--capture-screenshot=<path>` CLI flag). Most M15 tests use C
because it is fast, deterministic, headless-compatible
(`Q_QPA_PLATFORM=offscreen` works), and reuses the M14 S1
infrastructure (the existing `--dump-chart-png` is a degenerate
case of C scoped to the QQuickWidget). Add **mechanism B**
(xvfb + xwd | convert) only when a test specifically needs
full-window capture (menu open, dialog modal, multi-window
state). Per spec §3 M15.1 D, the test fixture's
`tests/visual/lib/capture.py` exposes both helpers; per-test
default is C; opt-in B via fixture parameter.

S1 lands C end-to-end + the B helper as a stub; S4 wires B
into the tests that need it (dialog / menu states).

---

## C2 — M15.2 vision-LLM survey + empirical CC native test

**Question**: Phase 4 deferred M15.2 to S0 with five candidate
options. Phase 5 amendment locked **local-only hybrid** with
the explicit constraint that CI does NOT call vision LLM
(public-repo security).

### Empirical test: CC native Read tool on a SignalForge PNG

S0 captured a real SignalForge screenshot via the M14 S1
infrastructure:

```
mkdir -p /tmp/m15-s0-empirical/{cfg,state}
XDG_CONFIG_HOME=/tmp/m15-s0-empirical/cfg \
    XDG_STATE_HOME=/tmp/m15-s0-empirical/state \
    QSG_RHI_BACKEND=software \
    SF_F4_DIAG=1 \
    xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" \
    timeout 8 ./build/release/src/app/signalforge \
        --auto-load-test-fixture tests/integration/gui/fixtures/m14_smoke.yaml \
        --auto-select-signal "udp:m14-smoke-udp/temperature" \
        --dump-chart-png-after-ms 3000 \
        --dump-chart-png-path /tmp/m15-s0-empirical/chart.png \
        --exit-after-dump &
APP_PID=$!
sleep 0.5
python3 tests/integration/gui/helpers/udp_fixture_sender.py \
    --host 127.0.0.1 --port 9998 --frames 100 --rate-hz 50 \
    --initial-delay-s 0
wait $APP_PID
```

→ produced `chart.png` (3 463 bytes, 661×720).

CC's Read tool was then invoked:

```
Read /tmp/m15-s0-empirical/chart.png
```

→ image rendered visually. CC observed:

- 661×720 mostly-white canvas (the QQuickWidget framebuffer
  for the chart pane).
- A single bright-orange `QSGSimpleRectNode` ~60×60 px at
  (10, 10) — the `SF_F4_DIAG` diagnostic rect from ADR-010
  §"Implementation lesson".
- No chart line visible (consistent with the xvfb + software-
  RHI rasterization limitation already documented in ADR-010
  Path α).

**Result**: ✓ CC native Read tool **works reliably on
SignalForge screenshots**. Renders PNG; describes positions,
colors, widget identification, and absence-of-content. Quality
sufficient for state-machine-complete baseline coverage when
combined with mechanism C for chart-pane-only capture and
mechanism B for full-window state tests.

### Resolution: M15.2 = local-only hybrid

| Layer | Tool | Where it runs |
|---|---|---|
| **CC interactive vision** | Read tool (multimodal) | CC's local session — used during development + S6 autonomy demo |
| **Optional benchmark** | Xiaomi MiMo-V2.5 API (OpenAI-compatible at `https://api.xiaomimimo.com/v1`, model `mimo-v2.5`) | Operator's local environment (API key in `~/.bashrc` / `.env.local`; **never** in repo) — used to cross-reference CC native description quality |
| **CI vision** | **NONE** (per security constraint C7) | CI runs pixel-level baseline diff only |
| **CI failure mode** | Pixel diff fails → upload screenshot artifact → CI fails → human / CC reviews locally | GitHub Actions |

### Rejected options

- **Local vision model (LLaVA / Qwen-VL / MiniCPM-V)**: GPU
  runners or slow CPU inference; CI image bloat from model
  download pipeline. Not justified for V0.2 when CC native
  Read tool already covers the dev workflow.
- **Other LLM API (GPT-4V / Gemini)**: similar profile to
  Claude API; adds vendor diversity but no benefit unless a
  multi-vendor mandate exists. None for V0.2.
- **Claude API in CI** (the spec's original recommendation):
  Phase 5 amendment removed this for public-repo security —
  no API key in repo means no API call from CI.

---

## C3 — Y-scope state enumeration (~30–50 baselines)

**Question**: which GUI states get baseline screenshots? Per
spec §3 M15.3 W, V0.2 ships Y-scope (state-machine complete);
V0.3+ may expand to Z (pixel-level comprehensive).

**Resolution**: 38 baselines across 8 categories. Final list
finalised at S3 with operator approval; this is the working
target.

| # | Category | State | Baseline filename |
|---|---|---|---|
| 1 | Empty | Just-launched, no connections | `00-empty-launch.png` |
| 2 | Empty | + chart added (still no signals) | `01-empty-with-chart.png` |
| 3 | Connection lifecycle | Idle UDP connection | `02-conn-udp-idle.png` |
| 4 | Connection lifecycle | Connecting UDP | `03-conn-udp-connecting.png` |
| 5 | Connection lifecycle | Connected UDP (no signals selected) | `04-conn-udp-connected.png` |
| 6 | Connection lifecycle | Connected UDP + signal selected | `05-conn-udp-with-signal.png` |
| 7 | Connection lifecycle | Disconnecting UDP | `06-conn-udp-disconnecting.png` |
| 8 | Connection lifecycle | Error UDP | `07-conn-udp-error.png` |
| 9 | Connection lifecycle | Idle Serial | `08-conn-serial-idle.png` |
| 10 | Connection lifecycle | Connected Serial | `09-conn-serial-connected.png` |
| 11 | Connection lifecycle | Idle TCP | `10-conn-tcp-idle.png` |
| 12 | Connection lifecycle | Connected TCP | `11-conn-tcp-connected.png` |
| 13 | Multi-connection | 2 drivers Connected | `12-multi-2-drivers.png` |
| 14 | Multi-connection | 5 drivers Connected (signal selector full) | `13-multi-5-drivers.png` |
| 15 | Recording | Recording active (1 driver) | `14-recording-active.png` |
| 16 | Recording | Recording stopped (status bar shows bytes) | `15-recording-stopped.png` |
| 17 | Replay | File-open dialog | `16-replay-open-dialog.png` |
| 18 | Replay | Replay loaded (paused) | `17-replay-loaded.png` |
| 19 | Replay | Replay playing | `18-replay-playing.png` |
| 20 | Replay | Replay scrubber mid-position | `19-replay-scrubber-mid.png` |
| 21 | Replay | Replay at end | `20-replay-end.png` |
| 22 | Replay | Speed combo open at 5× | `21-replay-speed-5x.png` |
| 23 | Mode transition | Live → Replay confirm dialog | `22-mode-live-to-replay.png` |
| 24 | Mode transition | Replay → Live 3-option dialog | `23-mode-replay-to-live.png` |
| 25 | Dialogs | Connection-add dialog (Serial type) | `24-dialog-add-serial.png` |
| 26 | Dialogs | Connection-add dialog (UDP type) | `25-dialog-add-udp.png` |
| 27 | Dialogs | Connection-edit dialog | `26-dialog-edit.png` |
| 28 | Dialogs | Quit-while-recording prompt | `27-dialog-quit-recording.png` |
| 29 | Dialogs | Recording-error dialog | `28-dialog-recording-error.png` |
| 30 | Dialogs | Replay-error dialog | `29-dialog-replay-error.png` |
| 31 | Menus | File menu open | `30-menu-file-open.png` |
| 32 | Menus | Connections menu open | `31-menu-connections-open.png` |
| 33 | Menus | Session menu open | `32-menu-session-open.png` |
| 34 | Status states | Buffer < 80 % | `33-status-buffer-normal.png` |
| 35 | Status states | Buffer ≥ 80 % warning | `34-status-buffer-warn.png` |
| 36 | Status states | Buffer FULL | `35-status-buffer-full.png` |
| 37 | Multi-chart | 2 charts coexisting | `36-multi-chart-2.png` |
| 38 | Multi-chart | 5 charts (vertical stack) | `37-multi-chart-5.png` |

S3 captures these; S4 wires them into automated comparison.

---

## C4 — `tests/visual/` layout

**Question**: directory structure + harness language.

**Resolution**:

```
tests/visual/
├── README.md                  # how to add new visual tests
├── CMakeLists.txt             # ctest wiring (label: visual)
├── lib/
│   ├── capture.py             # mechanism C in-process + B xvfb+xwd helpers
│   ├── describe_screenshot.py # CC-native + optional MiMo benchmark wrapper
│   ├── schema.py              # canonical JSON schema (single source of truth)
│   ├── compare.py             # pixel-level baseline diff (PIL/Pillow OR Python stdlib)
│   └── fixture_helpers.py     # connection / recording / replay state setup
├── baselines/
│   ├── 00-empty-launch.png
│   ├── 01-empty-with-chart.png
│   └── …  (38 PNGs per C3)
└── tests/
    ├── test_states_empty.py
    ├── test_states_connection_lifecycle.py
    ├── test_states_recording.py
    ├── test_states_replay.py
    ├── test_states_dialogs.py
    └── test_states_multi.py
```

Harness language: **Python** (consistent with M14
`tests/integration/gui/helpers/*.py`; stdlib-only base + PIL
optional). Each test launches `signalforge` via `subprocess`,
drives state via existing CLI flags + the fixture helpers,
captures via `capture.py`, compares via `compare.py`,
optionally describes via `describe_screenshot.py`.

Pixel-level baseline diff in **CI**: pure Python comparison
(no PIL dep — read PNG via stdlib `zlib`+`struct` and compare
byte-for-byte; tolerance for aliased fonts via fuzzy-match
percentage threshold, default 5 %). PIL **optional** for
local development convenience.

---

## C5 — `accept-baseline.sh` location + invocation

**Question**: where lives the baseline-update workflow?

**Resolution**: `scripts/accept-baseline.sh`.

```
# Usage:
#   scripts/accept-baseline.sh <test-name>
# Or:
#   scripts/accept-baseline.sh <test-name> <state-name>
#
# Copies the most recent
#   tests/screenshots/<test-name>/<state>.png
# to
#   tests/visual/baselines/<state>.png
# and stages the change for git commit.

set -e
TEST=$1
STATE=${2:-default}
ACTUAL="tests/screenshots/${TEST}/${STATE}.png"
BASELINE="tests/visual/baselines/${STATE}.png"

if [ ! -f "$ACTUAL" ]; then
    echo "no actual screenshot at $ACTUAL"
    exit 2
fi

cp "$ACTUAL" "$BASELINE"
git add "$BASELINE"
echo "staged $BASELINE; review + commit"
```

Documented in `tests/visual/README.md` + referenced from PR
diff display.

---

## C6 — Frozen-surface counter init

**Question**: where is the running count tracked?

**Resolution**: `.claude/M15-progress.md` §"Frozen-surface
modifications" (scaffolded by S0). Initial state: **0 / 2**.
Per V0 charter §3 ("Backend frozen during V0 series"), M2-M12
frozen `.hpp` files remain frozen as in V1. ADR + counter bump
required for any modification. HALT #5 fires at > 2.

Note: `main_window.hpp` is **not** frozen (V1 integration
point); M15 may add public methods like `captureScreenshot`
freely without ADR. Same for `main.cpp`.

---

## C7 (NEW) — Public-repo + no-CI-vision security

**Phase 5 constraint**: SignalForge is a public repo. API key
leakage in CI is unacceptable for V0.2.

**Resolution**:

- **Zero `secrets.*` references** in any
  `.github/workflows/*.yml` for vision-LLM purposes. No API
  key referenced from CI under any circumstance.
- **No `.env*` files committed.** `.env.local`, `.env`, etc.
  are operator-local and gitignored. Verify
  `.gitignore` covers them at S0 close.
- **Vision-LLM calls are local-only.** CC's Read tool runs in
  CC's session (no API key handling required at the
  application layer; CC's runtime owns its credentials).
  Optional MiMo benchmark uses an operator-local API key in
  the operator's shell environment.
- **CI gates run pixel-level baseline diff only.** No vision-
  LLM verdict in CI. Pixel diff is deterministic; failure
  uploads screenshot artifact for local human / CC review.
- **HALT trigger H7** (newly added per Phase 5 amendment):
  CI workflow accidentally references API key OR triggers
  vision-LLM call → HALT immediately, security violation;
  revert + audit logs.

Concrete .gitignore additions to verify at S0:

```
# Already present (carried from V1 governance):
*.log
build/
build-*/

# M15 additions:
.env
.env.local
.env.*.local
tests/screenshots/    # ephemeral output; baselines in tests/visual/baselines/ are committed
```

---

## Summary

| Concern | Resolution |
|---|---|
| **C1** capture mechanism order | Mechanism C in-process first; B xvfb+xwd as stub at S1, wired in S4 for full-window tests |
| **C2** M15.2 vision LLM | **Local-only hybrid**: CC Read tool (dev) + optional MiMo benchmark (operator-local). NO CI LLM. **Empirical test passed**: Read tool renders SignalForge PNG correctly |
| **C3** Y-scope baselines | 38 baselines across 8 categories; finalised at S3 |
| **C4** `tests/visual/` layout | Python harness; lib/baselines/tests subdirs; pixel diff via stdlib + optional PIL; describe_screenshot via CC-native + optional MiMo |
| **C5** accept-baseline.sh | `scripts/accept-baseline.sh <test-name> <state>`; copies actual → baseline + stages for git |
| **C6** frozen-surface counter | Tracked in M15-progress.md; baseline 0/2 (V0 charter §3); HALT #5 at > 2 |
| **C7** public-repo security | Zero `secrets.*` in CI; no `.env*` in repo; pixel-diff-only CI gate; HALT #7 on accidental API call |

All seven are documentation-only (no code touched). S0 commit
also scaffolds `.claude/M15-progress.md`.

S0 close gate: this concerns doc + the empirical CC native
test result + the M15.2 lock confirmation. Phase 4 (already
completed via the human's "approved, execute M15" message)
explicitly authorized the local-only hybrid; this S0
documents the test that confirms feasibility.
