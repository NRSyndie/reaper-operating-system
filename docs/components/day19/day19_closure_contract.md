# Day 19 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 19: Law 4 - Conditional Runes (Reality Gating)**.

## 1. Scope

Day 19 closure covers:

- mode-mask validity contract for capability identities and mint requests
- capability visibility/invisibility behavior across mode transitions
- monotonic mode narrowing for derived capabilities
- deterministic runtime evidence and fail-closed rejection markers

## 2. Vision Alignment Contract

- Conditional Runes preserve Quantum Split semantics by binding capability usability to active Reality.
- Reality-gated authority remains mechanism-level enforcement in kernel capability paths.
- Day 19 evidence is matrix-gated and repeat-run validated.

## 3. Security Contract

- Invalid mode masks (zero mask or undefined bits) fail closed.
- `SYS_CAP_MINT` rejects malformed mode masks at syscall boundary.
- Capability core rejects invalid masks and mode-widening derivations.
- Day 19 failures emit explicit `[DAY19-FAIL]` markers and block closure.

## 4. Performance Contract

- Mode-mask validation is constant-time bitmask logic in mint/create/lookup paths.
- No unbounded loops/retry semantics are introduced in Day 19 closure checks.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 19 Mode Mask Validation: SUCCESS.`
- `[TEST] Day 19 Conditional Runes: SUCCESS.`
- `[TEST] Day 19 Mint Monotonicity: SUCCESS.`

Forbidden markers:

- `[DAY19-FAIL]`

## 6. Enforcement Points

- capability mode-mask constants: `kernel/include/capability.h`, `shared/include/capability.h`
- capability mode validation and mint policy: `kernel/capability.c`
- syscall boundary validation: `kernel/syscall.c`
- closure probes + markers: `kernel/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day19_closure_suite.sh`

## 7. Exit Criteria

Day 19 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 19 required markers and no Day 19 forbidden markers
- `./tools/run_day19_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
