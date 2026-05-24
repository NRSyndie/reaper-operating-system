# Day 34 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 34: Real Fault Probe Validation**.

## 1. Scope

Day 34 closure covers:

- deterministic real-path #PF evidence from lattice first-touch behavior
- user-fault provenance enforcement for the sampled real-fault record
- bounded real-fault audit runtime budget validation
- explicit fail-marker gates for Day 34 regressions

## 2. Vision Alignment Contract

- Real exception-path evidence is authoritative; synthetic fault injection is not used for closure.
- User-visible Fate forensics must prove real, recoverable fault capture on the runtime path.
- Day 34 closure remains repeatable under matrix and repeat-run suite execution.

## 3. Security Contract

- Real fault probe record must exist as a fault record with user provenance.
- Real fault record must remain within expected lattice address window and include full context fields.
- Day 34 regressions emit explicit `[DAY34-FAIL]` markers and block closure.

## 4. Performance Contract

- Real-fault audit path must remain within bounded runtime budget:
  - `real_fault_audit_cycles <= DAY34_REAL_FAULT_AUDIT_BUDGET_CYCLES`
- Day 34 probes remain bounded and deterministic; no unbounded retries are introduced.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 34 Real Fault Path Contract: SUCCESS.`
- `[TEST] Day 34 User Fault Provenance Contract: SUCCESS.`
- `[TEST] Day 34 Real Fault Performance Contract: SUCCESS.`

Forbidden markers:

- `[DAY34-FAIL]`

## 6. Enforcement Points

- real fault capture path: `kernel/idt.c`, `kernel/lattice.c`, `kernel/mode.c`
- runtime Day 34 probes + markers: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day34_closure_suite.sh`

## 7. Exit Criteria

Day 34 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 34 required markers and no Day 34 forbidden markers
- `make -C kernel verify_day34` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
