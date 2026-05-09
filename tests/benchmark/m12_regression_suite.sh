#!/usr/bin/env bash
# tests/benchmark/m12_regression_suite.sh
#
# M12 regression suite per spec §3.4 J / plan §3 H1.
# Reruns the M5-M11 benchmarks that have JSON-emitting tools
# in tree, captures their output, and pipes through
# `tools/profile/check_regression.py` for a 5 % threshold check.
#
# Designed to run before each M12 optimisation commit (S3 / S4).
# Exits non-zero (3) if any tracked metric regresses > 5 %.

set -euo pipefail

BUILD_DIR="${1:-build/release-bench}"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Build dir not found: ${BUILD_DIR}" >&2
    echo "Configure first: cmake -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Release -DSIGNALFORGE_BENCHMARKS=ON" >&2
    exit 64
fi

OUT="$(mktemp /tmp/m12-regression-XXXXXX.jsonl)"
echo "[m12-regression] capturing bench output to ${OUT}"

run_bench() {
    local bench="$1"
    shift
    local bin="${BUILD_DIR}/tests/benchmark/${bench}"
    if [ ! -x "${bin}" ]; then
        echo "  skipping ${bench} (not built)" >&2
        return 0
    fi
    echo "  running ${bench} $*"
    "${bin}" "$@" 2>&1 | grep '^{' >> "${OUT}" || true
}

# M11 SessionPlayer (the headline case)
run_bench bench_replay --realtime 10
run_bench bench_replay --seek-test
run_bench bench_replay --step-test

# M10 SessionWriter (default mode = 10 s soak; bench-style throughput)
# Skip the default invocation here — its 10 s wall makes the suite
# expensive. Soak-mode tests are operator-run.
# run_bench bench_session_writer

echo "[m12-regression] checking thresholds"
python3 tools/profile/check_regression.py --current "${OUT}" "${@:2}"
RC=$?
echo "[m12-regression] check_regression exit code: ${RC}"
exit "${RC}"
