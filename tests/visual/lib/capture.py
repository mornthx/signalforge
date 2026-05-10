"""M15 S1 — screenshot capture helpers (mechanism C + B).

Mechanism C (in-process): launches SignalForge with the
`--capture-screenshot-after-ms` + `--capture-screenshot-path`
CLI flags + supporting fixture flags. Fast, deterministic,
headless via xvfb-run + software RHI. Default for most
visual tests.

Mechanism B (xvfb + xwd | convert): captures the full X
server framebuffer, including transient windows (menu open,
modal dialogs). Used by tests that need state outside the
SignalForge MainWindow. Requires `xwd` + `convert`
(ImageMagick); both already available in CI per M14 fixture
work.

Tests use mechanism C by default. Per-test opt-in to B via
`mechanism="B"` in `capture_signalforge_state`.

Usage (from a visual test):

    from lib.capture import capture_signalforge_state

    def test_state_connected():
        out = capture_signalforge_state(
            state_name="04-conn-udp-connected",
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
            ],
            capture_after_ms=2000,
            exit_after_ms=3000,
        )
        assert out.exists()

Stdlib only — no PIL / Pillow dep at the harness level.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path


# Repo root resolved relative to this file
REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent


def _resolve_binary() -> Path:
    """Locate the signalforge release binary."""
    env = os.environ.get("SIGNALFORGE_BINARY")
    if env:
        return Path(env)
    # Default: build/release relative to repo root
    candidates = [
        REPO_ROOT / "build" / "release" / "src" / "app" / "signalforge",
        REPO_ROOT / "build" / "debug" / "src" / "app" / "signalforge",
    ]
    for path in candidates:
        if path.is_file() and os.access(path, os.X_OK):
            return path
    raise FileNotFoundError(
        f"signalforge binary not found in any of: {candidates}\n"
        f"Build it (`cmake --build build/release`) or set SIGNALFORGE_BINARY."
    )


def _screenshot_dir() -> Path:
    """Where ephemeral per-test screenshots land.

    Per M15-concerns C4 / C7: gitignored (`tests/screenshots/`); committed
    baselines live in `tests/visual/baselines/` and are managed via
    `scripts/accept-baseline.sh`.
    """
    return REPO_ROOT / "tests" / "screenshots"


@dataclass
class CaptureResult:
    """Outcome of a state capture."""

    actual_path: Path
    state_name: str

    def exists(self) -> bool:
        return self.actual_path.is_file() and self.actual_path.stat().st_size > 0


def capture_signalforge_state(
    state_name: str,
    launch_args: list[str] | None = None,
    capture_after_ms: int = 2000,
    exit_after_ms: int = 3000,
    timeout_s: int = 30,
    mechanism: str = "C",
    config_dir: Path | None = None,
    state_dir: Path | None = None,
) -> CaptureResult:
    """Launch SignalForge, drive `launch_args`, capture a screenshot.

    Returns a CaptureResult with the path of the produced PNG.
    Raises subprocess errors on launch failure.

    `state_name` is used to name the output file under
    `tests/screenshots/<state_name>.png`.

    `mechanism="C"` (default) uses the binary's
    `--capture-screenshot-*` flags (in-process `QWidget::grab()`
    of MainWindow).

    `mechanism="B"` captures the full xvfb framebuffer via
    `xwd | convert` after letting the binary settle. Used for
    states that include transient windows (menu open, dialogs).
    """
    binary = _resolve_binary()
    out_dir = _screenshot_dir()
    out_dir.mkdir(parents=True, exist_ok=True)
    actual = out_dir / f"{state_name}.png"
    actual.unlink(missing_ok=True)

    if config_dir is None or state_dir is None:
        # Per-test isolated XDG dirs so tests don't interfere with
        # each other or the operator's real ~/.config/signalforge/
        with tempfile.TemporaryDirectory(prefix="m15-visual-") as tmp:
            tmp_path = Path(tmp)
            return _run_capture(
                binary=binary,
                state_name=state_name,
                actual=actual,
                launch_args=launch_args or [],
                capture_after_ms=capture_after_ms,
                exit_after_ms=exit_after_ms,
                timeout_s=timeout_s,
                mechanism=mechanism,
                config_dir=tmp_path / "cfg",
                state_dir=tmp_path / "state",
            )
    return _run_capture(
        binary=binary,
        state_name=state_name,
        actual=actual,
        launch_args=launch_args or [],
        capture_after_ms=capture_after_ms,
        exit_after_ms=exit_after_ms,
        timeout_s=timeout_s,
        mechanism=mechanism,
        config_dir=config_dir,
        state_dir=state_dir,
    )


def _run_capture(
    binary: Path,
    state_name: str,
    actual: Path,
    launch_args: list[str],
    capture_after_ms: int,
    exit_after_ms: int,
    timeout_s: int,
    mechanism: str,
    config_dir: Path,
    state_dir: Path,
) -> CaptureResult:
    config_dir.mkdir(parents=True, exist_ok=True)
    state_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env["XDG_CONFIG_HOME"] = str(config_dir)
    env["XDG_STATE_HOME"] = str(state_dir)
    # Software RHI keeps tests deterministic across hosts (avoids
    # GPU-driver variation in baseline pixel counts).
    env["QSG_RHI_BACKEND"] = "software"

    if mechanism == "C":
        cmd = [
            "xvfb-run",
            "--auto-servernum",
            "--server-args=-screen 0 1280x800x24",
            "timeout",
            "--signal=TERM",
            "--kill-after=5",
            f"{timeout_s}s",
            str(binary),
            *launch_args,
            "--capture-screenshot-after-ms",
            str(capture_after_ms),
            "--capture-screenshot-path",
            str(actual),
            "--exit-after-ms",
            str(exit_after_ms),
        ]
        # Run under xvfb with timeout. The binary may segfault on
        # offscreen-platform shutdown (M14 F2 known issue, V0.x
        # backlog) — that's after the screenshot is saved, so it
        # doesn't fail the test. Tolerate non-zero exit code; the
        # test gate is "screenshot file exists + non-empty".
        result = subprocess.run(cmd, env=env, capture_output=True, text=True, check=False)
        # Diagnostic: only log on failure
        if not actual.is_file() or actual.stat().st_size == 0:
            sys.stderr.write(
                f"[capture-C] state_name={state_name} produced no PNG.\n"
                f"  stdout: {result.stdout[-800:]}\n"
                f"  stderr: {result.stderr[-800:]}\n"
            )
            raise RuntimeError(f"capture-C failed for state_name={state_name}")
        return CaptureResult(actual_path=actual, state_name=state_name)

    if mechanism == "B":
        # xvfb + xwd: launch SF backgrounded under xvfb, sleep,
        # xwd -root | convert. Used for menu / dialog states.
        if not shutil.which("xwd") or not shutil.which("convert"):
            raise RuntimeError(
                "mechanism B requires xwd + convert (ImageMagick) — "
                "neither is installed in this environment."
            )
        # Implementation deferred to S4 when the first menu/dialog
        # test needs it. The interface is fixed; the implementation
        # lands per-test.
        raise NotImplementedError(
            "mechanism B (xwd) implementation lands at S4 with the "
            "first menu/dialog test; mechanism C suffices for state "
            "captures of MainWindow content."
        )

    raise ValueError(f"unknown mechanism={mechanism}; expected 'C' or 'B'")
