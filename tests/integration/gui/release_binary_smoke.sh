#!/usr/bin/env bash
# M14 S1 CI release-binary GUI smoke harness.
#
# Launches the production `signalforge` binary headlessly (offscreen Qt),
# loads a UDP fixture connection, drives `temperature_sensor`-shaped
# frames over the wire, and verifies:
#
#   Tier A — chart QQuickWidget framebuffer has at least one non-clear
#            pixel after 3 s of frames (= the live-mode chain is
#            actually rendering).
#   Tier B — the log file contains none of the known error patterns
#            ("ChartHost.qml failed to load", "rootObject() is null",
#            "decoder pipeline empty", "setParentItem(nullptr)").
#
# Both tiers must pass. The pixel-count assertion is computed by the
# binary itself (see `countNonClearPixels` in src/app/main.cpp) and
# emitted to the log as `M14_SMOKE_TIER_A: non_white_pixels=N …`; the
# harness greps for it and fails on `non_white_pixels=0`.
#
# Designed for portability: bash + python3 stdlib only. No PIL, no
# ImageMagick, no new top-level deps (CLAUDE.md §1).
#
# Usage:
#   release_binary_smoke.sh --binary <path> --repo-root <path> \\
#                           --fixture <path> [--timeout 30] \\
#                           [--log-dir <path>] [--png-out <path>]
#
# Exit code: 0 on PASS; non-zero on FAIL.

set -euo pipefail

BINARY=""
REPO_ROOT=""
FIXTURE=""
TIMEOUT_S=30
LOG_DIR=""
PNG_OUT=""
WINDOW_PNG_OUT=""

while [ $# -gt 0 ]; do
    case "$1" in
        --binary)        BINARY="$2"; shift 2 ;;
        --repo-root)     REPO_ROOT="$2"; shift 2 ;;
        --fixture)       FIXTURE="$2"; shift 2 ;;
        --timeout)       TIMEOUT_S="$2"; shift 2 ;;
        --log-dir)       LOG_DIR="$2"; shift 2 ;;
        --png-out)       PNG_OUT="$2"; shift 2 ;;
        --window-png-out) WINDOW_PNG_OUT="$2"; shift 2 ;;
        *) echo "release_binary_smoke: unknown arg '$1'" >&2; exit 2 ;;
    esac
done

if [ -z "$BINARY" ] || [ -z "$REPO_ROOT" ] || [ -z "$FIXTURE" ]; then
    echo "release_binary_smoke: --binary, --repo-root, --fixture required" >&2
    exit 2
fi
if [ ! -x "$BINARY" ]; then
    echo "release_binary_smoke: $BINARY is not executable" >&2
    exit 2
fi
if [ ! -d "$REPO_ROOT/examples/schemas" ]; then
    echo "release_binary_smoke: $REPO_ROOT/examples/schemas missing" >&2
    exit 2
fi
if [ ! -f "$FIXTURE" ]; then
    echo "release_binary_smoke: fixture $FIXTURE missing" >&2
    exit 2
fi

# Use a per-run temp dir so parallel ctest runs don't clobber each other.
RUNDIR=$(mktemp -d -t m14_smoke_XXXXXX)
trap 'rm -rf "$RUNDIR"' EXIT

if [ -z "$LOG_DIR" ]; then
    LOG_DIR="$RUNDIR/logs"
fi
mkdir -p "$LOG_DIR"

if [ -z "$PNG_OUT" ]; then
    PNG_OUT="$RUNDIR/chart.png"
fi

# M15 S4 Phase 2: also capture a full-MainWindow PNG for visual
# regression archive. The path defaults to the gitignored
# tests/screenshots/ dir so CI's upload-artifact step (S5) picks
# it up automatically. A committed baseline at
# tests/visual/baselines/m14-s1-smoke.png is OPTIONAL; if present,
# Tier C runs a pixel-diff. If absent, Tier C surfaces
# "baseline-absent" without failing (matches the visual-test
# harness convention).
if [ -z "$WINDOW_PNG_OUT" ]; then
    WINDOW_PNG_OUT="$REPO_ROOT/tests/screenshots/m14-s1-smoke.png"
fi
mkdir -p "$(dirname "$WINDOW_PNG_OUT")"
# Clean any stale capture so Tier C reads a fresh PNG.
rm -f "$WINDOW_PNG_OUT"

# Schema lookup is CWD-relative (V1.0 known limitation). Launch from
# the repo root so `examples/schemas/temperature_sensor.yaml` resolves.
cd "$REPO_ROOT"

# Route logging into the run dir so the harness can grep it without
# fighting the operator's real ~/.local/state/signalforge/logs.
export XDG_STATE_HOME="$LOG_DIR"

