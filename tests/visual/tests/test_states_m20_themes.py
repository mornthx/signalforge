"""M20 theme and keyboard-focus visual states."""

from __future__ import annotations

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.capture import capture_signalforge_state  # noqa: E402
from lib.compare import compare_with_contract  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
BASELINES = REPO_ROOT / "tests" / "visual" / "baselines"
SCREENSHOTS = REPO_ROOT / "tests" / "screenshots"


def _forced_states() -> set[str]:
    return {
        item.strip()
        for item in os.environ.get("SIGNALFORGE_VISUAL_FORCE_CAPTURE", "").split(",")
        if item.strip()
    }


def _ensure_capture(state: str, launch_args: list[str]) -> Path:
    actual = SCREENSHOTS / f"{state}.png"
    forced = _forced_states()
    if not actual.is_file() or state in forced or "all" in forced:
        capture_signalforge_state(
            state_name=state,
            launch_args=launch_args,
            capture_after_ms=1600,
            exit_after_ms=2600,
            timeout_s=20,
        )
    return actual


def _assert_baseline(state: str, actual: Path) -> None:
    cmp = compare_with_contract(actual, BASELINES / f"{state}.png", require_env_sidecar=False)
    assert cmp.matched, (
        f"visual regression: state='{state}' "
        f"diff={cmp.diff_percent:.3f}% max_cluster={cmp.max_cluster_size}px "
        f"note={cmp.note}"
    )


def _capture_and_assert(state: str, launch_args: list[str]) -> None:
    actual = _ensure_capture(state, launch_args)
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline(state, actual)


def test_dark_theme_empty_launch():
    _capture_and_assert("40-m20-dark-theme", ["--theme", "dark"])


def test_high_contrast_empty_launch():
    _capture_and_assert("41-m20-high-contrast", ["--theme", "high_contrast"])


def test_keyboard_focus_ring_visible():
    _capture_and_assert(
        "42-m20-focus-live-toggle",
        ["--theme", "dark", "--auto-focus-widget", "live_toggle"],
    )


if __name__ == "__main__":
    from lib.runner import run_tests

    run_tests(globals())
