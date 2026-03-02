# Day 24 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 24: Foundation Hardening & Ocular Projection**.

## 1. Scope

Day 24 closure covers:

- PMM baseline allocation/free invariants
- Law 9 counter progression under allocation probes
- Ocular engine readiness and bleach-path safety
- deterministic runtime evidence and fail-closed rejection markers

## 2. Vision Alignment Contract

- Foundation hardening remains concrete, testable, and release-gated.
- Ocular projection behavior remains an explicit first-class runtime contract.
- Day 24 evidence is matrix-gated and repeat-run validated.

## 3. Security Contract

- PMM baseline and post-free stats must remain internally consistent.
- Law 9 counters must advance under allocation activity.
- Ocular engine readiness is required before closure markers are emitted.
- Day 24 failures emit explicit `[DAY24-FAIL]` markers and block closure.

## 4. Performance Contract

- Day 24 checks are bounded boot-time probes.
- No new unbounded loops are introduced by Day 24 closure logic.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 24 Foundation Hardening Contract: SUCCESS.`
- `[TEST] Day 24 Ocular Projection Contract: SUCCESS.`

Forbidden markers:

- `[DAY24-FAIL]`

## 6. Enforcement Points

- PMM/Law9/Ocular closure probes: `kernel/main.c`
- Ocular runtime readiness API: `kernel/ocular.c`, `kernel/include/ocular.h`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day24_closure_suite.sh`

## 7. Exit Criteria

Day 24 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 24 required markers and no Day 24 forbidden markers
- `./tools/run_day24_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
