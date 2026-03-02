# Day 22 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 22: Law 1 - Derivation Trees (The Snap)**.

## 1. Scope

Day 22 closure covers:

- capability lineage model correctness (`parent/child/sibling` graph)
- recursive revocation contract for full descendant annihilation
- deep-derivation chain invalidation guarantees
- deterministic runtime evidence and fail-closed rejection markers

## 2. Vision Alignment Contract

- Authority remains explicit, hierarchical, and capability-bounded.
- Delegation is revocable at lineage root without residual authority.
- Day 22 evidence is matrix-gated and repeat-run validated.

## 3. Security Contract

- Revoking a capability invalidates all descendants in the lineage tree.
- Descendants must fail closed after ancestor revocation.
- Recursive revocation and deep-derivation checks must emit deterministic pass/fail markers.
- Day 22 failures emit explicit `[DAY22-FAIL]` markers and block closure.

## 4. Performance Contract

- Revocation traversal is bounded to the capability subtree under test.
- Day 22 closure checks do not introduce unbounded retry paths.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 22 Recursive Revocation Contract: SUCCESS.`
- `[TEST] Day 22 Deep Derivation Contract: SUCCESS.`

Forbidden markers:

- `[DAY22-FAIL]`

## 6. Enforcement Points

- lineage/revocation implementation: `kernel/capability.c`
- recursive/deep derivation runtime probes: `kernel/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day22_closure_suite.sh`

## 7. Exit Criteria

Day 22 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 22 required markers and no Day 22 forbidden markers
- `./tools/run_day22_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
