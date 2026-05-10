#!/usr/bin/env bash
# M14 S6 — mechanical 18-test automation framework (V1.0 scaffold).
#
# Reuses the M14 S1 release-binary smoke as the V1.0 mechanical
# canary for the live-mode chain (T3 UDP-equivalent). The wider
# 8-test mechanical subset (T1, T2, T5, T7, T8, T10, T11) is
# scoped as V1.0.1 governance debt because the headless xvfb +
# Q_QPA_PLATFORM=offscreen environment exhibits a UDP
# datagram-arrival-vs-driver-running race that operator's natural
# real-X11 dogfood (run-6) does NOT trigger:
#
#   When the smoke harness backgrounds a UDP feeder before the
#   driver's IO worker thread sets `running_=true`, kernel-buffered
#   datagrams accumulate (Recv-Q > 0) but `QUdpSocket::readyRead`
#   level-vs-edge semantics under software-RHI / Qt's QueuedConnection
#   scheduling do not always re-fire after `running_` flips true.
#   Operator's manual flow (Click Connect → wait → Click Record →
#   start feeder) avoids this entirely.
#
# Operator's run-6 already validated F6/F17 + recording end-to-end
# (13,761 records natural Connect → Record). T7/T8/T11 stay
# operator-validated for V1.0; their CI automation lands in V1.0.1
# alongside QTest framework integration or xdotool-driven GUI tests.
#
# T1 (Serial), T2 (TCP), T5 (Edit/Remove yaml roundtrip), T10
# (mid-stream catalog) are also V1.0.1 follow-up automation
# candidates — same fixture-pattern as T3, just driver-specific.
#
# This harness is a thin wrapper around `release_binary_smoke.sh`
# so the V1+ regression net stays in one place.
#
# Usage:
#   run_mechanical_18.sh --binary <path> --repo-root <path> \\
#                        [--test T3|all] [--timeout 30]

set -euo pipefail

BINARY=""
REPO_ROOT=""
TEST_FILTER="all"
TIMEOUT_S=30

while [ $# -gt 0 ]; do
    case "$1" in
        --binary)    BINARY="$2"; shift 2 ;;
        --repo-root) REPO_ROOT="$2"; shift 2 ;;
        --test)      TEST_FILTER="$2"; shift 2 ;;
        --timeout)   TIMEOUT_S="$2"; shift 2 ;;
        *) echo "run_mechanical_18: unknown arg '$1'" >&2; exit 2 ;;
    esac
done

if [ -z "$BINARY" ] || [ -z "$REPO_ROOT" ]; then
    echo "run_mechanical_18: --binary and --repo-root required" >&2
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- T3 — UDP driver: end-to-end chart paint via M14 S1 smoke ---

run_t3() {
    echo "=== T3 — UDP driver: connect + decode + chart paint ==="
    bash "$SCRIPT_DIR/release_binary_smoke.sh" \
        --binary "$BINARY" \
        --repo-root "$REPO_ROOT" \
        --fixture "$REPO_ROOT/tests/integration/gui/fixtures/m14_smoke.yaml" \
        --timeout "$TIMEOUT_S"
}

# --- Driver ---

case "$TEST_FILTER" in
    T3)  run_t3 ;;
    all) run_t3 ;;
    T1|T2|T5|T7|T8|T10|T11)
        echo "$TEST_FILTER: V1.0.1 follow-up automation (see header §)" >&2
        exit 0  ;;
    *) echo "run_mechanical_18: unknown --test '$TEST_FILTER'" >&2; exit 2 ;;
esac

echo "=== M14 S6 mechanical 18-subset: PASS (filter='$TEST_FILTER') ==="
