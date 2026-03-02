# Day 17 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch I, Day 17: Safety & Stability Finalization (Guards & Locks)**.

## 1. Scope

Day 17 closure covers:

- IRQ-safe spinlock save/restore contract
- stack canary guard integrity contract
- spurious IRQ accounting/filter contract (IRQ7/IRQ15)
- deterministic runtime evidence for safety-stability invariants

## 2. Vision Alignment Contract

- Kernel hardening mechanisms stay deterministic and auditable under scheduler/runtime pressure.
- Guard and lock primitives remain foundational substrate mechanics, not policy logic.
- Day 17 evidence is matrix-gated and repeat-run validated.

## 3. Security Contract

- Interrupt-state preservation across lock acquire/release is fail-closed.
- Thread stack canary integrity violations are fail-closed.
- Spurious interrupt handling/accounting regressions are fail-closed.
- Day 17 failures emit explicit `[DAY17-FAIL]` markers and block closure.

## 4. Performance Contract

- Day 17 probes are bounded, O(1), and boot-time deterministic.
- No unbounded loops/retry semantics are introduced in closure checks.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 17 IRQ-Safe Spinlocks: SUCCESS.`
- `[TEST] Day 17 Stack Canary Guard: SUCCESS.`
- `[TEST] Day 17 Spurious IRQ Filter: SUCCESS.`

Forbidden markers:

- `[DAY17-FAIL]`

## 6. Enforcement Points

- closure probes + marker emission: `kernel/main.c`
- spinlock contract primitive: `kernel/include/utils.h`
- canary guard + scheduler check path: `kernel/thread.c`, `kernel/scheduler.c`
- spurious IRQ accounting path: `kernel/idt.c`, `kernel/interrupts.s`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day17_closure_suite.sh`

## 7. Exit Criteria

Day 17 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 17 required markers and no Day 17 forbidden markers
- `./tools/run_day17_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
