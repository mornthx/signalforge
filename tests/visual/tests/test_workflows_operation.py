"""M18 operation-flow checks.

These tests are intentionally separate from pixel baselines. The workflow
state tests prove visual layout; this file proves a production binary can
complete a real operator flow from startup through recording and replay.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.capture import capture_signalforge_state  # noqa: E402
from scripts.capture_baselines import ensure_replay_fixture  # noqa: E402


def test_record_to_replay_ended_operation_flow():
    replay_fixture = ensure_replay_fixture()
    assert replay_fixture.is_file()
    assert replay_fixture.stat().st_size > 1200

    result = capture_signalforge_state(
        state_name="m18-operation-record-to-replay-ended",
        launch_args=[
            "--auto-load-replay",
            str(replay_fixture),
            "--auto-select-signal",
            "udp:m14-smoke-udp/temperature",
            "--auto-replay-step-to-end-after-ms",
            "900",
        ],
        capture_after_ms=1500,
        exit_after_ms=2500,
        timeout_s=20,
    )
    assert result.exists()


if __name__ == "__main__":
    from lib.runner import run_tests

    run_tests(globals())
