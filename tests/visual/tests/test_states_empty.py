"""M15 S1 smoke — empty-launch baseline.

Captures the SignalForge GUI immediately after launch with no
fixture loaded. This is the simplest visual baseline (state 00
in `M15-concerns.md` C3) and the canary for the
screenshot-capture infrastructure itself.

Usage from ctest:
    pytest tests/visual/tests/test_states_empty.py

Pixel-diff comparison runs once a baseline is committed under
`tests/visual/baselines/00-empty-launch.png`. Until then the
test passes with `note="baseline-absent"` per
`compare_baseline()` semantics — operator approves baseline via
`scripts/accept-baseline.sh 00-empty-launch`.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Make `lib.*` importable regardless of pytest discovery cwd
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.capture import capture_signalforge_state  # noqa: E402
from lib.compare import compare_baseline  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
BASELINES = REPO_ROOT / "tests" / "visual" / "baselines"


def test_empty_launch_captures_full_window():
    """Empty-state launch produces a full-window PNG."""
    result = capture_signalforge_state(
        state_name="00-empty-launch",
        launch_args=[],
        capture_after_ms=1500,
        exit_after_ms=2500,
        timeout_s=15,
    )
    assert result.exists(), f"capture failed: no PNG at {result.actual_path}"


def test_empty_launch_matches_baseline():
    """Pixel-diff vs baseline (5% tolerance for AA / Qt rendering jitter)."""
    actual = REPO_ROOT / "tests" / "screenshots" / "00-empty-launch.png"
    if not actual.is_file():
        # The capture test runs first; if it didn't, capture now
        capture_signalforge_state(
            state_name="00-empty-launch",
            launch_args=[],
            capture_after_ms=1500,
            exit_after_ms=2500,
            timeout_s=15,
        )

    baseline = BASELINES / "00-empty-launch.png"
    cmp = compare_baseline(actual, baseline, max_diff_percent=5.0)
    # When baseline is absent (first run), `compare_baseline()` returns
    # matched=True with note="baseline-absent". Operator approves via
    # accept-baseline.sh.
    assert cmp.matched, (
        f"visual regression: state='00-empty-launch' "
        f"diff={cmp.diff_percent:.2f}% threshold=5.0% note={cmp.note}"
    )
