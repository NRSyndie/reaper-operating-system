# Day 33 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 33: Full Fault Context Capture**.

## 1. Scope

Day 33 closure covers:

- full-context fault metadata integrity in Fate fault records
- strict vector sanity for sampled fault records (`#GP/#PF`)
- bounded full-context fault-audit runtime budget
- deterministic runtime markers and explicit fail-marker gates

## 2. Vision Alignment Contract

- Fault forensics remain replay-grade, not best-effort diagnostics.
- Day 33 closure guarantees complete low-level context in user-visible forensic evidence.
- Closure is enforced by repeatable runtime gates, not narrative-only claims.

## 3. Security Contract

- Fault records in audit windows must remain type-correct (`record_type == FATE_RECORD_FAULT`).
- Full context fields must be preserved for sampled fault records (`RIP/RSP/CS/RFLAGS`; `CR2` for #PF).
- Day 33 regressions emit explicit `[DAY33-FAIL]` markers and block closure.

## 4. Performance Contract

- Full-context fault audit path must remain within bounded runtime budget:
  - `full_context_audit_cycles <= DAY33_FULL_CONTEXT_AUDIT_BUDGET_CYCLES`
- Day 33 probes remain bounded and deterministic; no unbounded retries are introduced.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 33 Full Context Coverage Contract: SUCCESS.`
- `[TEST] Day 33 Fault Vector Coverage Contract: SUCCESS.`
- `[TEST] Day 33 Full Context Performance Contract: SUCCESS.`

Forbidden markers:

- `[DAY33-FAIL]`

## 6. Enforcement Points

- fault event append/context path: `kernel/idt.c`, `kernel/mode.c`
- fault read/filter path: `kernel/syscall.c`
- Day 33 closure probes + markers: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day33_closure_suite.sh`

## 7. Exit Criteria

Day 33 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 33 required markers and no Day 33 forbidden markers
- `make -C kernel verify_day33` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
