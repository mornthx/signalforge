#!/usr/bin/env bash
# M15 S5 — accept-baseline workflow (per M15-concerns C5).
#
# Promotes a per-run capture from `tests/screenshots/<state>.png`
# to the committed baseline at
# `tests/visual/baselines/<state>.png`.
#
# Use after a visual-test failure has been reviewed (locally,
# via CC's Read tool or operator's eyes) and the change is
# determined to be intended (UI tweak / theme update / etc.).
#
# Usage:
#   scripts/accept-baseline.sh <state-name>
#   scripts/accept-baseline.sh 04-conn-udp-connected
#
# Stages the new baseline for git commit; does NOT commit
# automatically — the operator reviews + commits with their
# own message.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ $# -lt 1 ]; then
    echo "usage: $0 <state-name>" >&2
    echo "  e.g. $0 00-empty-launch" >&2
    exit 2
fi

STATE="$1"
ACTUAL="$REPO_ROOT/tests/screenshots/${STATE}.png"
BASELINE="$REPO_ROOT/tests/visual/baselines/${STATE}.png"

if [ ! -f "$ACTUAL" ]; then
    echo "no actual screenshot at $ACTUAL" >&2
    echo "  → run the visual test for state '$STATE' first" >&2
    exit 1
fi

mkdir -p "$(dirname "$BASELINE")"
cp "$ACTUAL" "$BASELINE"
( cd "$REPO_ROOT" && git add "${BASELINE#$REPO_ROOT/}" )
echo "staged $BASELINE"
echo "  → review the diff (git diff --staged) + commit with your"
echo "    own message describing what changed visually."
