"""M15 S4 — production-fidelity baselines (parametric).

Wires the 10 production-fidelity baselines accepted by the
operator in M15 S3 (per the R7 fidelity audit) that do not
already have a dedicated test file:

  02-conn-udp-idle
  12-multi-2-drivers
  13-multi-5-drivers
  24-dialog-add-serial
  25-dialog-add-udp
  26-dialog-edit
  30-menu-file-open
  31-menu-connections-open
  32-menu-session-open
  33-status-buffer-normal

The 3 existing tests cover 00 (empty), 04 (connected), 05
(with-signal — FIDELITY-FAIL (c), baseline-absent). This file
fills the gap.

Each spec runs:
  1. Capture (if missing): `capture_signalforge_state` with the
     same launch_args + timings used by capture_baselines.py.
  2. Pixel-diff against the committed baseline; assert match
     within 5 % tolerance.

The runner discovers `test_*` callables in module globals,
including the dynamically-generated ones built in the loop at
the bottom of this file.

Per M15-concerns C7 / Phase 5: no vision-LLM verdict here —
pixel-diff is the only CI gate. Local operators can drive
`describe_screenshot()` via `SF_VISUAL_DESCRIBE_BACKEND=mimo`
manually if they want a semantic check.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.capture import capture_signalforge_state  # noqa: E402
from lib.compare import compare_with_contract  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
BASELINES = REPO_ROOT / "tests" / "visual" / "baselines"


@dataclass
class FidelitySpec:
    """Production-fidelity baseline capture recipe.

    Mirrors the corresponding entry in
    `tests/visual/scripts/capture_baselines.py` — kept in sync
    by hand. When the orchestrator's spec drifts, update both.
    """

    name: str
    launch_args: list[str] = field(default_factory=list)
    capture_after_ms: int = 2500
    exit_after_ms: int = 3500
    fullscreen: bool = False


SPECS: list[FidelitySpec] = [
    FidelitySpec(
        name="02-conn-udp-idle",
        launch_args=[
            "--auto-no-connect",
            "tests/integration/gui/fixtures/m14_smoke.yaml",
        ],
    ),
    FidelitySpec(
        name="12-multi-2-drivers",
        launch_args=[
            "--auto-load-test-fixture",
            "tests/integration/gui/fixtures/m15_multi_2.yaml",
        ],
    ),
    FidelitySpec(
        name="13-multi-5-drivers",
        launch_args=[
            "--auto-load-test-fixture",
            "tests/integration/gui/fixtures/m15_multi_5.yaml",
        ],
    ),
    FidelitySpec(
        name="24-dialog-add-serial",
        launch_args=["--auto-open-dialog", "add", "--auto-open-dialog-driver", "serial"],
        fullscreen=True,
    ),
    FidelitySpec(
        name="25-dialog-add-udp",
        launch_args=["--auto-open-dialog", "add", "--auto-open-dialog-driver", "udp"],
        fullscreen=True,
    ),
    FidelitySpec(
        name="26-dialog-edit",
        launch_args=[
            "--auto-load-test-fixture",
            "tests/integration/gui/fixtures/m14_smoke.yaml",
            "--auto-open-dialog",
            "edit",
        ],
        fullscreen=True,
    ),
    FidelitySpec(
        name="30-menu-file-open",
        launch_args=["--auto-open-menu", "File"],
        fullscreen=True,
    ),
    FidelitySpec(
        name="31-menu-connections-open",
        launch_args=["--auto-open-menu", "Connections"],
        fullscreen=True,
    ),
    FidelitySpec(
        name="32-menu-session-open",
        launch_args=["--auto-open-menu", "Session"],
        fullscreen=True,
    ),
    FidelitySpec(
        name="33-status-buffer-normal",
        launch_args=[
            "--auto-load-test-fixture",
            "tests/integration/gui/fixtures/m14_smoke.yaml",
            "--auto-select-signal",
            "udp:m14-smoke-udp/temperature",
        ],
    ),
]

# M16 S7: per-state tolerance hack removed. V0.2 needed it because
# state 13 (5-driver signal-selector) reflowed non-deterministically
# under V0.2's `std::unordered_map` iteration order (root cause of
# the FLAKY 0.999 % consecutive-run diff documented at M15 S3 close).
# ADR-014 (M16 S6.5) sorted the iteration order and the universal
# status-bar mask (M16 S6.6) absorbs the remaining cross-host
# live-counter drift. State 13 now diffs at 0.011 % / cluster 68 vs
# the M16 baseline, well under both M16 close-gate thresholds.


def _ensure_capture(spec: FidelitySpec) -> Path:
    actual = REPO_ROOT / "tests" / "screenshots" / f"{spec.name}.png"
    if not actual.is_file():
        capture_signalforge_state(
            state_name=spec.name,
            launch_args=list(spec.launch_args),
            capture_after_ms=spec.capture_after_ms,
            exit_after_ms=spec.exit_after_ms,
            timeout_s=15,
            fullscreen=spec.fullscreen,
        )
    return actual


def _make_match_test(spec: FidelitySpec):
    """Build a closure that captures + diffs one state under the
    M16 `compare_with_contract` algorithm. Defaults apply:
    `pixel_threshold = 4`, `cluster_threshold = 200 px`,
    `percent_threshold = 1.0 %`, `require_env_sidecar = True`
    (full R14 enforcement). Mask files at `<baseline>.mask.json`
    are auto-discovered."""

    def test_fn() -> None:
        actual = _ensure_capture(spec)
        assert actual.is_file() and actual.stat().st_size > 0, (
            f"capture failed: no PNG at {actual}"
        )
        baseline = BASELINES / f"{spec.name}.png"
        cmp = compare_with_contract(actual, baseline, require_env_sidecar=True)
        assert cmp.matched, (
            f"visual regression: state='{spec.name}' "
            f"diff={cmp.diff_percent:.3f}% max_cluster={cmp.max_cluster_size}px "
            f"note={cmp.note}"
        )

    test_fn.__name__ = f"test_baseline_{spec.name.replace('-', '_')}"
    return test_fn


# Wire each spec into the module's test_* namespace.
for _spec in SPECS:
    _fn = _make_match_test(_spec)
    globals()[_fn.__name__] = _fn


if __name__ == "__main__":
    from lib.runner import run_tests

    run_tests(globals())
