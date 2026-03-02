#!/usr/bin/env bash
set -euo pipefail

RUNS=5
TIMEOUT_SECS=35
ISO_PATH="kernel/reaper-os.iso"
OUT_DIR="kernel"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --runs)
            RUNS="${2:?missing value for --runs}"
            shift 2
            ;;
        --timeout)
            TIMEOUT_SECS="${2:?missing value for --timeout}"
            shift 2
            ;;
        --iso)
            ISO_PATH="${2:?missing value for --iso}"
            shift 2
            ;;
        --out-dir)
            OUT_DIR="${2:?missing value for --out-dir}"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

echo "[day23] building kernel iso"
make -C kernel iso >/dev/null

echo "[day23] running matrix with runs=${RUNS} timeout=${TIMEOUT_SECS}s"
./tools/run_law2_fate_matrix.sh --runs "${RUNS}" --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[day23] validating Day 23 markers across all runs"
for ((i = 1; i <= RUNS; i++)); do
    serial_file="${OUT_DIR}/serial_matrix_run${i}.log"
    if [[ ! -s "${serial_file}" ]]; then
        echo "ERROR: missing serial output for run ${i}: ${serial_file}" >&2
        exit 1
    fi

    if ! grep -Fq "[TEST] Day 23 Foundation Allocator Contract: SUCCESS." "${serial_file}"; then
        echo "ERROR: run ${i} missing Day 23 marker: [TEST] Day 23 Foundation Allocator Contract: SUCCESS." >&2
        exit 1
    fi

    if grep -Fq "[DAY23-FAIL]" "${serial_file}"; then
        echo "ERROR: run ${i} contains Day 23 forbidden marker: [DAY23-FAIL]" >&2
        exit 1
    fi

    echo "[day23] run ${i} PASS (${serial_file})"
done

echo "[day23] PASS: Day 23 closure suite succeeded for ${RUNS}/${RUNS} runs"
