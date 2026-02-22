#!/usr/bin/env bash
set -euo pipefail

RUNS=3
TIMEOUT_SECS=35
ISO_PATH="kernel/reaper-os.iso"
OUT_DIR="kernel"
QEMU_BIN="qemu-system-x86_64"

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

if ! command -v "${QEMU_BIN}" >/dev/null 2>&1; then
    echo "ERROR: ${QEMU_BIN} not found in PATH." >&2
    exit 1
fi

mkdir -p "${OUT_DIR}"

required_markers=(
    "PARADIGM: Boundary probes passed (safe failures confirmed)."
    "PARADIGM: Shadow Mapping SUCCESS. The Architect is pleased."
    "PARADIGM: Hash Chain Integrity VERIFIED."
    "PARADIGM: Lattice Attunement SUCCESS."
    "PARADIGM: Real fault probe captured in Fate Strings."
    "[TEST] Day 9 Void Gate redesign: SUCCESS."
    "[TEST] Syscall Gate ABI v2: SUCCESS."
    "[TEST] Syscall Gate validation invariants: SUCCESS."
    "[TEST] Syscall Gate security probes: SUCCESS."
    "[TEST] Syscall Gate SMP isolation: SUCCESS."
    "[TEST] Syscall Gate performance budget: SUCCESS."
    "[TEST] No authority -> no execution"
    "[TEST] Root ceiling enforced"
    "[TEST] Thread explosion prevented"
    "[TEST] Revocation immediate dequeue"
    "[TEST] Cross-mode scheduling rejected"
    "[TEST] Deterministic RR rotation stable"
    "[TEST] SMP atomic budget integrity"
)

forbidden_markers=(
    "PARADIGM: Failed to read Fate Strings."
    "PARADIGM: Fault ledger empty after real fault probe."
)

echo "[matrix] runs=${RUNS} timeout=${TIMEOUT_SECS}s iso=${ISO_PATH}"

for ((i = 1; i <= RUNS; i++)); do
    serial_file="${OUT_DIR}/serial_matrix_run${i}.log"
    qemu_stdout="${OUT_DIR}/qemu_matrix_run${i}.out"
    rm -f "${serial_file}" "${qemu_stdout}"

    echo "[matrix] run ${i}/${RUNS}"
    timeout "${TIMEOUT_SECS}s" "${QEMU_BIN}" \
        -cpu Skylake-Client,+pcid,+invpcid,-x2apic,-tsc-deadline,-hle,-rtm,-xsavec \
        -m 512 \
        -cdrom "${ISO_PATH}" \
        -no-shutdown \
        -display none \
        -serial "file:${serial_file}" \
        >"${qemu_stdout}" 2>&1 || true

    if [[ ! -s "${serial_file}" ]]; then
        echo "ERROR: missing serial output for run ${i}: ${serial_file}" >&2
        exit 1
    fi

    for marker in "${required_markers[@]}"; do
        if ! grep -Fq "${marker}" "${serial_file}"; then
            echo "ERROR: run ${i} missing required marker: ${marker}" >&2
            exit 1
        fi
    done

    for marker in "${forbidden_markers[@]}"; do
        if grep -Fq "${marker}" "${serial_file}"; then
            echo "ERROR: run ${i} contains forbidden marker: ${marker}" >&2
            exit 1
        fi
    done

    echo "[matrix] run ${i} PASS (${serial_file})"
done

echo "[matrix] PASS: all ${RUNS} runs met required markers and avoided forbidden markers."
