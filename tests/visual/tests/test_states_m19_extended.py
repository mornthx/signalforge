"""M19 extended visual states.

These tests automate the M15 operator-manual residual states called out by
the V0.3 charter amendment: transient connection states, Serial/TCP local
fixtures, modal/fault flows, and extreme buffer pressure.
"""

from __future__ import annotations

import os
import shutil
import socket
import subprocess
import sys
import threading
import time
from contextlib import contextmanager
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from lib.capture import capture_signalforge_state  # noqa: E402
from lib.compare import compare_with_contract  # noqa: E402
from scripts.capture_baselines import ensure_replay_fixture  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
BASELINES = REPO_ROOT / "tests" / "visual" / "baselines"
GUI_FIXTURES = REPO_ROOT / "tests" / "integration" / "gui" / "fixtures"
SCREENSHOTS = REPO_ROOT / "tests" / "screenshots"
RUN_DIR = SCREENSHOTS / "_runs" / "m19"


def _forced_states() -> set[str]:
    return {
        item.strip()
        for item in os.environ.get("SIGNALFORGE_VISUAL_FORCE_CAPTURE", "").split(",")
        if item.strip()
    }


def _ensure_capture(
    state: str,
    launch_args: list[str] | None = None,
    capture_after_ms: int = 1500,
    exit_after_ms: int = 2500,
    fullscreen: bool = False,
):
    actual = SCREENSHOTS / f"{state}.png"
    forced = _forced_states()
    if not actual.is_file() or state in forced or "all" in forced:
        capture_signalforge_state(
            state_name=state,
            launch_args=launch_args or [],
            capture_after_ms=capture_after_ms,
            exit_after_ms=exit_after_ms,
            timeout_s=25,
            fullscreen=fullscreen,
        )
    return actual


def _assert_baseline(state: str, actual: Path):
    cmp = compare_with_contract(actual, BASELINES / f"{state}.png", require_env_sidecar=False)
    assert cmp.matched, (
        f"visual regression: state='{state}' "
        f"diff={cmp.diff_percent:.3f}% max_cluster={cmp.max_cluster_size}px "
        f"note={cmp.note}"
    )


def _write_fixture(name: str, body: str) -> Path:
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    path = RUN_DIR / name
    path.write_text(body, encoding="utf-8")
    return path


def _serial_fixture(device: str) -> str:
    return f"""schema_version: 1
description: M19 serial visual fixture.
connections:
  - id: m19-serial
    displayName: "M19 serial fixture"
    driverType: serial
    driverConfig:
      device: "{device}"
      baudRate: 115200
      dataBits: 8
      parity: "none"
      stopBits: 1
      flowControl: "none"
    decoderSchemaId: ""
    autoConnectOnStartup: false
    autoConnectCommands: []
"""


def _tcp_fixture(port: int) -> str:
    return f"""schema_version: 1
description: M19 TCP visual fixture.
connections:
  - id: m19-tcp
    displayName: "M19 TCP fixture"
    driverType: tcp
    driverConfig:
      host: "127.0.0.1"
      port: {port}
      connectTimeout: 5000
    decoderSchemaId: ""
    autoConnectOnStartup: false
    autoConnectCommands: []
"""


