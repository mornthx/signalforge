"""M16 S5 — env-sidecar dump (thin wrapper over signalforge binary).

Per `docs/v0.3/rendering-environment-lock.md` §6 + spec §2.1 #4:
the canonical env sidecar emission for visual baselines runs
inside the signalforge binary via
`SignalForgeStyle::dumpEnvironmentJson` (so the Qt-side state
recorded is exactly what the screenshot capture sees). This
Python script is a thin wrapper that invokes the binary in
standalone env-dump mode:

    signalforge --dump-render-env <output-path>

Used by CI smoke checks + operator forensic flows that want
the env dump without a screenshot. For visual-baseline
captures, the env sidecar is **auto-emitted** alongside every
`--capture-screenshot-path` or `--capture-fullscreen-path`
invocation (M16 S5 main.cpp wiring); this script is only
needed for standalone uses.

Stdlib only per CLAUDE.md §1.

Usage:
    python3 tests/visual/scripts/dump_render_env.py <output-path>
    python3 tests/visual/scripts/dump_render_env.py /tmp/env.json
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent


def _resolve_binary() -> Path:
    env = os.environ.get("SIGNALFORGE_BINARY")
    if env:
        return Path(env)
    for candidate in (
        REPO_ROOT / "build" / "release" / "src" / "app" / "signalforge",
        REPO_ROOT / "build" / "debug" / "src" / "app" / "signalforge",
    ):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    raise FileNotFoundError("signalforge binary not found")


def dump(out_path: Path) -> int:
    """Invoke the binary's --dump-render-env mode under xvfb.

    The binary needs QGuiApplication to query primary-screen DPR /
    geometry / logical_dpi, so we wrap in xvfb-run. The dump-and-
    exit path doesn't construct MainWindow.
    """
    binary = _resolve_binary()
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    if out_path.is_file():
        out_path.unlink()

    cmd = [
        "xvfb-run", "--auto-servernum", "--server-args=-screen 0 1280x800x24",
        "timeout", "--signal=TERM", "--kill-after=5", "8s",
        str(binary),
        "--dump-render-env", str(out_path),
    ]
    env = os.environ.copy()
    env.setdefault("QSG_RHI_BACKEND", "software")
    result = subprocess.run(cmd, env=env, capture_output=True, text=True, check=False)
    if not out_path.is_file() or out_path.stat().st_size < 100:
        sys.stderr.write(
            f"dump_render_env: binary failed to emit {out_path}\n"
            f"  stdout: {result.stdout[-400:]}\n"
            f"  stderr: {result.stderr[-400:]}\n"
        )
        return 2
    return 0


def main() -> int:
    if len(sys.argv) != 2:
        sys.stderr.write("usage: dump_render_env.py <output-path>\n")
        return 2
    return dump(Path(sys.argv[1]))


if __name__ == "__main__":
    sys.exit(main())
