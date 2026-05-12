#!/usr/bin/env bash
# accept-baseline — promote a per-run capture to the committed
# baseline at `tests/visual/baselines/<state>.png`.
#
# M15 origin: PNG-only baseline promotion (M15 S5).
# M16 S7 extension: M16's deterministic-rendering baselines pair
# each PNG with a 4-tier env sidecar (`<state>.env.json` —
# auto-emitted by the binary's `--capture-screenshot-path` /
# `--capture-fullscreen-path` flags) and optionally with a mask
# region file (`<state>.mask.json` — per visual-diff-contract
# §1 Step 3, for live-counter regions). This script now promotes
# all three (PNG + env.json + optional mask.json) atomically so
# the R14 environment contract is preserved when an operator
# accepts a new baseline.
#
# Layouts:
#   Default — `tests/screenshots/baseline-candidate/<state>.png`
#             (produced by `tests/visual/scripts/capture_baselines.py`)
#   Empty subdir — `tests/screenshots/<state>.png` (visual-test
#                  write path, used by ctest)
#   Custom subdir — `tests/screenshots/<subdir>/<state>.png`
#
# Usage:
#   scripts/accept-baseline.sh <state-name>
#   scripts/accept-baseline.sh <state-name> <source-subdir>
#
# Examples:
#   scripts/accept-baseline.sh 00-empty-launch
#     -> reads tests/screenshots/baseline-candidate/00-empty-launch.png
#        + tests/screenshots/baseline-candidate/00-empty-launch.env.json
#        + tests/screenshots/baseline-candidate/00-empty-launch.mask.json (if present)
#
#   scripts/accept-baseline.sh 00-empty-launch ""
#     -> reads tests/screenshots/00-empty-launch.png
#        + tests/screenshots/00-empty-launch.env.json
#        + tests/screenshots/00-empty-launch.mask.json (if present)
#
#   scripts/accept-baseline.sh 12-multi-2-drivers m16-s6
#     -> reads tests/screenshots/m16-s6/12-multi-2-drivers.png + .env.json
#        + tests/screenshots/m16-s6/12-multi-2-drivers.mask.json (if present)
#
# Behaviour:
#   - PNG: required; script fails if absent.
#   - env.json sidecar: required if env-contract is in use
#                       (M16 S5+); script warns + still promotes
#                       PNG if absent (V0.2-style accept), but
#                       under M16 a missing sidecar typically
#                       indicates a capture-path bug.
#   - mask.json: optional; promoted only if present at source
#                (per-state mask regions are operator-authored
#                metadata; only states with known dynamic regions
#                need one).
#
# Stages all promoted files for git commit; does NOT commit
# automatically — the operator reviews + commits with their
# own message describing what changed visually.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ $# -lt 1 ]; then
    echo "usage: $0 <state-name> [<source-subdir>]" >&2
    echo "  e.g. $0 00-empty-launch" >&2
    echo "       $0 00-empty-launch \"\"          (visual-test path)" >&2
    echo "       $0 12-multi-2-drivers m16-s6    (custom subdir)" >&2
    exit 2
fi

STATE="$1"
# Default to S3 baseline-candidate layout; override via 2nd arg.
# Empty 2nd arg means "tests/screenshots/<state>.png" (root subdir,
# matches the visual-test write path).
SUBDIR="${2-baseline-candidate}"

if [ -z "$SUBDIR" ]; then
    SRC_DIR="$REPO_ROOT/tests/screenshots"
else
    SRC_DIR="$REPO_ROOT/tests/screenshots/${SUBDIR}"
fi
ACTUAL_PNG="$SRC_DIR/${STATE}.png"
ACTUAL_ENV="$SRC_DIR/${STATE}.env.json"
ACTUAL_MASK="$SRC_DIR/${STATE}.mask.json"

BASELINE_DIR="$REPO_ROOT/tests/visual/baselines"
BASELINE_PNG="$BASELINE_DIR/${STATE}.png"
BASELINE_ENV="$BASELINE_DIR/${STATE}.env.json"
BASELINE_MASK="$BASELINE_DIR/${STATE}.mask.json"

if [ ! -f "$ACTUAL_PNG" ]; then
    echo "no actual screenshot at $ACTUAL_PNG" >&2
    echo "  → run the capture for state '$STATE' first:" >&2
    echo "       python3 tests/visual/scripts/capture_baselines.py \"$STATE\"" >&2
    echo "    or pass an explicit source-subdir as the 2nd arg" >&2
    exit 1
fi

mkdir -p "$BASELINE_DIR"

# Promote PNG — always required.
cp "$ACTUAL_PNG" "$BASELINE_PNG"
( cd "$REPO_ROOT" && git add "${BASELINE_PNG#$REPO_ROOT/}" )
echo "staged $BASELINE_PNG"

# Promote env.json sidecar — required for M16 R14 compliance,
# warn if absent (pre-S5 capture or post-S5 capture-path bug).
if [ -f "$ACTUAL_ENV" ]; then
    cp "$ACTUAL_ENV" "$BASELINE_ENV"
    ( cd "$REPO_ROOT" && git add "${BASELINE_ENV#$REPO_ROOT/}" )
    echo "staged $BASELINE_ENV"
else
    echo "  WARNING: no env-sidecar at $ACTUAL_ENV" >&2
    echo "    → under M16 / R14, every baseline must ship with its env contract sidecar." >&2
    echo "    → the binary auto-emits this alongside --capture-screenshot-path / --capture-fullscreen-path." >&2
    echo "    → if this is a pre-S5 capture, re-capture with the M16 binary." >&2
    if [ -f "$BASELINE_ENV" ]; then
        echo "    NOTE: existing baseline env sidecar at $BASELINE_ENV is left in place (stale risk)." >&2
    fi
fi

# Promote mask.json — optional; only promote if source has one.
# Per visual-diff-contract.md §1 Step 3, mask regions are
# operator-authored metadata for live-counter / known-dynamic
# regions; not every state needs one.
if [ -f "$ACTUAL_MASK" ]; then
    cp "$ACTUAL_MASK" "$BASELINE_MASK"
    ( cd "$REPO_ROOT" && git add "${BASELINE_MASK#$REPO_ROOT/}" )
    echo "staged $BASELINE_MASK"
fi

echo ""
echo "  → review the staged diff (git diff --staged) + commit with your"
echo "    own message describing what changed visually."
