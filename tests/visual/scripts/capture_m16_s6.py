"""M16 S6 — cross-environment determinism capture for the 12 V0.2
production-fidelity baselines under M16 SignalForgeStyle rendering.

Runs `capture_signalforge_state` (M15 S3 infrastructure) against
each of the 12 V0.2 baselines listed in `M15-done.md §3 / final
inventory`. SignalForgeStyle::applyAtStartup is always-active in
production at S4, so this captures the M16 stack directly; no
spike flag is needed.

S5 wired auto-emit of the env-sidecar (`<stem>.env.json`)
alongside `--capture-screenshot-path` / `--capture-fullscreen-path`,
so each capture produces a PNG + a 4-tier env contract sidecar in
lockstep. Output:

    tests/screenshots/m16-s6/<state>.png
    tests/screenshots/m16-s6/<state>.env.json

Stdlib only per CLAUDE.md §1. Invoked via:

    python3 tests/visual/scripts/capture_m16_s6.py

CI runs the same command from `.github/workflows/ci.yml` (M16 S6
step). After both captures land, `compare_with_contract` (S3) is
applied to each LOCAL/CI pair to produce the §2 per-baseline
diff table in `docs/v0.3/s6-cross-env-verification.md`.

Per M16-plan §S6: gate is < 1 % per baseline (M16 close gate);
< 0.3 % stronger op criterion based on S4 keystone.
"""

from __future__ import annotations

import shutil
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tests" / "visual"))
sys.path.insert(0, str(REPO_ROOT / "tests" / "visual" / "scripts"))

from lib.capture import capture_signalforge_state  # noqa: E402
from capture_baselines import all_specs  # noqa: E402

# Operator-locked 12 V0.2 production-fidelity baselines (per the
# S6 authorization prompt; matches `M15-done.md §3` final
# inventory).
V02_BASELINES_12 = (
    "00-empty-launch",
    "02-conn-udp-idle",
    "04-conn-udp-connected",
    "12-multi-2-drivers",
    "13-multi-5-drivers",
    "24-dialog-add-serial",
    "25-dialog-add-udp",
    "26-dialog-edit",
    "30-menu-file-open",
    "31-menu-connections-open",
    "32-menu-session-open",
    "33-status-buffer-normal",
)

OUT_DIR = REPO_ROOT / "tests" / "screenshots" / "m16-s6"
SCREENSHOTS_DIR = REPO_ROOT / "tests" / "screenshots"


def _move_pair(state_name: str) -> tuple[bool, str]:
    """Move PNG + env.json from tests/screenshots/ to OUT_DIR/.

    Returns (ok, note). Both files must exist; missing env sidecar
    is a fail (R14 — S5 must auto-emit it; absence implies binary
    regression).
    """
    png_src = SCREENSHOTS_DIR / f"{state_name}.png"
    env_src = SCREENSHOTS_DIR / f"{state_name}.env.json"
    if not png_src.is_file() or png_src.stat().st_size < 100:
        return False, f"PNG missing or empty at {png_src}"
    if not env_src.is_file() or env_src.stat().st_size < 100:
        return False, f"env sidecar missing or empty at {env_src} (S5 auto-emit broken?)"
    png_dst = OUT_DIR / f"{state_name}.png"
    env_dst = OUT_DIR / f"{state_name}.env.json"
    shutil.move(str(png_src), str(png_dst))
    shutil.move(str(env_src), str(env_dst))
    return True, f"PNG {png_dst.stat().st_size}B + env {env_dst.stat().st_size}B"


def _ensure_replay_fixture_if_needed(specs) -> None:
    """Bootstrap the replay fixture if any selected spec needs it.

    None of the 12 V0.2 production-fidelity baselines uses replay
    (per V02_BASELINES_12 vs `--auto-load-replay` specs in
    capture_baselines.py), so this is a defensive no-op; kept for
    forward compatibility if V0.4+ adds replay states to the
    M16-equivalent list.
    """
    if any("--auto-load-replay" in s.launch_args for s in specs):
        from capture_baselines import ensure_replay_fixture
        ensure_replay_fixture()


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    specs_by_name = {s.name: s for s in all_specs()}
    selected = []
    for name in V02_BASELINES_12:
        if name not in specs_by_name:
            print(f"ERROR: spec for '{name}' not found in capture_baselines.all_specs()",
                  file=sys.stderr)
            return 2
        spec = specs_by_name[name]
        if spec.mechanism != "C":
            print(f"ERROR: spec '{name}' has mechanism={spec.mechanism!r}, "
                  f"S6 expects C (all 12 V0.2 baselines are headless-capturable)",
                  file=sys.stderr)
            return 2
        selected.append(spec)

    _ensure_replay_fixture_if_needed(selected)

    print(f"M16 S6 — capturing {len(selected)} V0.2 baselines under M16 SignalForgeStyle")
    print(f"  output dir: {OUT_DIR.relative_to(REPO_ROOT)}")
    print()

    pad = max(len(s.name) for s in selected)
    ok_count = 0
    fail_count = 0
    for spec in selected:
        sys.stdout.write(f"  {spec.name.ljust(pad)}  [fs={int(spec.fullscreen)}]  ... ")
        sys.stdout.flush()
        try:
            res = capture_signalforge_state(
                state_name=spec.name,
                launch_args=list(spec.launch_args),
                capture_after_ms=spec.capture_after_ms,
                exit_after_ms=spec.exit_after_ms,
                timeout_s=spec.timeout_s,
                mechanism="C",
                fullscreen=spec.fullscreen,
            )
            if not res.exists():
                print(f"ERROR: capture file missing at {res.actual_path}")
                fail_count += 1
                continue
            ok, note = _move_pair(spec.name)
            if ok:
                print(f"PASS  {note}")
                ok_count += 1
            else:
                print(f"FAIL  {note}")
                fail_count += 1
        except Exception as exc:  # noqa: BLE001
            print(f"ERROR {type(exc).__name__}: {exc}")
            fail_count += 1
        time.sleep(0.2)

    print()
    print(f"Summary: PASS={ok_count} FAIL={fail_count} of {len(selected)}")
    return 0 if fail_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
