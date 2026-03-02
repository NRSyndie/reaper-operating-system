# Day 20 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 20: Law 6 - The Lattice Bridge (Streaming)**.

## 1. Scope

Day 20 closure covers:

- lattice creation topology and slot-validation contract
- source/listener rights and attunement authority contract
- attach/detach lifecycle safety (address validity + overlap prevention)
- deterministic runtime evidence and fail-closed rejection markers

## 2. Vision Alignment Contract

- Lattice Bridge remains the zero-copy substrate for high-volume communication.
- Source/Listener role separation remains kernel-enforced, not policy-by-convention.
- Day 20 evidence is matrix-gated and repeat-run validated.

## 3. Security Contract

- Lattice create rejects malformed topology and invalid destination slots fail-closed.
- Lattice attach requires readable lattice capability and valid user virtual range.
- Lattice attach rejects unaligned addresses, overlapping windows, and duplicate `(lattice, vaddr)` binds.
- `SYS_ATTUNE` remains write-authority gated; listener attunement is rejected.
- Day 20 failures emit explicit `[DAY20-FAIL]` markers and block closure.

## 4. Performance Contract

- Added checks are constant-time topology/bit/range validation.
- No unbounded loops/retry semantics are introduced in Day 20 closure checks.
- Repeat-run closure suite must remain deterministic across runs.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 20 Lattice Create Contract: SUCCESS.`
- `[TEST] Day 20 Lattice Rights Contract: SUCCESS.`
- `[TEST] Day 20 Lattice Lifecycle Contract: SUCCESS.`

Forbidden markers:

- `[DAY20-FAIL]`

## 6. Enforcement Points

- lattice create/attach/detach/attune gate checks: `kernel/syscall.c`
- process attachment overlap/duplicate guards: `kernel/process.c`
- closure probes + marker emission: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day20_closure_suite.sh`

## 7. Exit Criteria

Day 20 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 20 required markers and no Day 20 forbidden markers
- `./tools/run_day20_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
