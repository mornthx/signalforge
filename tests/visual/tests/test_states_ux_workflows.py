"""M18 UX workflow visual states.

These states exercise workflow-level affordances rather than only
pixel-stable chrome. They intentionally cover status-strip semantics,
replay transport states, dialog disclosure, and buffer pressure.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.capture import capture_signalforge_state  # noqa: E402
from lib.compare import compare_with_contract  # noqa: E402
from scripts.capture_baselines import ensure_replay_fixture  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
BASELINES = REPO_ROOT / "tests" / "visual" / "baselines"
GUI_FIXTURES = REPO_ROOT / "tests" / "integration" / "gui" / "fixtures"
SCREENSHOTS = REPO_ROOT / "tests" / "screenshots"


def _ensure_capture(
    state: str,
    launch_args: list[str] | None = None,
    capture_after_ms: int = 1500,
    exit_after_ms: int = 2500,
    fullscreen: bool = False,
):
    actual = SCREENSHOTS / f"{state}.png"
    forced = {
        item.strip()
        for item in os.environ.get("SIGNALFORGE_VISUAL_FORCE_CAPTURE", "").split(",")
        if item.strip()
    }
    if not actual.is_file() or state in forced or "all" in forced:
        capture_signalforge_state(
            state_name=state,
            launch_args=launch_args or [],
            capture_after_ms=capture_after_ms,
            exit_after_ms=exit_after_ms,
            timeout_s=20,
            fullscreen=fullscreen,
        )
    return actual


def _assert_baseline_when_present(state: str, actual: Path):
    cmp = compare_with_contract(actual, BASELINES / f"{state}.png", require_env_sidecar=False)
    assert cmp.matched, (
        f"visual regression: state='{state}' "
        f"diff={cmp.diff_percent:.3f}% max_cluster={cmp.max_cluster_size}px "
        f"note={cmp.note}"
    )


def test_empty_guided_workflow_state():
    state = "35-empty-guided-workflow"
    actual = _ensure_capture(state)
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_recording_active_workflow_state():
    state = "36-recording-active"
    recording_path = str(SCREENSHOTS / "36-recording-active.sfreplay")
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-load-test-fixture",
            str(GUI_FIXTURES / "m14_smoke.yaml"),
            "--auto-record-to",
            recording_path,
        ],
        capture_after_ms=1600,
        exit_after_ms=2600,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_replay_playing_workflow_state():
    state = "37-replay-playing"
    replay_fixture = ensure_replay_fixture()
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-load-replay",
            str(replay_fixture),
            "--auto-select-signal",
            "udp:m14-smoke-udp/temperature",
            "--auto-replay-play-after-ms",
            "900",
        ],
        capture_after_ms=1500,
        exit_after_ms=2600,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_advanced_dialog_collapsed_workflow_state():
    state = "38-dialog-add-udp-advanced-collapsed"
    actual = _ensure_capture(
        state,
        launch_args=["--auto-open-dialog", "add", "--auto-open-dialog-driver", "udp"],
        capture_after_ms=2600,
        exit_after_ms=3600,
        fullscreen=True,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_replay_paused_workflow_state():
    state = "39-replay-paused"
    replay_fixture = ensure_replay_fixture()
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-load-replay",
            str(replay_fixture),
            "--auto-select-signal",
            "udp:m14-smoke-udp/temperature",
            "--auto-replay-play-after-ms",
            "900",
            "--auto-replay-pause-after-ms",
            "1250",
        ],
        capture_after_ms=1700,
        exit_after_ms=2800,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_replay_ended_workflow_state():
    state = "40-replay-ended"
    replay_fixture = ensure_replay_fixture()
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-load-replay",
            str(replay_fixture),
            "--auto-select-signal",
            "udp:m14-smoke-udp/temperature",
            "--auto-replay-step-to-end-after-ms",
            "900",
        ],
        capture_after_ms=1500,
        exit_after_ms=2600,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_advanced_dialog_expanded_workflow_state():
    state = "41-dialog-add-udp-advanced-expanded"
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-open-dialog",
            "add",
            "--auto-open-dialog-driver",
            "udp",
            "--auto-open-dialog-advanced",
        ],
        capture_after_ms=2600,
        exit_after_ms=3600,
        fullscreen=True,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_connection_error_workflow_state():
    state = "42-connection-error-workflow"
    actual = _ensure_capture(
        state,
        launch_args=["--auto-load-test-fixture", str(GUI_FIXTURES / "m17_replay_error.yaml")],
        capture_after_ms=2200,
        exit_after_ms=3200,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_buffer_warning_workflow_state():
    state = "43-buffer-warning"
    actual = _ensure_capture(
        state,
        launch_args=["--auto-buffer-status", "warning"],
        capture_after_ms=1200,
        exit_after_ms=2200,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_buffer_full_workflow_state():
    state = "44-buffer-full"
    actual = _ensure_capture(
        state,
        launch_args=["--auto-buffer-status", "full"],
        capture_after_ms=1200,
        exit_after_ms=2200,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_replay_close_confirm_workflow_state():
    state = "45-replay-close-confirm"
    replay_fixture = ensure_replay_fixture()
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-load-replay",
            str(replay_fixture),
            "--auto-select-signal",
            "udp:m14-smoke-udp/temperature",
            "--auto-close-window-after-ms",
            "1700",
        ],
        capture_after_ms=2200,
        exit_after_ms=3200,
        fullscreen=True,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_recording_close_confirm_workflow_state():
    state = "46-recording-close-confirm"
    recording_path = str(SCREENSHOTS / "46-recording-close-confirm.sfreplay")
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-load-test-fixture",
            str(GUI_FIXTURES / "m14_smoke.yaml"),
            "--auto-record-to",
            recording_path,
            "--auto-close-window-after-ms",
            "1500",
        ],
        capture_after_ms=2100,
        exit_after_ms=3200,
        fullscreen=True,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_config_save_failed_close_confirm_workflow_state():
    state = "47-config-save-failed-close-confirm"
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-config-save-status",
            "failed",
            "--auto-close-window-after-ms",
            "1100",
        ],
        capture_after_ms=1700,
        exit_after_ms=2700,
        fullscreen=True,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_config_save_failed_connection_panel_workflow_state():
    state = "48-config-save-failed-connection-panel"
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-config-save-status",
            "failed",
        ],
        capture_after_ms=1100,
        exit_after_ms=2100,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


def test_chart_data_interrupted_workflow_state():
    state = "49-chart-data-interrupted"
    actual = _ensure_capture(
        state,
        launch_args=[
            "--auto-chart-status",
            "interrupted",
        ],
        capture_after_ms=1100,
        exit_after_ms=2100,
    )
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline_when_present(state, actual)


if __name__ == "__main__":
    from lib.runner import run_tests

    run_tests(globals())
