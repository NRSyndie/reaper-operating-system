# Day 21 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 21: Fatal Forensics (Auditor Path)**.

## 1. Scope

Day 21 closure covers:

- auditor capability access control contract for Fate ledger reads
- fate-chain integrity validation contract in Paradigm
- fault-forensics record completeness contract
- deterministic runtime evidence and fail-closed rejection markers

## 2. Vision Alignment Contract

- Fate Strings remain the immutable, user-verifiable forensic ledger.
- Auditor access remains explicit capability authority, never ambient privilege.
- Day 21 evidence is matrix-gated and repeat-run validated.

## 3. Security Contract

- `SYS_FATE_READ` requires `CAP_TYPE_AUDITOR` with `CAP_RIGHT_READ`.
- Non-auditor capability Fate reads fail closed.
- Invalid Fate read filters fail closed.
- Kernel copy-out count mismatch fails closed.
- Day 21 failures emit explicit `[DAY21-FAIL]` markers and block closure.

## 4. Performance Contract

- Day 21 checks are bounded by fixed count caps and constant-time capability/type/right checks.
- No unbounded loops/retry semantics are introduced in Day 21 closure checks.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 21 Auditor Access Contract: SUCCESS.`
- `[TEST] Day 21 Fate Integrity Contract: SUCCESS.`
- `[TEST] Day 21 Fault Forensics Contract: SUCCESS.`

Forbidden markers:

- `[DAY21-FAIL]`

## 6. Enforcement Points

- fate-read authority + copy bound checks: `kernel/syscall.c`
- auditor probes + hash/fault verification markers: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day21_closure_suite.sh`

## 7. Exit Criteria

Day 21 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 21 required markers and no Day 21 forbidden markers
- `./tools/run_day21_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
