# Day 31 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 31: Fate Strings Revalidation Pass**.

## 1. Scope

Day 31 closure covers:

- deterministic revalidation of kernel-owned Day 28/29/30 attestation status
- repeat-call parity for Day 29/30 reason masks and performance budgets
- bounded drift validation for Day 29 strict-unmap latency and Day 30 reject-scan latency
- explicit fail markers for Day 31 revalidation regressions

## 2. Vision Alignment Contract

- Day 31 is no longer a one-off rerun; it is an explicit runtime contract.
- Revalidation proves audit continuity and closure stability across immediate attest snapshots.
- Closure remains evidence-first and deterministic under repeated matrix boots.

## 3. Security Contract

- Day 28/29/30 attestation status must remain PASS across both Day 31 snapshots.
- Day 29/30 required reason masks must remain covered in both snapshots.
- Day 31 regressions emit explicit `[DAY31-FAIL]` markers and block closure.

## 4. Performance Contract

- Day 29/30 per-snapshot metrics must remain within their kernel budgets.
- Inter-snapshot metric drift must remain bounded:
  - `abs(day29_unmap_cycles_max[0]-day29_unmap_cycles_max[1]) <= DAY31_DAY29_DRIFT_BUDGET_CYCLES`
  - `abs(day30_reject_scan_cycles[0]-day30_reject_scan_cycles[1]) <= DAY31_DAY30_DRIFT_BUDGET_CYCLES`
- Day 31 checks remain bounded and deterministic; no unbounded retries are introduced.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 31 Revalidation Security Contract: SUCCESS.`
- `[TEST] Day 31 Revalidation Determinism Contract: SUCCESS.`
- `[TEST] Day 31 Revalidation Performance Contract: SUCCESS.`
- line beginning with `[LAW2_ATTEST] day=30 result=PASS`

Forbidden markers:

- `[DAY31-FAIL]`
- any line beginning with `[LAW2_ATTEST] day=30 result=FAIL`

## 6. Enforcement Points

- Day 31 revalidation probes + markers: `user/paradigm/main.c`
- kernel-owned attestation evidence source: `kernel/syscall.c`, `kernel/mode.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day31_closure_suite.sh`

## 7. Exit Criteria

Day 31 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 31 required markers and no Day 31 forbidden markers
- `make -C kernel verify_day31` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