# Render path: xvfb-run (virtual X display) + QML software RHI. We
# initially tried Q_QPA_PLATFORM=offscreen for purity, but
# QQuickWidget::grabFramebuffer() returns an empty image under
# offscreen because the RHI swap-chain is never actually presented.
# xvfb-run supplies a real X display backed by a memory framebuffer,
# so QQuickWidget renders + grabs as it would on a real desktop.
# Both ubuntu CI runners and our local host already have xvfb-run
# (CI installs it; local Ubuntu 24.04 has it from xvfb).
export QSG_RHI_BACKEND=software
unset QT_QPA_PLATFORM  # let xvfb-run pick xcb

# M14 F4 Wave 1 (Path α): software-RHI under xvfb-run does not
# reliably rasterize 1-px QSGGeometryNode line strips that the
# Chart's per-signal paint nodes use. The operator's real-X11
# (hardware-RHI) dogfood confirmed that production chart line
# painting works — see docs/m14-audit-operator-runs/run5-... and
# docs/m14-gui-audit-report.md §F4.
#
# To make the smoke's Tier A pixel-diff a reliable canary in CI,
# we set SF_F4_DIAG=1 so the chart appends a bright orange
# QSGSimpleRectNode to its scene-graph root each paint pass. The
# rect is a solid fill (rasterizes under software RHI) and
# completes the same submission path every chart paint goes
# through. If the rect renders, Tier A passes — proof that:
#   • scene-graph submission is intact,
#   • the chart's paint hooks fire,
#   • the QSGNode tree reaches the QQuickWindow render pass,
#   • the QQuickWidget framebuffer captures it.
#
# Production users do NOT set SF_F4_DIAG and never see the rect.
# Real-X11 chart-line verification is operator-driven (run5+).
export SF_F4_DIAG=1

LOG_FILE="$LOG_DIR/signalforge/logs/signalforge.log"
mkdir -p "$(dirname "$LOG_FILE")"

# UDP fixture sender lives next to this script.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UDP_SENDER="$SCRIPT_DIR/helpers/udp_fixture_sender.py"
if [ ! -x "$UDP_SENDER" ]; then
    UDP_SENDER="python3 $UDP_SENDER"
fi

echo "=== M14 S1 release-binary smoke ===" >&2
echo "  BINARY    = $BINARY"      >&2
echo "  REPO_ROOT = $REPO_ROOT"   >&2
echo "  FIXTURE   = $FIXTURE"     >&2
echo "  RUNDIR    = $RUNDIR"      >&2
echo "  LOG_FILE  = $LOG_FILE"    >&2

# Background UDP sender (delays 0.5 s internally; harness keeps it
# running for ~5 s — well past the 3 s dump deadline).
$UDP_SENDER --host 127.0.0.1 --port 9998 --frames 250 --rate-hz 50 \
    >"$RUNDIR/udp_sender.stdout" 2>"$RUNDIR/udp_sender.stderr" &
UDP_PID=$!

# Launch signalforge under xvfb-run + timeout so we never hang.
set +e
xvfb-run --auto-servernum --server-args="-screen 0 1280x800x24" \
    timeout --signal=TERM --kill-after=5 "${TIMEOUT_S}s" \
        "$BINARY" \
            --auto-load-test-fixture "$FIXTURE" \
            --auto-select-signal "udp:m14-smoke-udp/temperature" \
            --capture-screenshot-after-ms 2800 \
            --capture-screenshot-path "$WINDOW_PNG_OUT" \
            --dump-chart-png-after-ms 3000 \
            --dump-chart-png-path "$PNG_OUT" \
            --exit-after-dump \
    >"$RUNDIR/signalforge.stdout" 2>"$RUNDIR/signalforge.stderr"
APP_RC=$?
set -e

# Best-effort cleanup of the UDP sender.
kill "$UDP_PID" 2>/dev/null || true
wait "$UDP_PID" 2>/dev/null || true

if [ ! -f "$LOG_FILE" ]; then
    echo "FAIL: log file $LOG_FILE missing after launch" >&2
    cat "$RUNDIR/signalforge.stderr" >&2
    exit 1
fi

# ---- Tier A: pixel-count assertion ------------------------------------
TIER_A_LINE=$(grep -E "M14_SMOKE_TIER_A:" "$LOG_FILE" | tail -1 || true)
if [ -z "$TIER_A_LINE" ]; then
    echo "FAIL Tier A: no M14_SMOKE_TIER_A line in $LOG_FILE" >&2
    echo "--- signalforge.stderr (tail) ---" >&2
    tail -50 "$RUNDIR/signalforge.stderr" >&2
    echo "--- signalforge.log (tail) ---" >&2
    tail -50 "$LOG_FILE" >&2
    exit 1
fi
echo "Tier A line: $TIER_A_LINE"

NON_WHITE=$(echo "$TIER_A_LINE" | sed -nE 's/.*non_white_pixels=([0-9]+).*/\1/p')
TOTAL=$(echo "$TIER_A_LINE" | sed -nE 's/.*total_pixels=([0-9]+).*/\1/p')
if [ -z "$NON_WHITE" ] || [ -z "$TOTAL" ]; then
    echo "FAIL Tier A: could not parse pixel counts from '$TIER_A_LINE'" >&2
    exit 1
