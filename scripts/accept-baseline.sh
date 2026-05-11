#!/usr/bin/env bash
# M15 S5 / S3 — accept-baseline workflow (per M15-concerns C5).
#
# Promotes a per-run capture to the committed baseline at
# `tests/visual/baselines/<state>.png`.
#
# S3 layout (default): captures live under
# `tests/screenshots/baseline-candidate/<state>.png` (produced
# by `tests/visual/scripts/capture_baselines.py`). The visual-
# regression test path used by ctest writes to
# `tests/screenshots/<state>.png` directly; that path is the
# fallback subdir lookup.
#
# Use after a capture has been reviewed (locally, via CC's
# Read tool or operator's eyes) and the change is determined
# to be the new accepted baseline (intended UI change, V0.2
# production-fidelity baseline, etc.).
#
# Usage:
#   scripts/accept-baseline.sh <state-name>
#   scripts/accept-baseline.sh <state-name> <source-subdir>
#
# Examples:
#   scripts/accept-baseline.sh 00-empty-launch
#     -> reads tests/screenshots/baseline-candidate/00-empty-launch.png
#   scripts/accept-baseline.sh 00-empty-launch ""
#     -> reads tests/screenshots/00-empty-launch.png (visual-test path)
#   scripts/accept-baseline.sh 00-empty-launch per-test/empty-suite
#     -> reads tests/screenshots/per-test/empty-suite/00-empty-launch.png
#
# Stages the new baseline for git commit; does NOT commit
# automatically — the operator reviews + commits with their
# own message.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ $# -lt 1 ]; then
    echo "usage: $0 <state-name> [<source-subdir>]" >&2
    echo "  e.g. $0 00-empty-launch" >&2
    echo "       $0 00-empty-launch \"\"     (visual-test path)" >&2
    exit 2
fi

STATE="$1"
# Default to S3 baseline-candidate layout; override via 2nd arg.
# Empty 2nd arg means "tests/screenshots/<state>.png" (root subdir,
# matches the visual-test write path).
SUBDIR="${2-baseline-candidate}"

if [ -z "$SUBDIR" ]; then
    ACTUAL="$REPO_ROOT/tests/screenshots/${STATE}.png"
else
    ACTUAL="$REPO_ROOT/tests/screenshots/${SUBDIR}/${STATE}.png"
fi
BASELINE="$REPO_ROOT/tests/visual/baselines/${STATE}.png"

if [ ! -f "$ACTUAL" ]; then
    echo "no actual screenshot at $ACTUAL" >&2
    echo "  → run the capture for state '$STATE' first:" >&2
    echo "       python3 tests/visual/scripts/capture_baselines.py \"$STATE\"" >&2
    echo "    or pass an explicit source-subdir as the 2nd arg" >&2
    exit 1
fi

mkdir -p "$(dirname "$BASELINE")"
cp "$ACTUAL" "$BASELINE"
( cd "$REPO_ROOT" && git add "${BASELINE#$REPO_ROOT/}" )
echo "staged $BASELINE"
echo "  → review the diff (git diff --staged) + commit with your"
echo "    own message describing what changed visually."
