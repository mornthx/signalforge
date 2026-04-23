#!/usr/bin/env bash
# run-check2.sh — HiDPI driver for M1 Check 2.
#
# Launches the spike under a dedicated Xvfb display at four QT_SCALE_FACTOR
# values and captures each scale's rendering via scrot against that Xvfb.
#
# About the real-display vs. xvfb choice:
#   Approval update #2 requested scrot (not QWidget::grab) for Check 2 to get
#   compositor-authentic evidence. An initial attempt on DISPLAY=:0 failed on
#   this host: scrot -u fell back to the root window (spike didn't reliably
#   take focus on a portrait-rotated desktop), producing byte-identical root-
#   window PNGs. M1 spec §S4 explicitly permits xvfb evidence with the caveat
#   that the report must flag the limitation and the human re-verifies real-
#   compositor crispness on a physical display. This driver takes that path
#   so Check 2 still ships reproducible per-scale artifacts.
#
# Required tools: Xvfb, scrot, the spike binary.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SPIKE="$REPO_ROOT/tools/spike/qquick_dock_test/build/qquick_dock_test"
ART_DIR="$REPO_ROOT/docs/spikes/M1-artifacts"
LOG="$ART_DIR/check2-log.txt"

if [[ ! -x "$SPIKE" ]]; then
    echo "error: spike binary not found at $SPIKE — build first." >&2
    exit 2
fi
for t in Xvfb scrot; do
    if ! command -v $t >/dev/null; then
        echo "error: $t not installed." >&2
        exit 2
    fi
done

mkdir -p "$ART_DIR"
: > "$LOG"

# Pick a free display number.
DISPLAY_NUM=99
while [[ -e "/tmp/.X${DISPLAY_NUM}-lock" ]]; do
    DISPLAY_NUM=$((DISPLAY_NUM + 1))
done
XVFB_DISPLAY=":${DISPLAY_NUM}"

# Screen size: big enough to accommodate a 1280×800 window at scale 2.0
# (2560×1600) with margin.
XVFB_GEOMETRY="3200x2000x24"

echo "Starting Xvfb on $XVFB_DISPLAY (${XVFB_GEOMETRY})" | tee -a "$LOG"
Xvfb "$XVFB_DISPLAY" -screen 0 "$XVFB_GEOMETRY" >/dev/null 2>&1 &
XVFB_PID=$!

# Make sure Xvfb is cleaned up on any exit.
trap 'kill -TERM $XVFB_PID 2>/dev/null || true; wait $XVFB_PID 2>/dev/null || true' EXIT

# Give Xvfb a moment to come up.
sleep 0.5
if ! kill -0 $XVFB_PID 2>/dev/null; then
    echo "error: Xvfb failed to start." | tee -a "$LOG"
    exit 2
fi

for scale in 1.25 1.50 1.75 2.00; do
    echo "=== scale=$scale ===" | tee -a "$LOG"
    DISPLAY="$XVFB_DISPLAY" \
        QT_SCALE_FACTOR=$scale QT_AUTO_SCREEN_SCALE_FACTOR=0 \
        "$SPIKE" --auto-check 2 >>"$LOG" 2>&1 &
    PID=$!
    # Let the window realize and paint a few frames; the spike self-exits at ~3 s.
    sleep 1.8
    DISPLAY="$XVFB_DISPLAY" scrot -o \
        "$ART_DIR/check2-scale-$scale.png" \
        >>"$LOG" 2>&1 \
        || echo "(scrot failed at scale $scale)" | tee -a "$LOG"
    wait "$PID" || true
    echo "--- end scale=$scale ---" >> "$LOG"
done

echo "Check 2 complete. Artifacts:"
ls -la "$ART_DIR"/check2-*.png 2>/dev/null || echo "(no screenshots produced)"
