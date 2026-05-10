"""M15 S3 — baseline candidate capture orchestrator.

Captures each of the 38 states from M15-concerns C3 into
`tests/screenshots/baseline-candidate/<state>.png`. For each
state, runs the capture three times and verifies pixel
stability (diff < 0.5 % between consecutive runs); a flaky
state is flagged in the report.

Output:
- `tests/screenshots/baseline-candidate/<state>.png` (the
  third stable capture)
- A report printed to stdout summarising per-state status:
  PASS (3/3 stable), FLAKY (k/3 stable), SKIP (mechanism
  not yet implemented), ERROR (capture failed).

Stdlib only — no pytest. Invoked via
`python3 tests/visual/scripts/capture_baselines.py [filter]`.

When `filter` is provided, only states matching the substring
are captured (useful for incremental S3 work).
"""

from __future__ import annotations

import shutil
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tests" / "visual"))

from lib.capture import capture_signalforge_state  # noqa: E402
from lib.compare import compare_baseline  # noqa: E402

CAND_DIR = REPO_ROOT / "tests" / "screenshots" / "baseline-candidate"
TMP_RUN_DIR = REPO_ROOT / "tests" / "screenshots" / "_runs"


@dataclass
class StateSpec:
    """Per-state capture recipe."""

    name: str
    """Filename stem (e.g. ``00-empty-launch``)."""

    description: str
    """Human-friendly description, mirrors C3 table."""

    mechanism: str = "C"
    """``C`` = in-process MainWindow grab. ``B`` = full screen
    grab (xvfb / Qt screen). ``manual`` = operator captures
    locally (transient or hardware-required state)."""

    launch_args: list[str] = field(default_factory=list)
    """Argv passed to signalforge between the binary path and
    the M15 capture flags."""

    capture_after_ms: int = 2500
    """When to fire the screenshot QTimer."""

    exit_after_ms: int = 3500
    """When to issue QApplication::quit."""

    timeout_s: int = 15
    """Hard timeout for the launched process."""

    note: str = ""
    """Free-form note surfaced in the report."""


@dataclass
class StateReport:
    """Outcome of capturing a state."""

    spec: StateSpec
    status: str  # PASS / FLAKY / SKIP / ERROR
    diffs: list[float] = field(default_factory=list)
    note: str = ""


def specs_phase_a() -> list[StateSpec]:
    """Phase A: states already capturable with existing CLI."""
    return [
        StateSpec(
            name="00-empty-launch",
            description="Just-launched MainWindow, no fixture, no chart, no signals",
            launch_args=[],
            note="Empty mode — no autoLoad fixture. Captures default chart-pane chrome.",
        ),
        StateSpec(
            name="04-conn-udp-connected",
            description="UDP connection bound to 127.0.0.1:9998 via m14_smoke fixture",
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
            ],
            note="m14_smoke fixture: autoLoadTestFixture calls connectAll → Connected.",
        ),
        StateSpec(
            name="05-conn-udp-with-signal",
            description="UDP-connected fixture + temperature signal selected into chart",
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
                "--auto-select-signal",
                "udp:m14-smoke-udp/temperature",
            ],
            note="Chart line not visible under software-RHI (ADR-010); validates signal-selector + connection-list state.",
        ),
    ]