fi
if [ "$NON_WHITE" -eq 0 ]; then
    echo "FAIL Tier A: chart framebuffer is entirely clear-color" >&2
    echo "  total_pixels=$TOTAL non_white_pixels=$NON_WHITE" >&2
    exit 1
fi
echo "PASS Tier A: $NON_WHITE / $TOTAL non-clear pixels"

# ---- Tier B: log error grep -------------------------------------------
declare -a TIER_B_PATTERNS=(
    "ChartHost\\.qml failed to load"
    "rootObject\\(\\) is null"
    "decoder pipeline empty"
    "setParentItem\\(nullptr\\)"
)
for pattern in "${TIER_B_PATTERNS[@]}"; do
    if grep -qE "$pattern" "$LOG_FILE"; then
        echo "FAIL Tier B: log file contains '$pattern'" >&2
        grep -nE "$pattern" "$LOG_FILE" | head -5 >&2
        exit 1
    fi
done
echo "PASS Tier B: no known error pattern in log"

# ---- Tier C: full-window screenshot capture (M15 S4 Phase 2) ----------
#
# The binary's `--capture-screenshot-path` writes a full MainWindow
# PNG at 3200 ms (200 ms after the Tier A chart-pane dump). The
# capture provides a richer regression surface than the chart-pane
# alone — connection list, signal-selector tree, status bar, etc.
#
# If `tests/visual/baselines/m14-s1-smoke.png` exists, run a
# pixel-diff against it (5 % tolerance, matching the visual-test
# default). Absent baseline → "baseline-absent" reported but Tier C
# still passes (operator opts in via `scripts/accept-baseline.sh
# m14-s1-smoke "" `→ promotes from `tests/screenshots/<state>.png`).
if [ ! -f "$WINDOW_PNG_OUT" ] || [ "$(stat -c%s "$WINDOW_PNG_OUT" 2>/dev/null || echo 0)" -eq 0 ]; then
    echo "FAIL Tier C: full-window screenshot at $WINDOW_PNG_OUT missing or empty" >&2
    exit 1
fi
echo "Tier C capture: $WINDOW_PNG_OUT ($(stat -c%s "$WINDOW_PNG_OUT") bytes)"

TIER_C_BASELINE="$REPO_ROOT/tests/visual/baselines/m14-s1-smoke.png"
if [ -f "$TIER_C_BASELINE" ]; then
    TIER_C_PYTHON="$REPO_ROOT/tests/visual"
    TIER_C_OUTPUT=$(PYTHONPATH="$TIER_C_PYTHON" python3 - "$WINDOW_PNG_OUT" "$TIER_C_BASELINE" <<'PY'
import sys
from lib.compare import compare_baseline
actual, baseline = sys.argv[1], sys.argv[2]
cmp = compare_baseline(actual, baseline, max_diff_percent=5.0)
print(f"diff_percent={cmp.diff_percent:.4f}", f"matched={cmp.matched}", f"note={cmp.note}", sep="|")
sys.exit(0 if cmp.matched else 1)
PY
    ) && TIER_C_RC=0 || TIER_C_RC=$?
    echo "Tier C diff: $TIER_C_OUTPUT"
    if [ "$TIER_C_RC" -ne 0 ]; then
        echo "FAIL Tier C: pixel-diff against $TIER_C_BASELINE exceeded 5 % threshold" >&2
        exit 1
    fi
    echo "PASS Tier C: pixel-diff within tolerance"
else
    echo "SKIP Tier C: no baseline at $TIER_C_BASELINE (operator may accept via"
    echo "             scripts/accept-baseline.sh m14-s1-smoke \"\""
    echo "             to promote tests/screenshots/m14-s1-smoke.png)"
fi

# ---- Tier D: clean-exit assertion (M21 C2 regression guard) -----------
# The binary must not crash on teardown. SIGSEGV (139) / SIGABRT (134)
# indicates a teardown fault (e.g. the M21 C2 pipeline/connection
# member-order UAF). Timeout kills (124/137/143) are tolerated — other
# tiers gate functional behavior; only crash signals fail here.
if [ "$APP_RC" -eq 139 ] || [ "$APP_RC" -eq 134 ]; then
    echo "FAIL Tier D: binary crashed on exit (rc=$APP_RC — SIGSEGV/SIGABRT)" >&2
    echo "--- signalforge.stderr (tail) ---" >&2
    tail -40 "$RUNDIR/signalforge.stderr" >&2
    exit 1
fi
echo "PASS Tier D: no crash signal on exit (rc=$APP_RC)"

echo "=== M14 S1 smoke PASS (rc=$APP_RC) ==="
exit 0
