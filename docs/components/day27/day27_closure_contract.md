# Day 27 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 27: Syscall Boundary Hardening + Law 2 Strict Foundation**.

## 1. Scope

Day 27 closure covers:

- syscall boundary rejection behavior for malformed inputs
- strict map/unmap foundation negative-path behavior
- deterministic runtime evidence and fail-closed marker gates

## 2. Vision Alignment Contract

- The user-kernel boundary remains fail-closed under malformed syscall inputs.
- Law 2 strict foundation behavior remains active and deterministic.
- Day 27 behavior is closure-gated by matrix-backed evidence.

## 3. Security Contract

- Boundary probes must confirm safe syscall rejection behavior.
- Strict negative probes must confirm strict-map foundation rejection behavior.
- Day 27 regressions emit explicit `[DAY27-FAIL]` markers and block closure.

## 4. Performance Contract

- Added Day 27 closure checks are bounded runtime probes.
- No new unbounded loops are introduced by Day 27 closure logic.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 27 Boundary Hardening Contract: SUCCESS.`
- `[TEST] Day 27 Strict Foundation Contract: SUCCESS.`
- `[TEST] Day 27 Syscall Rejection Contract: SUCCESS.`

Forbidden markers:

- `[DAY27-FAIL]`

## 6. Enforcement Points

- boundary + strict negative probes: `user/paradigm/main.c`
- strict map/unmap validation paths: `kernel/syscall.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day27_closure_suite.sh`

## 7. Exit Criteria

Day 27 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 27 required markers and no Day 27 forbidden markers
- `./tools/run_day27_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
