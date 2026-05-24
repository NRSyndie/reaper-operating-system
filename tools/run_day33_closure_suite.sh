#!/usr/bin/env bash
set -euo pipefail

RUNS=5
TIMEOUT_SECS=35
ISO_PATH="kernel/reaper-os.iso"
OUT_DIR="kernel"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

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

if [[ "${ISO_PATH}" != /* ]]; then
    ISO_PATH="${PWD}/${ISO_PATH}"
fi

if [[ "${OUT_DIR}" != /* ]]; then
    OUT_DIR="${PWD}/${OUT_DIR}"
fi

echo "[day33] building kernel iso"
make -C "${REPO_ROOT}/kernel" iso >/dev/null

echo "[day33] running matrix with runs=${RUNS} timeout=${TIMEOUT_SECS}s"
"${REPO_ROOT}/tools/run_law2_fate_matrix.sh" --runs "${RUNS}" --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[day33] validating Day 33 markers across all runs"
for ((i = 1; i <= RUNS; i++)); do
    serial_file="${OUT_DIR}/serial_matrix_run${i}.log"
    if [[ ! -s "${serial_file}" ]]; then
        echo "ERROR: missing serial output for run ${i}: ${serial_file}" >&2
        exit 1
    fi

    for marker in \
        "[TEST] Day 33 Full Context Coverage Contract: SUCCESS." \
        "[TEST] Day 33 Fault Vector Coverage Contract: SUCCESS." \
        "[TEST] Day 33 Full Context Performance Contract: SUCCESS."; do
        if ! grep -Fq "${marker}" "${serial_file}"; then
            echo "ERROR: run ${i} missing Day 33 marker: ${marker}" >&2
            exit 1
        fi
    done

    if grep -Fq "[DAY33-FAIL]" "${serial_file}"; then
        echo "ERROR: run ${i} contains Day 33 forbidden marker: [DAY33-FAIL]" >&2
        exit 1
    fi

    echo "[day33] run ${i} PASS (${serial_file})"
done

echo "[day33] PASS: Day 33 closure suite succeeded for ${RUNS}/${RUNS} runs"
