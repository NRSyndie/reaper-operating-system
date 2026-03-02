# Day 25 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 25: Law 5 - PCID Colorization (Reality Binding)**.

## 1. Scope

Day 25 closure covers:

- mode-partitioned PCID allocation bounds
- fail-closed `vmm_switch` validation (pcid/mode/alignment)
- deterministic TLB scrubbing on PCID free/reuse
- secure transition context switching (`PCID_KERNEL_SECURE`)
- deterministic runtime evidence and fail-closed rejection markers

## 2. Vision Alignment Contract

- Reality isolation remains hardware-backed through strict PCID colorization.
- Sensitive transition paths execute in dedicated secure-kernel PCID context.
- Day 25 behavior is closure-gated by deterministic matrix markers.

## 3. Security Contract

- Invalid mode/PCID/alignment inputs in `vmm_switch` are fail-closed (`kpanic`).
- User-mode PCIDs must remain inside their mode range.
- Kernel context may only use `PCID_KERNEL` or `PCID_KERNEL_SECURE`.
- Recycled PCIDs must receive context TLB scrub before reuse.
- Day 25 failures emit explicit `[DAY25-FAIL]` markers and block closure.

## 4. Performance Contract

- Hot-path CR3 switching remains PCID-optimized (`NOFLUSH=1`) outside mandatory flush scenarios.
- GHOST transitions force flush behavior to preserve zero-residue semantics.
- Added Day 25 checks remain bounded boot-time probes.

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 25 PCID Partition Contract: SUCCESS.`
- `[TEST] Day 25 TLB Scrub Contract: SUCCESS.`
- `[TEST] Day 25 Secure Context Contract: SUCCESS.`

Forbidden markers:

- `[DAY25-FAIL]`

## 6. Enforcement Points

- PCID allocator + free scrub behavior: `kernel/pcid.c`
- CR3 switch fail-closed policy + counters: `kernel/vmm.c`
- secure transition context helper usage: `kernel/mode.c`
- deterministic Day 25 probes/markers: `kernel/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day25_closure_suite.sh`

## 7. Exit Criteria

Day 25 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 25 required markers and no Day 25 forbidden markers
- `./tools/run_day25_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
