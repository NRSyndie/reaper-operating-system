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

echo "[day34] building kernel iso"
make -C "${REPO_ROOT}/kernel" iso >/dev/null

echo "[day34] running matrix with runs=${RUNS} timeout=${TIMEOUT_SECS}s"
"${REPO_ROOT}/tools/run_law2_fate_matrix.sh" --runs "${RUNS}" --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[day34] validating Day 34 markers across all runs"
for ((i = 1; i <= RUNS; i++)); do
    serial_file="${OUT_DIR}/serial_matrix_run${i}.log"
    if [[ ! -s "${serial_file}" ]]; then
        echo "ERROR: missing serial output for run ${i}: ${serial_file}" >&2
        exit 1
    fi

    for marker in \
        "[TEST] Day 34 Real Fault Path Contract: SUCCESS." \
        "[TEST] Day 34 User Fault Provenance Contract: SUCCESS." \
        "[TEST] Day 34 Real Fault Performance Contract: SUCCESS."; do
        if ! grep -Fq "${marker}" "${serial_file}"; then
            echo "ERROR: run ${i} missing Day 34 marker: ${marker}" >&2
            exit 1
        fi
    done

    if grep -Fq "[DAY34-FAIL]" "${serial_file}"; then
        echo "ERROR: run ${i} contains Day 34 forbidden marker: [DAY34-FAIL]" >&2
        exit 1
    fi

    echo "[day34] run ${i} PASS (${serial_file})"
done

echo "[day34] PASS: Day 34 closure suite succeeded for ${RUNS}/${RUNS} runs"