def specs_phase_b_skipped() -> list[StateSpec]:
    """Phase B: skipped — needs new MainWindow primitives + CLI flags."""
    return [
        StateSpec(
            name="01-empty-with-chart",
            description="No fixture, default chart pane (chart auto-added on launch)",
            mechanism="manual",
            note="MainWindow already creates a default chart on init; this overlaps with 00-empty-launch under current UI. Re-investigate after S4.",
        ),
        StateSpec(
            name="02-conn-udp-idle",
            description="UDP connection loaded but NOT connected",
            mechanism="manual",
            note="autoLoadTestFixture unconditionally calls connectAll. Needs a new --auto-no-connect flag OR fixture-honour autoConnectOnStartup=false. V0.3 work.",
        ),
        StateSpec(
            name="03-conn-udp-connecting",
            description="Connecting UDP — transient state",
            mechanism="manual",
            note="UDP bind is synchronous; this state may not be observable headlessly. Operator captures via real GUI.",
        ),
        StateSpec(
            name="06-conn-udp-disconnecting",
            description="Disconnecting UDP — transient state",
            mechanism="manual",
            note="Transient; needs operator-driven disconnect-then-capture timing.",
        ),
        StateSpec(
            name="07-conn-udp-error",
            description="UDP connection in error state",
            mechanism="manual",
            note="Needs an intentional bind failure or driver fault injection. Operator captures.",
        ),
        StateSpec(
            name="08-conn-serial-idle",
            description="Idle Serial connection",
            mechanism="manual",
            note="Serial requires hardware or virtual pty; out-of-scope without socat fixture extension.",
        ),
        StateSpec(
            name="09-conn-serial-connected",
            description="Connected Serial connection",
            mechanism="manual",
            note="Same hardware constraint as 08.",
        ),
        StateSpec(
            name="10-conn-tcp-idle",
            description="Idle TCP connection",
            mechanism="manual",
            note="Needs TCP-server fixture; not yet built.",
        ),
        StateSpec(
            name="11-conn-tcp-connected",
            description="Connected TCP connection",
            mechanism="manual",
            note="Needs TCP-server fixture coordinator.",
        ),
        StateSpec(
            name="12-multi-2-drivers",
            description="2 UDP drivers, both Connected",
            mechanism="manual",
            note="Needs new fixture m15_multi_2.yaml + matching fixture sender. Operator manual or S4 work.",
        ),
        StateSpec(
            name="13-multi-5-drivers",
            description="5 UDP drivers, all Connected (signal-selector full)",
            mechanism="manual",
            note="Needs fixture + 5 sender ports. Operator manual or S4 work.",
        ),
        StateSpec(
            name="14-recording-active",
            description="Recording active (--auto-record-to + connected fixture)",
            mechanism="C",
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
                "--auto-select-signal",
                "udp:m14-smoke-udp/temperature",
                "--auto-record-to",
                "/tmp/m15-baseline-rec.sf",
            ],
            capture_after_ms=2500,
            exit_after_ms=3500,
            note="--auto-record-to fires at 700ms; capture at 2500ms shows recording-active status bar.",
        ),
        StateSpec(
            name="15-recording-stopped",
            description="Recording started + stopped before capture",
            mechanism="C",
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
                "--auto-select-signal",
                "udp:m14-smoke-udp/temperature",
                "--auto-record-to",
                "/tmp/m15-baseline-rec-stopped.sf",
                "--auto-stop-recording-after-ms",
                "1500",
            ],
            capture_after_ms=2500,
            exit_after_ms=3500,
            note="record fires 700ms; stop 1500ms; capture 2500ms — status bar shows post-stop bytes.",
        ),
        StateSpec(
            name="16-replay-open-dialog",
            description="File-open dialog modal (replay)",
            mechanism="manual",
            note="Modal QFileDialog; needs mechanism B (xvfb screen grab) + xdotool to trigger. Operator captures.",
        ),
        StateSpec(
            name="17-replay-loaded",
            description="Replay session loaded, paused at start",
            mechanism="manual",
            note="Needs --auto-replay-load <path> flag + a session-file fixture. V0.3 work.",
        ),
        StateSpec(
            name="18-replay-playing",
            description="Replay actively playing",
            mechanism="manual",
            note="Needs --auto-replay-load + --auto-replay-play flags.",
        ),
        StateSpec(
            name="19-replay-scrubber-mid",
            description="Replay paused at 50% position",
            mechanism="manual",
            note="Needs replay-load + seek flag.",
        ),
        StateSpec(
            name="20-replay-end",
            description="Replay finished, scrubber at end",
            mechanism="manual",
            note="Needs replay-load + auto-play-to-end timing.",
        ),
        StateSpec(
            name="21-replay-speed-5x",
            description="Replay speed combo open at 5×",
            mechanism="manual",
            note="Needs replay-load + speed-set flag + B mechanism for combo dropdown.",
        ),
        StateSpec(
            name="22-mode-live-to-replay",
            description="Live → Replay confirm dialog",
            mechanism="manual",
            note="Modal QMessageBox; needs B mechanism + interaction trigger.",
        ),
        StateSpec(
            name="23-mode-replay-to-live",
            description="Replay → Live 3-option dialog",
            mechanism="manual",
            note="Modal; needs B mechanism + replay-loaded prerequisite.",
        ),
        StateSpec(
            name="24-dialog-add-serial",
            description="Connection-add dialog, Serial driver type selected",
            mechanism="manual",
            note="Needs --auto-open-add-conn-dialog=serial flag + B mechanism for the modal.",
        ),
        StateSpec(
            name="25-dialog-add-udp",
            description="Connection-add dialog, UDP driver type selected",
            mechanism="manual",
            note="Same as 24 but UDP-typed.",
        ),
        StateSpec(
            name="26-dialog-edit",
            description="Connection-edit dialog",
            mechanism="manual",
            note="Needs --auto-open-edit-conn-dialog flag + existing connection.",
        ),
        StateSpec(
            name="27-dialog-quit-recording",
            description="Quit-while-recording prompt",
            mechanism="manual",
            note="Modal triggered by quit gesture during recording; needs B + closeEvent injection.",
        ),
        StateSpec(
            name="28-dialog-recording-error",
            description="Recording-error dialog",
            mechanism="manual",
            note="Triggered by SessionWriter::start failure; needs error injection + B mechanism.",
        ),
        StateSpec(
            name="29-dialog-replay-error",
            description="Replay-error dialog",
            mechanism="manual",
            note="Triggered by malformed session file; needs error injection + B mechanism.",
        ),
        StateSpec(
            name="30-menu-file-open",
            description="File menu open",
            mechanism="manual",
            note="Menu popup is a separate top-level window; mechanism C grabs the MainWindow only. Needs B (full screen) + xdotool key 'Alt+F'.",
        ),
        StateSpec(
            name="31-menu-connections-open",
            description="Connections menu open",
            mechanism="manual",
            note="Same as 30; B mechanism + xdotool.",
        ),
        StateSpec(
            name="32-menu-session-open",
            description="Session menu open",
            mechanism="manual",
            note="Same as 30; B mechanism + xdotool.",
        ),
        StateSpec(
            name="33-status-buffer-normal",
            description="Buffer < 80 % usage indicator",
            mechanism="C",
            launch_args=[
                "--auto-load-test-fixture",
                "tests/integration/gui/fixtures/m14_smoke.yaml",
                "--auto-select-signal",
                "udp:m14-smoke-udp/temperature",
            ],
            capture_after_ms=2500,
            exit_after_ms=3500,
            note="Idle UDP buffer ≈ 0 %; identical to 04 except status text. Validates the F15 buffer-budget label.",
        ),
        StateSpec(
            name="34-status-buffer-warn",
            description="Buffer ≥ 80 % usage indicator",
            mechanism="manual",
            note="Needs traffic-flooding sender + timing — operator captures or S4 builds a flood fixture.",
        ),
        StateSpec(
            name="35-status-buffer-full",
            description="Buffer FULL indicator",
            mechanism="manual",
            note="Same as 34 plus drop-overflow timing.",
        ),
        StateSpec(
            name="36-multi-chart-2",
            description="2 charts coexisting",
            mechanism="manual",
            note="Needs --auto-add-chart=2 flag (new MainWindow primitive). V0.3 work.",
        ),
        StateSpec(
            name="37-multi-chart-5",
            description="5 charts (vertical stack)",
            mechanism="manual",
            note="Same as 36 with count=5.",
        ),
    ]


