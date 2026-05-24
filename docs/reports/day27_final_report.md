# Day 27 Final Report: Syscall Boundary Hardening + Law 2 Strict Foundation

## 1. Overview
Day 27 focused on reducing kernel risk at the user-kernel boundary while opening a safe rollout path for **Law 2: Shadow Mapping** hardening. The implementation preserves current ABI behavior while introducing strict enforcement controls for staged adoption.

## 2. Infrastructure & Implementation

### A. Syscall Contract Freeze
- Added `docs/components/syscalls/syscall_contracts.md` to define argument/return contracts for high-risk syscall surfaces.
- Locked baseline rules for:
  - `SYS_MAP`
  - `SYS_UNMAP`
  - `SYS_FATE_READ`

### B. Boundary Hardening in `kernel/syscall.c`
- Unified user-range validation through canonical helper paths used by syscall copy operations.
- Ensured malformed user pointers fail with `-1` instead of risking kernel fault behavior.
- Added explicit rejection accounting for:
  - fault-based rejects
  - invalid argument rejects
  - permission rejects

### C. Law 2 Strict Rollout Path (Non-Breaking)
- Added strict-mode controls without introducing new syscall numbers:
  - `SYS_MAP`: strict mode in `a4 bit0`
  - `SYS_UNMAP`: strict marker in `a2 bit0`
- Strict mode currently enforces:
  - parent must be writable `CAP_TYPE_PAGETABLE`
  - child must be `CAP_TYPE_RAM` or `CAP_TYPE_PAGETABLE`
  - strict non-leaf links require `USER|WRITABLE`
  - strict mode rejects unknown user PTE bits outside allowed mask

### D. Observability
- Added periodic syscall diagnostics and counters for:
  - map/unmap calls and strict-call counts
  - map failure reasons (parent/rights/index/child/flags)
  - TLB flush count

### E. Userland Staging APIs
- Added wrappers:
  - `sys_map_strict(...)` (historical Day 27 wrapper; removed in Day 72 when strict path became default `sys_map(...)`)
  - `sys_unmap_strict(...)` (historical Day 27 wrapper; removed in Day 72 when strict path became default `sys_unmap(...)`)
- Enables incremental migration of user-space mapping paths to strict mode.

## 3. Verification Results
- [PASS] Kernel build successful (`make -C kernel`).
- [PASS] Userland build successful (`make -C user`).
- [PASS] Legacy compatibility retained while strict mode is available for staged enablement.

## 4. Next Step
- Migrate Paradigm mapping paths to strict wrappers and add explicit negative-path tests (invalid flags, invalid parent/child, invalid index, strict-mode enforcement failures).

**"The boundary must reject chaos before the multiverse can safely expand."**
