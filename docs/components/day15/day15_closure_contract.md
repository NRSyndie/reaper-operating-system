# Day 15 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch I, Day 15: The Genesis Bridge (Initial Cap Injection)**.

## 1. Scope

Day 15 closure covers:

- genesis module discovery and loader handoff
- Paradigm process creation and queueing
- genesis capability and bootstrap capability injection contract
- boot info bridge mapping and user-space probe validation

## 2. Vision Alignment Contract

- Kernel supplies bootstrap mechanisms only (bridge, caps, boot metadata).
- Policy ownership remains in Paradigm/user space after bridge crossing.
- Genesis bridge evidence is deterministic and matrix-gated.

## 3. Security Contract

- Missing/broken genesis prerequisites fail closed with explicit Day 15 failure markers.
- Capability injection path must complete before process launch.
- Boot info bridge must pass user-space validation probe before closure is accepted.

## 4. Performance Contract

- Genesis path remains a bounded one-shot boot operation.
- No unbounded retries/loops introduced in genesis launch pipeline.
- Repeat-run suite confirms deterministic boot bridge behavior across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 15 Genesis Module Contract: SUCCESS.`
- `[TEST] Day 15 Genesis Capability Injection: SUCCESS.`
- `[TEST] Day 15 Bootinfo Bridge: SUCCESS.`
- `PARADIGM: Genesis bridge probe PASS.`

Forbidden markers:

- `[DAY15-FAIL]`
- `PARADIGM: Genesis bridge probe FAIL.`

## 6. Enforcement Points

- Genesis bridge implementation: `kernel/genesis.c`
- Boot info ABI contract: `shared/include/bootinfo.h`, `kernel/include/bootinfo.h`
- User-space bridge validation probe: `user/paradigm/main.c`
- Closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day15_closure_suite.sh`

## 7. Exit Criteria

Day 15 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 15 required markers and no Day 15 forbidden markers
- `./tools/run_day15_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
