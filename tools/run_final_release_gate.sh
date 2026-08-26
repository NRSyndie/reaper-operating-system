#!/usr/bin/env bash
set -euo pipefail

LOCK_RUNS=5
TIMEOUT_SECS=45
ISO_PATH="kernel/reaper-os.iso"
OUT_DIR="kernel"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --lock-runs)
            LOCK_RUNS="${2:?missing value for --lock-runs}"
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

echo "[final-gate] build gates"
make -C "${REPO_ROOT}/user"
make -C "${REPO_ROOT}/kernel"
make -C "${REPO_ROOT}/kernel" iso

echo "[final-gate] deterministic runtime baseline"
"${REPO_ROOT}/tools/run_law2_fate_matrix.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[final-gate] closure suites (security-critical track)"
"${REPO_ROOT}/tools/run_day28_closure_suite.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"
"${REPO_ROOT}/tools/run_day29_closure_suite.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"
"${REPO_ROOT}/tools/run_day30_closure_suite.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"
"${REPO_ROOT}/tools/run_day31_closure_suite.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"
"${REPO_ROOT}/tools/run_day32_closure_suite.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"
"${REPO_ROOT}/tools/run_day33_closure_suite.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"
"${REPO_ROOT}/tools/run_day34_closure_suite.sh" --runs 3 --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[final-gate] release lock (${LOCK_RUNS} consecutive matrix runs)"
"${REPO_ROOT}/tools/run_law2_fate_matrix.sh" --runs "${LOCK_RUNS}" --timeout "${TIMEOUT_SECS}" --iso "${ISO_PATH}" --out-dir "${OUT_DIR}"

echo "[final-gate] docs synchronization checks"
for doc_file in \
    "${REPO_ROOT}/docs/components/final_product/reaper_product_contract_v1.md" \
    "${REPO_ROOT}/docs/development_log/final_redesign_strategy.md" \
    "${REPO_ROOT}/docs/development_log/final_redesign_checklist.md"; do
    if [[ ! -s "${doc_file}" ]]; then
        echo "ERROR: required redesign artifact missing or empty: ${doc_file}" >&2
        exit 1
    fi
done

if ! grep -Fq "Final Redesign One-Way Gate" "${REPO_ROOT}/docs/development_log/release_checklist.md"; then
    echo "ERROR: release checklist missing one-way gate section." >&2
    exit 1
fi

echo "[final-gate] PASS: all build/runtime/closure/release-lock/documentation checks succeeded"
