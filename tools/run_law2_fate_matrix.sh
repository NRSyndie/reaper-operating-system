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
    "PARADIGM: Fate Strings include transition reject reason codes."
    "PARADIGM: Lattice Attunement SUCCESS."
    "PARADIGM: Real fault probe captured in Fate Strings."
    "PARADIGM: Envelope transition acceptance probe PASS."
    "PARADIGM: Envelope transition rejection probe PASS."
    "PARADIGM: Lifecycle gate probe PASS."
    "PARADIGM: Genesis bridge probe PASS."
    "[TEST] Day 9 Void Gate redesign: SUCCESS."
    "[TEST] Day 12 Fault Isolation: SUCCESS."
    "[TEST] Day 12 Rendezvous Contract: SUCCESS."
    "[TEST] Day 12 Reaper Lifecycle: SUCCESS."
    "[TEST] Day 12 Process Annihilation: SUCCESS."
    "[TEST] Day 13 Extended-State Init: SUCCESS."
    "[TEST] Day 13 Context Preservation: SUCCESS."
    "[TEST] Day 13 Cross-Thread FPU Isolation: SUCCESS."
    "[TEST] Day 13 Crucible Stability: SUCCESS."
    "[TEST] Day 14 Wait Contract: SUCCESS."
    "[TEST] Day 14 Yield Gate: SUCCESS."
    "[TEST] Day 14 Lifecycle ABI Surface: SUCCESS."
    "[TEST] Day 15 Genesis Module Contract: SUCCESS."
    "[TEST] Day 15 Genesis Capability Injection: SUCCESS."
    "[TEST] Day 15 Bootinfo Bridge: SUCCESS."
    "[TEST] Day 16 Capability-Scoped Mapping: SUCCESS."
    "[TEST] Day 16 Strict Rights Enforcement: SUCCESS."
    "[TEST] Day 16 Unmap/Remap Contract: SUCCESS."
    "[TEST] Day 17 IRQ-Safe Spinlocks: SUCCESS."
    "[TEST] Day 17 Stack Canary Guard: SUCCESS."
    "[TEST] Day 17 Spurious IRQ Filter: SUCCESS."
    "[TEST] Day 18 ELF Header Validation: SUCCESS."
    "[TEST] Day 18 ELF Loader Contract: SUCCESS."
    "[TEST] Day 18 Paradigm C Daemon Bootstrap: SUCCESS."
    "[TEST] Day 19 Mode Mask Validation: SUCCESS."
    "[TEST] Day 19 Conditional Runes: SUCCESS."
    "[TEST] Day 19 Mint Monotonicity: SUCCESS."
    "[TEST] Day 20 Lattice Create Contract: SUCCESS."
    "[TEST] Day 20 Lattice Rights Contract: SUCCESS."
    "[TEST] Day 20 Lattice Lifecycle Contract: SUCCESS."
    "[TEST] Day 21 Auditor Access Contract: SUCCESS."
    "[TEST] Day 21 Fate Integrity Contract: SUCCESS."
    "[TEST] Day 21 Fault Forensics Contract: SUCCESS."
    "[TEST] Day 22 Recursive Revocation Contract: SUCCESS."
    "[TEST] Day 22 Deep Derivation Contract: SUCCESS."
    "[TEST] Day 23 Foundation Allocator Contract: SUCCESS."
    "[TEST] Day 24 Foundation Hardening Contract: SUCCESS."
    "[TEST] Day 24 Ocular Projection Contract: SUCCESS."
    "[TEST] Day 25 PCID Partition Contract: SUCCESS."
    "[TEST] Day 25 TLB Scrub Contract: SUCCESS."
    "[TEST] Day 25 Secure Context Contract: SUCCESS."
    "[TEST] Day 26 Prismatic Substrate Contract: SUCCESS."
    "[TEST] Day 26 Void Wall Contract: SUCCESS."
    "[TEST] Day 26 Attunement Contract: SUCCESS."
    "[TEST] Day 27 Boundary Hardening Contract: SUCCESS."
    "[TEST] Day 27 Strict Foundation Contract: SUCCESS."
    "[TEST] Day 27 Syscall Rejection Contract: SUCCESS."
    "[TEST] Day 28 Strict Adoption Contract: SUCCESS."
    "[TEST] Day 28 Strict Negative Path Contract: SUCCESS."
    "[TEST] Day 28 Strict Chain Contract: SUCCESS."
    "[TEST] Day 29 Strict Unmap Adoption Contract: SUCCESS."
    "[TEST] Day 29 Runtime Validation Contract: SUCCESS."
    "[TEST] Day 29 Strict Path Runtime Contract: SUCCESS."
    "[TEST] Day 29 Reason Coverage Contract: SUCCESS."
    "[TEST] Day 29 Performance Budget Contract: SUCCESS."
    "[TEST] Day 30 Rejection Auditing Contract: SUCCESS."
    "[TEST] Day 30 Fate Result-Code Contract: SUCCESS."
    "[TEST] Day 30 Rejected Evidence Contract: SUCCESS."
    "[TEST] Day 30 Reason Coverage Contract: SUCCESS."
    "[TEST] Day 30 Performance Budget Contract: SUCCESS."
    "[TEST] Day 31 Revalidation Security Contract: SUCCESS."
    "[TEST] Day 31 Revalidation Determinism Contract: SUCCESS."
    "[TEST] Day 31 Revalidation Performance Contract: SUCCESS."
    "[TEST] Day 32 Fault Filter Contract: SUCCESS."
    "[TEST] Day 32 Fault Metadata Contract: SUCCESS."
    "[TEST] Day 32 Fault Read Performance Contract: SUCCESS."
    "[TEST] Day 33 Full Context Coverage Contract: SUCCESS."
    "[TEST] Day 33 Fault Vector Coverage Contract: SUCCESS."
    "[TEST] Day 33 Full Context Performance Contract: SUCCESS."
    "[TEST] Day 34 Real Fault Path Contract: SUCCESS."
    "[TEST] Day 34 User Fault Provenance Contract: SUCCESS."
    "[TEST] Day 34 Real Fault Performance Contract: SUCCESS."
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
    "[TEST] ESAK IPI profile: BSP_ONLY"
    "[MODE_LEGACY_SHIM]"
    "[ENV_COMPILE]"
    "[ENV_VERIFY]"
    "[ENV_APPLY]"
    "[ENV_ATTEST]"
    "[ENTRY_COMPILE]"
    "[ENTRY_VERIFY]"
    "[ENTRY_APPLY]"
    "[ENTRY_ATTEST]"
)

