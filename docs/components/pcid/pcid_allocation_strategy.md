# PCID Allocation Strategy (Mode-Partitioned)

This document defines deterministic mode-aware PCID allocation behavior for Reaper-OS Law 5.

## 1. Partition Model

The allocator does not use a global 1-4094 first-fit policy.
It uses fixed mode-partitioned ranges:

- Casual: `1-256`
- Secure: `257-512`
- Lockdown: `513-768`
- Ghost: `769-1024`

Kernel-only IDs:

- `PCID_KERNEL = 0`
- `PCID_KERNEL_SECURE = 4095`

## 2. Allocation Lifecycle

### A. Allocate

1. Resolve mode bitmap + range.
2. Scan from per-mode next-hint (round-robin).
3. Set bitmap bit and return mode-base + relative index.
4. Update mode and global metrics.

### B. Free

1. Validate mode/range ownership.
2. Issue `INVPCID` type-1 (single context) scrub for the PCID.
3. Clear bitmap bit and update counters.

This guarantees reused IDs are scrubbed before ownership transfer.

## 3. Switch Policy

- `vmm_switch(...)` enforces mode->PCID invariants.
- `MODE_GHOST` forces `NOFLUSH=0` for mandatory flush behavior.
- Invalid mode/PCID/alignment inputs are fail-closed.

## 4. Exhaustion Behavior

Each mode has a bounded per-range capacity (`PCIDS_PER_USER_MODE`).
Allocation failure returns `PCID_ERROR` and is treated as hard failure by callers.

## 5. Closure Requirements

Runtime closure must prove:

- mode-range allocation correctness
- deterministic TLB scrub accounting on free
- secure context switch behavior for transition-critical paths
