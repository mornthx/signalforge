#!/usr/bin/env bash
# tools/profile/run_profile.sh
#
# M12 S1 — tiered profile-runner per `.claude/M12-concerns.md` C2.
#
#   Tier 1: perf record + perf report (preferred; flame graph friendly)
#   Tier 2: valgrind --tool=callgrind (works without root; ~10× slower)
#   Tier 3: built-in QElapsedTimer (always works; coarse)
#
# Usage:
#   tools/profile/run_profile.sh <build-dir> <scenario> [duration]
#
# Example:
#   tools/profile/run_profile.sh build/release-bench A 10
#
# Output goes to tests/benchmark/results/M12-profile-<tier>-<scenario>.<fmt>
#
# The script auto-detects the highest available tier and uses it.

set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <build-dir> <scenario> [duration]" >&2
    echo "  scenario: A | B | C" >&2
    exit 64
fi

BUILD_DIR="$1"
SCENARIO="$2"
DURATION="${3:-10}"

PROFILE_BIN="${BUILD_DIR}/tools/profile/profile_main"
if [ ! -x "${PROFILE_BIN}" ]; then
    echo "profile_main not found at ${PROFILE_BIN}" >&2
    echo "Build first: cmake --build ${BUILD_DIR} --target profile_main" >&2
    exit 65
fi

OUT_DIR="$(dirname "$(realpath "$0")")/../../tests/benchmark/results"
mkdir -p "${OUT_DIR}"

# Tier 1 — perf
if perf stat -- /bin/true > /dev/null 2>&1; then
    OUT="${OUT_DIR}/M12-profile-perf-${SCENARIO}.data"
    REPORT="${OUT_DIR}/M12-profile-perf-${SCENARIO}.txt"
    echo "[tier 1] perf record → ${OUT}"
    perf record -g -F 999 -o "${OUT}" --quiet -- "${PROFILE_BIN}" --scenario "${SCENARIO}" --duration "${DURATION}"
    echo "[tier 1] perf report → ${REPORT}"
    perf report -i "${OUT}" --stdio --no-children > "${REPORT}" 2>&1 || true
    echo "Profile data: ${OUT}"
    echo "Hot-function table: ${REPORT}"
    exit 0
fi

# Tier 2 — valgrind callgrind
if command -v valgrind > /dev/null 2>&1; then
    OUT="${OUT_DIR}/M12-profile-callgrind-${SCENARIO}.out"
    echo "[tier 2] valgrind callgrind → ${OUT}"
    valgrind --tool=callgrind --callgrind-out-file="${OUT}" \
        "${PROFILE_BIN}" --scenario "${SCENARIO}" --duration "${DURATION}"
    if command -v callgrind_annotate > /dev/null 2>&1; then
        REPORT="${OUT_DIR}/M12-profile-callgrind-${SCENARIO}.txt"
        callgrind_annotate "${OUT}" > "${REPORT}" 2>&1 || true
        echo "Hot-function table: ${REPORT}"
    fi
    echo "Profile data: ${OUT}"
    exit 0
fi

# Tier 3 — built-in QElapsedTimer
echo "[tier 3] QElapsedTimer fallback (no system profiler available)"
"${PROFILE_BIN}" --scenario "${SCENARIO}" --duration "${DURATION}" --tier3 \
    | tee "${OUT_DIR}/M12-profile-tier3-${SCENARIO}.json"
exit 0
