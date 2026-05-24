# Day 29 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 29: Law 2 Runtime Validation + Strict Unmap Adoption**.

## 1. Scope

Day 29 closure covers:

- strict unmap adoption in Paradigm active boundary/runtime probes
- runtime validation of strict map/unmap behavior under headless matrix execution
- deterministic strict-path runtime evidence and fail-closed marker gates
- kernel-owned Day 29 attestation evidence (`[LAW2_ATTEST] day=29`)
- reason-coded strict-unmap reject coverage (`AUTH_OK`, `CTRL_DENY`, `PARENT_DENY`, `RIGHTS_DENY`, `INDEX_DENY`)
- strict-unmap performance budget evidence (bounded max latency per run)

## 2. Vision Alignment Contract

- Law 2 strict behavior remains active in default user-space flow.
- Map/unmap strict interfaces are exercised consistently, not partially.
- Day 29 behavior is closure-gated by repeatable runtime evidence.

## 3. Security Contract

- Strict unmap rejection behavior must remain fail-closed.
- Strict runtime adoption regressions emit explicit `[DAY29-FAIL]` markers and block closure.
- Boundary + strict probes must remain deterministic across repeated runs.
- Day 29 attestation must include full reject-class coverage bitmask (`LAW2_DAY29_REASON_MASK_REQUIRED`).

## 4. Performance Contract

- Day 29 checks add bounded runtime probes only.
- No unbounded loops/retry logic are introduced by Day 29 closure logic.
- Repeat-run closure suite remains deterministic across runs.
- Kernel attestation max strict-unmap latency must not exceed budget (`day29_unmap_cycles_max <= day29_perf_budget_cycles`).

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 29 Strict Unmap Adoption Contract: SUCCESS.`
- `[TEST] Day 29 Runtime Validation Contract: SUCCESS.`
- `[TEST] Day 29 Strict Path Runtime Contract: SUCCESS.`
- `[TEST] Day 29 Reason Coverage Contract: SUCCESS.`
- `[TEST] Day 29 Performance Budget Contract: SUCCESS.`
- line beginning with `[LAW2_ATTEST] day=29 result=PASS`

Forbidden markers:

- `[DAY29-FAIL]`
- any line beginning with `[LAW2_ATTEST] day=29 result=FAIL`

## 6. Enforcement Points

- strict map/unmap validation: `kernel/syscall.c`
- kernel attestation records: `kernel/mode.c` (`FATE_RECORD_ATTEST`)
- strict runtime probes + markers: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day29_closure_suite.sh`

## 7. Exit Criteria

Day 29 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 29 required markers and no Day 29 forbidden markers
- `./tools/run_day29_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
