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

echo "[day18] building kernel iso"
make -C kernel iso >/dev/null

echo "[day18] running matrix with runs=${RUNS} timeout=${TIMEOUT_SECS}s"
./tools/run_law2_fate_matrix.sh --runs "${RUNS}" --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[day18] validating Day 18 markers across all runs"
for ((i = 1; i <= RUNS; i++)); do
    serial_file="${OUT_DIR}/serial_matrix_run${i}.log"
    if [[ ! -s "${serial_file}" ]]; then
        echo "ERROR: missing serial output for run ${i}: ${serial_file}" >&2
        exit 1
    fi

    for marker in \
        "[TEST] Day 18 ELF Header Validation: SUCCESS." \
        "[TEST] Day 18 ELF Loader Contract: SUCCESS." \
        "[TEST] Day 18 Paradigm C Daemon Bootstrap: SUCCESS."; do
        if ! grep -Fq "${marker}" "${serial_file}"; then
            echo "ERROR: run ${i} missing Day 18 marker: ${marker}" >&2
            exit 1
        fi
    done

    if grep -Fq "[DAY18-FAIL]" "${serial_file}"; then
        echo "ERROR: run ${i} contains Day 18 forbidden marker: [DAY18-FAIL]" >&2
        exit 1
    fi

    echo "[day18] run ${i} PASS (${serial_file})"
done

echo "[day18] PASS: Day 18 closure suite succeeded for ${RUNS}/${RUNS} runs"
