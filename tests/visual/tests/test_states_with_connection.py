"""M15 S2 — connected-state baseline test.

Captures SignalForge with the M14 smoke fixture (one UDP
connection bound to 127.0.0.1:9998) auto-loaded + auto-
connected. State 04 from `M15-concerns.md` C3.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.capture import capture_signalforge_state  # noqa: E402
from lib.compare import compare_baseline  # noqa: E402
from lib.describe import describe_screenshot  # noqa: E402
from lib.validate_description import validate_description  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
BASELINES = REPO_ROOT / "tests" / "visual" / "baselines"
STATE = "04-conn-udp-connected"


def _ensure_capture():
    actual = REPO_ROOT / "tests" / "screenshots" / f"{STATE}.png"
    if not actual.is_file():
        capture_signalforge_state(
            state_name=STATE,
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
            ],
            capture_after_ms=2500,  # let the connection reach Connected
            exit_after_ms=3500,
            timeout_s=15,
        )
    return actual


def test_connected_state_captures():
    actual = _ensure_capture()
    assert actual.is_file() and actual.stat().st_size > 0


def test_connected_state_matches_baseline():
    actual = _ensure_capture()
    baseline = BASELINES / f"{STATE}.png"
    cmp = compare_baseline(actual, baseline, max_diff_percent=5.0)
    assert cmp.matched, (
        f"visual regression: state='{STATE}' "
        f"diff={cmp.diff_percent:.2f}% threshold=5.0% note={cmp.note}"
    )


def test_connected_state_optional_description():
    """Schema-validate vision-LLM description; operator-local only."""
    actual = _ensure_capture()
    desc = describe_screenshot(actual)
    if desc is None:
        return  # CI default: pixel-diff is the gate

    violations = validate_description(desc)
    assert not violations, f"schema violations: {violations}"
    # State invariants for a UDP connection in Connected state
    assert desc["window_state"] in ("connected", "idle"), desc
    assert desc["dialogs_open"] == [], desc
    assert desc["errors_visible"] == [], desc
    # At least one connection visible with a non-empty state
    assert len(desc["connections"]) >= 1, desc
    for c in desc["connections"]:
        assert c["state"], c