forbidden_markers=(
    "PARADIGM: Failed to read Fate Strings."
    "PARADIGM: Fault ledger empty after real fault probe."
    "PARADIGM: Envelope transition probes FAILED."
    "PARADIGM: Fate Strings missing transition reject reason codes."
    "PARADIGM: Lifecycle gate probe FAIL."
    "PARADIGM: Genesis bridge probe FAIL."
    "[ENV_ROLLBACK]"
    "[DAY13-FAIL]"
    "[DAY14-FAIL]"
    "[DAY15-FAIL]"
    "[DAY16-FAIL]"
    "[DAY17-FAIL]"
    "[DAY18-FAIL]"
    "[DAY19-FAIL]"
    "[DAY20-FAIL]"
    "[DAY21-FAIL]"
    "[DAY22-FAIL]"
    "[DAY23-FAIL]"
    "[DAY24-FAIL]"
    "[DAY25-FAIL]"
    "[DAY26-FAIL]"
    "[DAY27-FAIL]"
    "[DAY28-FAIL]"
    "[DAY29-FAIL]"
    "[DAY30-FAIL]"
    "[DAY31-FAIL]"
    "[DAY32-FAIL]"
    "[DAY33-FAIL]"
    "[DAY34-FAIL]"
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

    for marker_regex in \
        '^\[LAW2_ATTEST\] day=28 result=PASS' \
        '^\[LAW2_ATTEST\] day=29 result=PASS' \
        '^\[LAW2_ATTEST\] day=30 result=PASS'; do
        if ! grep -Eq "${marker_regex}" "${serial_file}"; then
            echo "ERROR: run ${i} missing required kernel attestation marker: ${marker_regex}" >&2
            exit 1
        fi
    done

    for marker in "${forbidden_markers[@]}"; do
        if grep -Fq "${marker}" "${serial_file}"; then
            echo "ERROR: run ${i} contains forbidden marker: ${marker}" >&2
            exit 1
        fi
    done

    for marker_regex in \
        '^\[LAW2_ATTEST\] day=28 result=FAIL' \
        '^\[LAW2_ATTEST\] day=29 result=FAIL' \
        '^\[LAW2_ATTEST\] day=30 result=FAIL'; do
        if grep -Eq "${marker_regex}" "${serial_file}"; then
            echo "ERROR: run ${i} contains forbidden kernel attestation marker: ${marker_regex}" >&2
            exit 1
        fi
    done

    echo "[matrix] run ${i} PASS (${serial_file})"
done

echo "[matrix] PASS: all ${RUNS} runs met required markers and avoided forbidden markers."
