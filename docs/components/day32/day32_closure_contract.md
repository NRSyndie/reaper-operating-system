# Day 32 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 32: Fault-to-String Integration**.

## 1. Scope

Day 32 closure covers:

- fault-only Fate read filtering correctness (`FATE_READ_FAULTS`)
- fault metadata integrity checks for #GP/#PF forensic records
- bounded Day 32 fault-read runtime budget validation
- deterministic runtime markers and explicit fail-marker gates

## 2. Vision Alignment Contract

- Fault events are first-class Fate evidence, not transient diagnostics.
- User-space forensic readers can deterministically isolate fault records without cross-type leakage.
- Day 32 behavior is closure-gated by repeatable runtime evidence.

## 3. Security Contract

- Fault filter paths must not leak non-fault record types.
- Fault records must preserve minimum forensic context (`RIP/RSP/CS/RFLAGS`, and `CR2` for #PF).
- Day 32 regressions emit explicit `[DAY32-FAIL]` markers and block closure.

## 4. Performance Contract

- Fault-only read path must remain within bounded runtime budget:
  - `fault_read_cycles <= DAY32_FAULT_READ_BUDGET_CYCLES`
- Day 32 probes remain bounded and deterministic; no unbounded retry loops are introduced.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 32 Fault Filter Contract: SUCCESS.`
- `[TEST] Day 32 Fault Metadata Contract: SUCCESS.`
- `[TEST] Day 32 Fault Read Performance Contract: SUCCESS.`

Forbidden markers:

- `[DAY32-FAIL]`

## 6. Enforcement Points

- fault event append/filter path: `kernel/mode.c`, `kernel/idt.c`, `kernel/syscall.c`
- runtime Day 32 probes + markers: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day32_closure_suite.sh`

## 7. Exit Criteria

Day 32 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 32 required markers and no Day 32 forbidden markers
- `make -C kernel verify_day32` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
