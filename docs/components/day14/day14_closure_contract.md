# Day 14 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch I, Day 14: Lifecycle Syscalls (Yield, Exit, Wait)**.

## 1. Scope

Day 14 closure covers:

- `SYS_YIELD` scheduler handoff behavior
- `SYS_EXIT` thread termination path (`thread_exit` -> zombie queue -> reap)
- `SYS_WAIT` same-process wait contract (`flags` handling, non-blocking status, waiter ownership)
- Lifecycle syscall ABI stability via gate operations (`GATE_OP_YIELD`, `GATE_OP_EXIT`, `GATE_OP_WAIT`)

## 2. Vision Alignment Contract

- Lifecycle operations remain mechanism-level kernel primitives.
- Policy-rich process orchestration remains in user space (Paradigm and future daemons).
- Runtime evidence is deterministic and matrix-gated.

## 3. Security Contract

- Invalid wait flags fail closed (`-1`).
- Unauthorized multi-waiter conflicts fail closed.
- Exit path enforces scheduler-owned state transitions and cleanup ownership boundaries.
- Lifecycle corruption signals must be explicit via Day 14 failure markers.

## 4. Performance Contract

- Yield path remains O(1)-class scheduler handoff trigger.
- Wait path avoids unbounded work in no-event baseline and non-blocking mode.
- No additional unbounded work introduced in lifecycle syscall hot paths.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 14 Wait Contract: SUCCESS.`
- `[TEST] Day 14 Yield Gate: SUCCESS.`
- `[TEST] Day 14 Lifecycle ABI Surface: SUCCESS.`
- `PARADIGM: Lifecycle gate probe PASS.`

Forbidden markers:

- `[DAY14-FAIL]`
- `PARADIGM: Lifecycle gate probe FAIL.`

## 6. Enforcement Points

- Lifecycle syscall dispatch: `kernel/syscall.c`
- Exit/yield thread primitives: `kernel/thread.c`
- Wait ownership/event contract: `kernel/syscall.c`, `kernel/process.h`
- Closure marker checks: `kernel/main.c`, `user/paradigm/main.c`

## 7. Exit Criteria

Day 14 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 14 markers and no forbidden Day 14 markers
- `./tools/run_day14_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete test evidence
