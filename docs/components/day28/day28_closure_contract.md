# Day 28 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 28: Law 2 Strict Adoption Pass (Paradigm Migration)**.

## 1. Scope

Day 28 closure covers:

- strict-map adoption in Paradigm's active Shadow Mapping flow
- deterministic strict negative-path enforcement checks
- strict-chain lifecycle correctness (link, access, unmap/remap)
- deterministic runtime evidence and fail-closed marker gates

## 2. Vision Alignment Contract

- Law 2 strict behavior is no longer "available only"; it is actively exercised in default user-space flow.
- Mapping authority remains explicit and capability-scoped with no ambient privileges.
- Day 28 behavior is closure-gated by matrix-backed runtime evidence.

## 3. Security Contract

- Strict negative probes must reject malformed map requests fail-closed.
- Strict chain operations must reject unsafe states and preserve deterministic behavior.
- Day 28 regressions emit explicit `[DAY28-FAIL]` markers and block closure.

## 4. Performance Contract

- Day 28 checks remain bounded probes over strict map/unmap paths.
- No unbounded loops/retry logic are introduced by Day 28 closure logic.
- Repeat-run closure suite remains deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 28 Strict Adoption Contract: SUCCESS.`
- `[TEST] Day 28 Strict Negative Path Contract: SUCCESS.`
- `[TEST] Day 28 Strict Chain Contract: SUCCESS.`

Forbidden markers:

- `[DAY28-FAIL]`

## 6. Enforcement Points

- strict map/unmap validation: `kernel/syscall.c`
- strict adoption probes + markers: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day28_closure_suite.sh`

## 7. Exit Criteria

Day 28 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 28 required markers and no Day 28 forbidden markers
- `./tools/run_day28_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
