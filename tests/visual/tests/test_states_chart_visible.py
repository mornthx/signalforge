"""M15 S2 — chart-with-signal baseline test.

Captures SignalForge with the M14 smoke fixture connected +
the temperature signal selected into the first chart.
State 05 from `M15-concerns.md` C3.

Note on rendering: under xvfb + software-RHI, 1-px
QSGGeometryNode line strips do not reliably rasterize (per
ADR-010 §"Implementation lesson"). The captured image will
show the chart pane sized correctly but the actual signal line
may be invisible — operator's real-X11 dogfood (run-6) confirms
the production line renders correctly. The pixel-diff baseline
captures the xvfb behaviour (no line); the optional vision-LLM
verdict will report `chart_contents.lines_visible: false` for
this fixture under software-RHI.

For visual coverage of *what real users see*, V0.3+ may add
a hardware-RHI baseline pass (separate test track or local-only
operator capture). V0.2 ships the headless-CI-deterministic
baseline.
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
STATE = "05-conn-udp-with-signal"


def _ensure_capture():
    actual = REPO_ROOT / "tests" / "screenshots" / f"{STATE}.png"
    if not actual.is_file():
        capture_signalforge_state(
            state_name=STATE,
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
                "--auto-select-signal",
                "udp:m14-smoke-udp/temperature",
            ],
            capture_after_ms=2500,
            exit_after_ms=3500,
            timeout_s=15,
        )
    return actual


def test_chart_with_signal_captures():
    actual = _ensure_capture()
    assert actual.is_file() and actual.stat().st_size > 0


def test_chart_with_signal_matches_baseline():
    actual = _ensure_capture()
    baseline = BASELINES / f"{STATE}.png"
    cmp = compare_baseline(actual, baseline, max_diff_percent=5.0)
    assert cmp.matched, (
        f"visual regression: state='{STATE}' "
        f"diff={cmp.diff_percent:.2f}% threshold=5.0% note={cmp.note}"
    )


def test_chart_with_signal_optional_description():
    """Schema-validate vision-LLM description; operator-local only."""
    actual = _ensure_capture()
    desc = describe_screenshot(actual)
    if desc is None:
        return  # CI default: pixel-diff is the gate

    violations = validate_description(desc)
    assert not violations, f"schema violations: {violations}"
    # We cannot assert chart_contents.lines_visible == True under
    # xvfb + software-RHI (ADR-010 §"Implementation lesson"). The
    # signal-selector tree should however list the signal id; that
    # is implicit — vision-LLM widget enumeration would surface
    # signal_selector in widgets_visible.
    assert "signal_selector" in desc["widgets_visible"], desc
