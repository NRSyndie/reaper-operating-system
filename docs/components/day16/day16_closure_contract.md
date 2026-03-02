# Day 16 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch I, Day 16: Authority over Memory (Map/Unmap Syscalls)**.

## 1. Scope

Day 16 closure covers:

- capability-scoped map authority over user address spaces
- strict rights enforcement for map parameters
- explicit unmap/remap lifecycle contract
- deterministic runtime evidence for map-path correctness

## 2. Vision Alignment Contract

- Kernel remains mechanism-only (`SYS_MAP`/`SYS_UNMAP`) while policy stays in user space.
- Mapping authority is explicit via capabilities and never ambient.
- Day 16 behavior is closure-gated by deterministic runtime markers.

## 3. Security Contract

- Invalid strict map inputs are rejected fail-closed.
- Duplicate/unowned unmap operations are rejected fail-closed.
- Day 16 failures emit explicit `[DAY16-FAIL]` markers and block closure.

## 4. Performance Contract

- Day 16 checks add bounded map/unmap/remap probes only.
- No unbounded loops/retry logic introduced in map/unmap contract path.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 16 Capability-Scoped Mapping: SUCCESS.`
- `[TEST] Day 16 Strict Rights Enforcement: SUCCESS.`
- `[TEST] Day 16 Unmap/Remap Contract: SUCCESS.`

Forbidden markers:

- `[DAY16-FAIL]`

## 6. Enforcement Points

- map/unmap syscall enforcement: `kernel/syscall.c`
- runtime Day 16 probes and marker emission: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day16_closure_suite.sh`

## 7. Exit Criteria

Day 16 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 16 required markers and no Day 16 forbidden markers
- `./tools/run_day16_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
