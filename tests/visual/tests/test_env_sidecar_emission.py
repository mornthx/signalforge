"""M16 S5 — env-sidecar emission unit test.

Invokes the production signalforge binary in
`--dump-render-env` mode and verifies the emitted JSON
satisfies the S3 contract (`compare.py:ENV_CONTRACT_REQUIRED_KEYS`).

Closes the S3 → S5 contract loop: the keys S3 reads must
match the keys S5 writes. Without this test, S6 + S7 would
discover any schema drift only at full-baseline-capture time;
this catches it cheaper.

Run via stdlib runner like other tests.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tests" / "visual"))

from lib.compare import ENV_CONTRACT_REQUIRED_KEYS  # noqa: E402


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


def _nested_get(obj: dict, dotted_path: str):
    cur = obj
    for part in dotted_path.split("."):
        if not isinstance(cur, dict) or part not in cur:
            return None
        cur = cur[part]
    return cur


def _emit_sidecar() -> dict:
    """Invoke the binary's --dump-render-env mode + return parsed JSON."""
    binary = _resolve_binary()
    with tempfile.TemporaryDirectory(prefix="m16-s5-test-") as tmp:
        out_path = Path(tmp) / "env.json"
        cmd = [
            "xvfb-run", "--auto-servernum", "--server-args=-screen 0 1280x800x24",
            "timeout", "--signal=TERM", "--kill-after=5", "8s",
            str(binary),
            "--dump-render-env", str(out_path),
        ]
        env = os.environ.copy()
        env.setdefault("QSG_RHI_BACKEND", "software")
        env["XDG_CONFIG_HOME"] = str(Path(tmp) / "cfg")
        env["XDG_STATE_HOME"]  = str(Path(tmp) / "state")
        Path(env["XDG_CONFIG_HOME"]).mkdir(parents=True, exist_ok=True)
        Path(env["XDG_STATE_HOME"]).mkdir(parents=True, exist_ok=True)
        result = subprocess.run(cmd, env=env, capture_output=True, text=True, check=False)
        assert out_path.is_file() and out_path.stat().st_size > 100, (
            f"binary failed to emit env sidecar; "
            f"stderr={result.stderr[-400:]}"
        )
        return json.loads(out_path.read_text())


def test_env_sidecar_has_all_s3_required_keys():
    """Every key in S3's ENV_CONTRACT_REQUIRED_KEYS must be present in the live sidecar.

    Closes the S3 (consume) ↔ S5 (emit) schema contract.
    """
    env = _emit_sidecar()
    missing = []
    for keypath, source in ENV_CONTRACT_REQUIRED_KEYS:
        v = _nested_get(env, keypath)
        if v is None:
            missing.append(f"{keypath} (source: {source})")
    assert not missing, (
        f"S5 sidecar missing S3-required keys (schema drift between S3 + S5):\n  "
        + "\n  ".join(missing)
    )


def test_env_sidecar_tier1_font_cascade_values():
    """Tier 1 keystone values match M16 contract."""
    env = _emit_sidecar()
    assert _nested_get(env, "tier_1_font_cascade.app_default_family") == "Inter"
    assert _nested_get(env, "tier_1_font_cascade.app_default_size_pt") == 12
    assert _nested_get(env, "tier_1_font_cascade.app_mono_family") == "JetBrains Mono"


def test_env_sidecar_tier2_qt_stack_fusion():
    """Tier 2 records Fusion + xcb + Wayland-disallowed.

    Per operator's S4 caveat: style_object_introspection records
    'Fusion' as the SignalForgeStyle-enforced value, not the
    post-setStyleSheet-wrap introspection (which would be empty).
    """
    env = _emit_sidecar()
    assert _nested_get(env, "tier_2_qt_rendering.qt_version_major_minor") == "6.10"
    assert _nested_get(env, "tier_2_qt_rendering.qpa_platform") == "xcb"
    assert _nested_get(env, "tier_2_qt_rendering.style_object_introspection") == "Fusion"
    assert _nested_get(env, "tier_2_qt_rendering.wayland_disallowed") is True
    assert _nested_get(env, "tier_2_qt_rendering.gpu_rasterization_disallowed") is True
    # The "set-as-applied" note required by operator caveat:
    note = _nested_get(env, "tier_2_qt_rendering.style_recording_note") or ""
    assert "set-as-applied" in note, f"missing operator-required style_recording_note: {note!r}"


def test_env_sidecar_tier3_geometry_string_dpr():
    """Tier 3 DPR is a STRING ('1.0') not a number (avoids Qt JSON
    integer-vs-float serialisation drift between hosts)."""
    env = _emit_sidecar()
    dpr = _nested_get(env, "tier_3_geometry.device_pixel_ratio")
    assert isinstance(dpr, str), f"DPR must be string for cross-host stability; got {type(dpr).__name__}"
    assert dpr == "1.0", f"DPR contract value is '1.0' at M16 (HiDPI is M20); got {dpr!r}"
    assert _nested_get(env, "tier_3_geometry.screen_geometry") == "1280x800"


def test_env_sidecar_auto_emitted_alongside_capture_screenshot_path():
    """`--capture-screenshot-path /path/to/foo.png` auto-emits
    `/path/to/foo.env.json` (R14 / H10 — every baseline ships with
    its env sidecar, no extra flag needed)."""
    binary = _resolve_binary()
    with tempfile.TemporaryDirectory(prefix="m16-s5-auto-") as tmp:
        png = Path(tmp) / "foo.png"
        env_json = Path(tmp) / "foo.env.json"
        cmd = [
            "xvfb-run", "--auto-servernum", "--server-args=-screen 0 1280x800x24",
            "timeout", "--signal=TERM", "--kill-after=5", "15s",
            str(binary),
            "--capture-screenshot-after-ms", "1000",
            "--capture-screenshot-path", str(png),
            "--exit-after-ms", "2500",
        ]
        env = os.environ.copy()
        env.setdefault("QSG_RHI_BACKEND", "software")
        env["XDG_CONFIG_HOME"] = str(Path(tmp) / "cfg")
        env["XDG_STATE_HOME"]  = str(Path(tmp) / "state")
        Path(env["XDG_CONFIG_HOME"]).mkdir(parents=True, exist_ok=True)
        Path(env["XDG_STATE_HOME"]).mkdir(parents=True, exist_ok=True)
        subprocess.run(cmd, env=env, capture_output=True, text=True, check=False)
        assert png.is_file() and png.stat().st_size > 100, "PNG capture failed"
        assert env_json.is_file() and env_json.stat().st_size > 100, (
            f"S5 auto-emit: env sidecar missing at {env_json}"
        )
        # Quick schema-correctness pass
        data = json.loads(env_json.read_text())
        assert _nested_get(data, "tier_1_font_cascade.app_default_family") == "Inter"


if __name__ == "__main__":
    from lib.runner import run_tests
    run_tests(globals())
