# Day 13 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch I, Day 13: Full Context Preservation (SSE/FPU Support)**.

## 1. Scope

Day 13 closure covers:

- CPU extended-state initialization (`FXSAVE`/`XSAVE` mode selection)
- Context-switch save/restore of extended state
- Cross-thread FPU/SSE register isolation
- Runtime stability under sustained scheduler switching

## 2. Vision Alignment Contract

- Kernel provides mechanism-only context preservation; no user-space policy is embedded.
- Context integrity guarantees remain compatible with the current scheduler and syscall-gate architecture.
- Closure evidence is deterministic and runtime-verifiable through serial markers.

## 3. Security Contract

- Cross-thread register contamination is fail-closed and must panic deterministically.
- Unsupported or invalid extended-state mode is fail-closed.
- No silent acceptance of FPU corruption is allowed in runtime tests.

## 4. Performance Contract

- Context save/restore remains bounded per switch for active mode (`FXSAVE` or `XSAVE`).
- No unbounded operations are introduced in hot scheduler context-switch paths.
- Crucible stability must hold under repeated matrix runs without Day 13 failure markers.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 13 Extended-State Init: SUCCESS.`
- `[TEST] Day 13 Context Preservation: SUCCESS.`
- `[TEST] Day 13 Cross-Thread FPU Isolation: SUCCESS.`
- `[TEST] Day 13 Crucible Stability: SUCCESS.`

Forbidden marker:

- `[DAY13-FAIL]`

## 6. Enforcement Points

- Extended-state initialization: `kernel/cpu.c`
- Save/restore implementation: `kernel/interrupts.s` (`context_switch`)
- Thread extended-state ownership/scrub lifecycle: `kernel/thread.c`
- Runtime closure markers and failure probes: `kernel/main.c`

## 7. Exit Criteria

Day 13 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes 3/3 with Day 13 markers present and `[DAY13-FAIL]` absent
- `./tools/run_day13_closure_suite.sh` passes repeat-run closure checks
- docs/version logs/checklists are synchronized with concrete command evidence