@contextmanager
def _socat_pty_pair():
    if shutil.which("socat") is None:
        raise RuntimeError("socat is required for M19 serial-connected visual state")
    RUN_DIR.mkdir(parents=True, exist_ok=True)
    left = RUN_DIR / "tty-m19-left"
    right = RUN_DIR / "tty-m19-right"
    left.unlink(missing_ok=True)
    right.unlink(missing_ok=True)
    proc = subprocess.Popen(
        [
            "socat",
            "-d",
            "-d",
            f"pty,link={left},raw,echo=0",
            f"pty,link={right},raw,echo=0",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        deadline = time.time() + 3.0
        while time.time() < deadline:
            if left.exists() and right.exists():
                yield left
                return
            time.sleep(0.05)
        raise RuntimeError("socat PTY links did not appear")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()


@contextmanager
def _tcp_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", 0))
    server.listen(1)
    port = server.getsockname()[1]
    stop = threading.Event()

    def serve() -> None:
        server.settimeout(0.2)
        conn: socket.socket | None = None
        try:
            while not stop.is_set() and conn is None:
                try:
                    conn, _ = server.accept()
                    conn.settimeout(0.2)
                except TimeoutError:
                    pass
                except socket.timeout:
                    pass
            while not stop.is_set() and conn is not None:
                try:
                    data = conn.recv(4096)
                    if not data:
                        time.sleep(0.05)
                except socket.timeout:
                    pass
        finally:
            if conn is not None:
                conn.close()
            server.close()

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    try:
        yield port
    finally:
        stop.set()
        thread.join(timeout=2)


def _capture_and_assert(state: str, *args, **kwargs) -> None:
    actual = _ensure_capture(state, *args, **kwargs)
    assert actual.is_file() and actual.stat().st_size > 0
    _assert_baseline(state, actual)


def test_udp_connecting_state():
    _capture_and_assert(
        "03-conn-udp-connecting",
        launch_args=["--auto-connection-state", "connecting"],
        capture_after_ms=1300,
        exit_after_ms=2300,
    )


def test_udp_disconnecting_state():
    _capture_and_assert(
        "06-conn-udp-disconnecting",
        launch_args=["--auto-connection-state", "disconnecting"],
        capture_after_ms=1300,
        exit_after_ms=2300,
    )


def test_udp_error_state():
    _capture_and_assert(
        "07-conn-udp-error",
        launch_args=["--auto-connection-state", "error"],
        capture_after_ms=1300,
        exit_after_ms=2300,
    )


def test_serial_idle_state():
    fixture = _write_fixture("serial-idle.yaml", _serial_fixture("/tmp/m19-serial-idle"))
    _capture_and_assert(
        "08-conn-serial-idle",
        launch_args=["--auto-no-connect", str(fixture)],
        capture_after_ms=1200,
        exit_after_ms=2200,
    )


def test_serial_connected_state():
    with _socat_pty_pair() as device:
        fixture = _write_fixture("serial-connected.yaml", _serial_fixture(str(device)))
        _capture_and_assert(
            "09-conn-serial-connected",
            launch_args=["--auto-load-test-fixture", str(fixture)],
            capture_after_ms=1800,
            exit_after_ms=2800,
        )


def test_tcp_idle_state():
    fixture = _write_fixture("tcp-idle.yaml", _tcp_fixture(65001))
    _capture_and_assert(
        "10-conn-tcp-idle",
        launch_args=["--auto-no-connect", str(fixture)],
        capture_after_ms=1200,
        exit_after_ms=2200,
    )


def test_tcp_connected_state():
    with _tcp_server() as port:
        fixture = _write_fixture("tcp-connected.yaml", _tcp_fixture(port))
        _capture_and_assert(
            "11-conn-tcp-connected",
            launch_args=["--auto-load-test-fixture", str(fixture)],
            capture_after_ms=1800,
            exit_after_ms=2800,
        )


def test_replay_open_dialog_state():
    _capture_and_assert(
        "16-replay-open-dialog",
        launch_args=["--auto-m19-modal", "replay-open-dialog"],
        capture_after_ms=2600,
        exit_after_ms=3600,
        fullscreen=True,
    )


def test_live_to_replay_confirm_state():
    _capture_and_assert(
        "22-mode-live-to-replay",
        launch_args=[
            "--auto-load-test-fixture",
            str(GUI_FIXTURES / "m14_smoke.yaml"),
            "--auto-m19-modal",
            "live-to-replay",
        ],
        capture_after_ms=2600,
        exit_after_ms=3600,
        fullscreen=True,
    )


def test_replay_to_live_confirm_state():
    replay_fixture = ensure_replay_fixture()
    _capture_and_assert(
        "23-mode-replay-to-live",
        launch_args=[
            "--auto-load-replay",
            str(replay_fixture),
            "--auto-m19-modal",
            "replay-to-live",
        ],
        capture_after_ms=2600,
        exit_after_ms=3600,
        fullscreen=True,
    )


def test_quit_recording_dialog_state():
    recording_path = str(SCREENSHOTS / "27-dialog-quit-recording.sfreplay")
    _capture_and_assert(
        "27-dialog-quit-recording",
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


def test_recording_error_dialog_state():
    _capture_and_assert(
        "28-dialog-recording-error",
        launch_args=["--auto-m19-modal", "recording-error"],
        capture_after_ms=2600,
        exit_after_ms=3600,
        fullscreen=True,
    )


def test_replay_error_dialog_state():
    _capture_and_assert(
        "29-dialog-replay-error",
        launch_args=["--auto-m19-modal", "replay-error"],
        capture_after_ms=2600,
        exit_after_ms=3600,
        fullscreen=True,
    )


def test_extreme_buffer_warning_state():
    _capture_and_assert(
        "34-status-buffer-warn",
        launch_args=["--auto-buffer-status", "warning"],
        capture_after_ms=1200,
        exit_after_ms=2200,
    )


def test_extreme_buffer_full_state():
    _capture_and_assert(
        "35-status-buffer-full",
        launch_args=["--auto-buffer-status", "full"],
        capture_after_ms=1200,
        exit_after_ms=2200,
    )


if __name__ == "__main__":
    from lib.runner import run_tests

    run_tests(globals())