def all_specs() -> list[StateSpec]:
    return specs_phase_a() + specs_phase_b_skipped()


def _ensure_clean(path: Path) -> None:
    if path.is_file():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)


def capture_state_with_stability(spec: StateSpec, runs: int = 3) -> StateReport:
    """Capture a single state ``runs`` times; verify pixel stability."""
    if spec.mechanism == "manual":
        return StateReport(spec=spec, status="SKIP", note="manual mechanism")

    if spec.mechanism != "C":
        return StateReport(spec=spec, status="SKIP", note=f"mechanism={spec.mechanism} (S4)")

    TMP_RUN_DIR.mkdir(parents=True, exist_ok=True)
    paths: list[Path] = []
    try:
        for i in range(runs):
            run_name = f"{spec.name}__run{i}"
            tmp_path = TMP_RUN_DIR / f"{run_name}.png"
            _ensure_clean(tmp_path)
            # capture_signalforge_state writes to
            # tests/screenshots/<state>.png by convention; we move
            # it after each run to a per-run path so diffs work.
            res = capture_signalforge_state(
                state_name=run_name,
                launch_args=list(spec.launch_args),
                capture_after_ms=spec.capture_after_ms,
                exit_after_ms=spec.exit_after_ms,
                timeout_s=spec.timeout_s,
                mechanism="C",
            )
            if not res.exists():
                return StateReport(spec=spec, status="ERROR",
                                   note=f"run {i}: capture file missing")
            # capture_signalforge_state writes to
            # tests/screenshots/<run_name>.png; move to tmp_path
            produced = res.actual_path
            if produced != tmp_path:
                shutil.move(produced, tmp_path)
            paths.append(tmp_path)
            time.sleep(0.2)

        diffs: list[float] = []
        for i in range(1, runs):
            cmp = compare_baseline(paths[i], paths[i - 1],
                                   max_diff_percent=0.5,
                                   channel_tolerance=4)
            diffs.append(cmp.diff_percent)

        max_diff = max(diffs) if diffs else 0.0
        stable = max_diff < 0.5

        # Promote the last (steady-state) capture to baseline-candidate
        CAND_DIR.mkdir(parents=True, exist_ok=True)
        final = CAND_DIR / f"{spec.name}.png"
        shutil.copyfile(paths[-1], final)

        if stable:
            return StateReport(spec=spec, status="PASS", diffs=diffs,
                               note=f"max-diff {max_diff:.3f}%")
        return StateReport(spec=spec, status="FLAKY", diffs=diffs,
                           note=f"max-diff {max_diff:.3f}% > 0.5%")
    except Exception as exc:  # noqa: BLE001
        return StateReport(spec=spec, status="ERROR",
                           note=f"{type(exc).__name__}: {exc}")


