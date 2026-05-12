"""M16 S0.5 R13 minimal-determinism spike capture.

Captures 2 V0.2 baselines under the prototype M16 stack
(Fusion + bundled Inter Regular 12pt + 6-role minimal palette,
no QSS / no manifesto / no generator — per M16-concerns.md
§C7) on whichever environment runs this script.

Outputs:
- `tests/screenshots/m16-spike/<state>.png` (the captured image)
- `tests/screenshots/m16-spike/<state>.env.json` (a minimal env
  dump for R14 / forensic context — full env contract lands at
  M16 S5)

Run locally:
    bash tests/visual/scripts/setup_m16_spike.sh
    python3 tests/visual/scripts/capture_m16_spike.py

Run in CI: same commands; CI workflow `M16 spike capture` step
runs both then uploads `tests/screenshots/m16-spike/**` as the
`m16-spike-<preset>` artifact.

Ephemeral S0.5 infrastructure. Replaced by the proper visual-
test + env-dump pipeline at M16 S3 + S5. Removable when S4
lands.
"""

from __future__ import annotations

import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
SPIKE_OUT = REPO_ROOT / "tests" / "screenshots" / "m16-spike"
SPIKE_FONT_PATH = REPO_ROOT / ".m16-spike" / "fonts" / "Inter-Regular.otf"


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


def _ensure_setup() -> None:
    if SPIKE_FONT_PATH.is_file() and SPIKE_FONT_PATH.stat().st_size > 100000:
        return
    setup_script = REPO_ROOT / "tests" / "visual" / "scripts" / "setup_m16_spike.sh"
    subprocess.run(["bash", str(setup_script)], check=True)


def _dump_env(out_path: Path, captured_state: str) -> None:
    """Minimal env snapshot for R14 / forensic context.

    Not the full M16 S5 env-contract sidecar — that lands at M16 S5
    once `dump_render_env.py` is built. This S0.5 version captures
    the bare minimum: OS, kernel, GTK theme env vars, fontconfig
    version, freetype version, screen geometry (xvfb), `QT_*`
    environment-variable overrides.
    """
    env = {
        "spike": "M16-S0.5",
        "captured_state": captured_state,
        "os": platform.platform(),
        "kernel": platform.uname().release,
        "python_version": sys.version.split()[0],
        "xvfb_screen": os.environ.get("DISPLAY", ""),
        "xdg_current_desktop": os.environ.get("XDG_CURRENT_DESKTOP", ""),
        "desktop_session": os.environ.get("DESKTOP_SESSION", ""),
        "gtk_theme_env": os.environ.get("GTK_THEME", ""),
        "qt_env_overrides": {
            k: v for k, v in os.environ.items()
            if k.startswith("QT_") and k != "QT_QPA_PLATFORM"
        },
        "qpa_platform_env": os.environ.get("QT_QPA_PLATFORM", ""),
        "qsg_rhi_backend_env": os.environ.get("QSG_RHI_BACKEND", ""),
        "spike_font_path": str(SPIKE_FONT_PATH.relative_to(REPO_ROOT)),
        "spike_font_size_bytes": SPIKE_FONT_PATH.stat().st_size if SPIKE_FONT_PATH.is_file() else 0,
        "spike_font_sha256": _read_sha256(SPIKE_FONT_PATH),
    }
    # Optional advisory: fontconfig + freetype versions when fc-list / pkg-config available
    try:
        fc_version = subprocess.run(
            ["fc-list", "--version"], check=False, capture_output=True, text=True, timeout=5
        )
        env["fontconfig_version"] = (fc_version.stderr or fc_version.stdout).strip().split("\n")[0]
    except Exception:  # noqa: BLE001
        env["fontconfig_version"] = "unknown"
    try:
        freetype = subprocess.run(
            ["pkg-config", "--modversion", "freetype2"],
            check=False, capture_output=True, text=True, timeout=5,
        )
        env["freetype_version"] = freetype.stdout.strip()
    except Exception:  # noqa: BLE001
        env["freetype_version"] = "unknown"

    out_path.write_text(json.dumps(env, indent=2, sort_keys=True))


