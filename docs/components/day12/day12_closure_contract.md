# Day 12 Closure Contract (Final Product)

This document freezes the final closure contract for **Epoch I, Day 12: Base Solidification** and defines mandatory runtime evidence for release confidence.

## 1. Scope

Day 12 closure covers four kernel mechanisms:

- User-mode fault isolation (thread-kill, kernel survives)
- Synchronous IPC rendezvous (endpoint capability-gated handoff)
- Resource reaper lifecycle (zombie enqueue and deterministic reap)
- Process annihilation (last-thread teardown with address-space cleanup)

## 2. Vision Alignment Contract

- Kernel remains mechanism-only:
  - it enforces fault boundaries, queue transitions, and teardown rules
  - it does not embed daemon policy
- Capability model remains the only authority path for endpoint IPC.
- Day 12 behavior remains compatible with the execution-envelope era and ABI-v2 gate path.

## 3. Security Contract

- User faults are fail-closed for offending threads and must not panic kernel.
- Endpoint invocation must reject missing/invalid capability identity.
- Reaper path must preserve state-transition legality (`RUNNING/BLOCKED -> ZOMBIE -> destroy`).
- Process annihilation must release process-owned space without destroying active kernel context.

## 4. Performance Contract

- Reaper executes bounded work per pass (`SCHED_REAP_BUDGET`).
- Rendezvous queue operations remain O(1) enqueue/dequeue at endpoint head/tail.
- No unbounded work while holding scheduler run-queue lock.

## 5. Runtime Evidence Markers

The following markers are required in matrix serial logs:

- `[TEST] Day 12 Fault Isolation: SUCCESS.`
- `[TEST] Day 12 Rendezvous Contract: SUCCESS.`
- `[TEST] Day 12 Reaper Lifecycle: SUCCESS.`
- `[TEST] Day 12 Process Annihilation: SUCCESS.`

## 6. Enforcement Points

- Fault isolation: `kernel/idt.c`
- Endpoint rendezvous: `kernel/syscall.c`
- Reaper enqueue/reap: `kernel/thread.c`, `kernel/scheduler.c`
- Annihilation: `kernel/process.c`, `kernel/vmm.c`

## 7. Exit Criteria

Day 12 is closure-ratified only when all are true:

- `make -C kernel` passes
- `make -C kernel verify_matrix` passes 3/3
- required Day 12 markers are present in matrix serial logs
- docs/checklists/version log are synchronized with exact evidence