def main() -> int:
    filter_substr = sys.argv[1] if len(sys.argv) > 1 else ""
    specs = [s for s in all_specs() if filter_substr in s.name]
    if not specs:
        print(f"no specs matched filter {filter_substr!r}", file=sys.stderr)
        return 2

    CAND_DIR.mkdir(parents=True, exist_ok=True)

    print(f"M15 S3 — capturing {len(specs)} state(s) (filter={filter_substr!r})")
    print(f"  output dir: {CAND_DIR.relative_to(REPO_ROOT)}")
    print()

    pad = max(len(s.name) for s in specs)
    reports: list[StateReport] = []
    for spec in specs:
        sys.stdout.write(f"  {spec.name.ljust(pad)}  [{spec.mechanism:>6}]  ... ")
        sys.stdout.flush()
        rep = capture_state_with_stability(spec)
        reports.append(rep)
        print(f"{rep.status:<5}  {rep.note}")

    counts = {"PASS": 0, "FLAKY": 0, "SKIP": 0, "ERROR": 0}
    for r in reports:
        counts[r.status] += 1
    print()
    print(f"Summary: PASS={counts['PASS']} FLAKY={counts['FLAKY']}"
          f" SKIP={counts['SKIP']} ERROR={counts['ERROR']}"
          f" of {len(reports)}")
    return 0 if counts["ERROR"] == 0 and counts["FLAKY"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
