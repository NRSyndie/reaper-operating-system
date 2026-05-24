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

echo "[day31] building kernel iso"
make -C "${REPO_ROOT}/kernel" iso >/dev/null

echo "[day31] running matrix with runs=${RUNS} timeout=${TIMEOUT_SECS}s"
"${REPO_ROOT}/tools/run_law2_fate_matrix.sh" --runs "${RUNS}" --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[day31] validating Day 31 markers across all runs"
for ((i = 1; i <= RUNS; i++)); do
    serial_file="${OUT_DIR}/serial_matrix_run${i}.log"
    if [[ ! -s "${serial_file}" ]]; then
        echo "ERROR: missing serial output for run ${i}: ${serial_file}" >&2
        exit 1
    fi

    for marker in \
        "[TEST] Day 31 Revalidation Security Contract: SUCCESS." \
        "[TEST] Day 31 Revalidation Determinism Contract: SUCCESS." \
        "[TEST] Day 31 Revalidation Performance Contract: SUCCESS."; do
        if ! grep -Fq "${marker}" "${serial_file}"; then
            echo "ERROR: run ${i} missing Day 31 marker: ${marker}" >&2
            exit 1
        fi
    done

    if ! grep -Eq '^\[LAW2_ATTEST\] day=30 result=PASS' "${serial_file}"; then
        echo "ERROR: run ${i} missing Day 30 kernel attestation PASS marker" >&2
        exit 1
    fi
    if grep -Eq '^\[LAW2_ATTEST\] day=30 result=FAIL' "${serial_file}"; then
        echo "ERROR: run ${i} contains Day 30 kernel attestation FAIL marker" >&2
        exit 1
    fi

    if grep -Fq "[DAY31-FAIL]" "${serial_file}"; then
        echo "ERROR: run ${i} contains Day 31 forbidden marker: [DAY31-FAIL]" >&2
        exit 1
    fi

    echo "[day31] run ${i} PASS (${serial_file})"
done

echo "[day31] PASS: Day 31 closure suite succeeded for ${RUNS}/${RUNS} runs"