def _read_sha256(path: Path) -> str:
    sha_path = path.with_suffix(path.suffix + ".sha256")
    if sha_path.is_file():
        return sha_path.read_text().strip()
    return ""


def _capture(binary: Path, state_name: str, capture_args: list[str], fullscreen: bool) -> None:
    SPIKE_OUT.mkdir(parents=True, exist_ok=True)
    png = SPIKE_OUT / f"{state_name}.png"
    env_json = SPIKE_OUT / f"{state_name}.env.json"
    png.unlink(missing_ok=True)
    env_json.unlink(missing_ok=True)

    capture_flag = "--capture-fullscreen-after-ms" if fullscreen else "--capture-screenshot-after-ms"
    path_flag = "--capture-fullscreen-path" if fullscreen else "--capture-screenshot-path"

    cmd = [
        "xvfb-run", "--auto-servernum", "--server-args=-screen 0 1280x800x24",
        "timeout", "--signal=TERM", "--kill-after=5", "15s",
        str(binary),
        "--m16-spike-stack",
        "--m16-spike-font-path", str(SPIKE_FONT_PATH),
        *capture_args,
        capture_flag, "2500",
        path_flag, str(png),
        "--exit-after-ms", "3500",
    ]

    env = os.environ.copy()
    # M16 spike under same software-RHI constraint as V0.2 baselines
    # (ADR-010).
    env.setdefault("QSG_RHI_BACKEND", "software")

    # Isolate XDG dirs so the binary doesn't pick up the operator's
    # persisted ConnectionManager config (V0.2 capture_baselines.py
    # pattern). Without this, the "empty-launch" baseline gets
    # contaminated by ~/.local/state/signalforge/.
    print(f"  capturing {state_name} ...", end=" ", flush=True)
    with tempfile.TemporaryDirectory(prefix="m16-spike-") as tmp:
        tmp_path = Path(tmp)
        env["XDG_CONFIG_HOME"] = str(tmp_path / "cfg")
        env["XDG_STATE_HOME"] = str(tmp_path / "state")
        (tmp_path / "cfg").mkdir(parents=True, exist_ok=True)
        (tmp_path / "state").mkdir(parents=True, exist_ok=True)
        result = subprocess.run(cmd, env=env, cwd=REPO_ROOT,
                                capture_output=True, text=True, check=False)

    if not png.is_file() or png.stat().st_size == 0:
        print(f"FAIL\n    stdout: {result.stdout[-400:]}\n    stderr: {result.stderr[-400:]}")
        sys.exit(2)

    _dump_env(env_json, state_name)
    print(f"OK ({png.stat().st_size} bytes)")


def main() -> int:
    print("M16 S0.5 spike capture")
    print(f"  repo:     {REPO_ROOT}")
    print(f"  out:      {SPIKE_OUT.relative_to(REPO_ROOT)}")
    print()

    _ensure_setup()
    if not SPIKE_FONT_PATH.is_file():
        print(f"  setup_m16_spike.sh failed: {SPIKE_FONT_PATH} missing", file=sys.stderr)
        return 3

    binary = _resolve_binary()
    print(f"  binary:   {binary.relative_to(REPO_ROOT)}")
    print(f"  font:     {SPIKE_FONT_PATH.relative_to(REPO_ROOT)} "
          f"({SPIKE_FONT_PATH.stat().st_size} bytes)")
    print()

    # Per M16-concerns.md §C7:
    # baseline 1 = 00-empty-launch (chrome-only diff surface)
    # baseline 2 = 24-dialog-add-serial (text-heavy form diff surface)
    _capture(binary, "00-empty-launch", capture_args=[], fullscreen=False)
    _capture(binary, "24-dialog-add-serial",
             capture_args=["--auto-open-dialog", "add",
                           "--auto-open-dialog-driver", "serial"],
             fullscreen=True)
    print()
    print("M16 S0.5 spike capture: done")
    print(f"  see {SPIKE_OUT.relative_to(REPO_ROOT)}/*.png + *.env.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
