# Epoch III, Day 53: Syscall ABI v2 Gate Envelope Implementation

**Date:** Wednesday, February 18, 2026  
**Status:** COMPLETE  
**Modules:** `shared/include/syscall.h`, `kernel/include/syscall.h`, `kernel/syscall.c`, `user/lib/reaper.c`, `kernel/main.c`

## 1. Executive Summary
Implemented syscall ABI v2 by moving userspace to a single syscall entry (`SYS_GATE_CALL`) with operation ids and a fixed payload envelope (`gate_call_msg_t`).

## 2. What Was Implemented
- Added new shared ABI surface:
  - `SYS_GATE_CALL`
  - `GATE_OP_*` operation ids
  - `gate_call_msg_t` payload structure
- Removed public legacy syscall-number exposure from kernel headers.
- Updated kernel dispatcher (`kernel/syscall.c`):
  - reject non-gate syscall numbers
  - decode/copy gate payload from userspace
  - translate gate op ids to internal legacy handlers
- Updated userspace wrapper layer (`user/lib/reaper.c`):
  - all public `sys_*` wrappers now use gate envelope marshalling
- Added runtime marker for redesign verification:
  - `[TEST] Syscall Gate ABI v2: SUCCESS.`

## 3. Why It Was Added
- To enforce a single controlled syscall boundary for userspace entry.
- To decouple external ABI from internal handler numbering and support full redesign flexibility.

## 4. Verification Evidence
- [PASS] `make -C user`
- [PASS] `make -C kernel`
- [PASS] `make -C kernel verify_matrix` (3/3 runs)
- [PASS] serial markers in matrix logs:
  - `[TEST] Syscall Gate ABI v2: SUCCESS.`
  - `[TEST] Syscall Gate validation invariants: SUCCESS.`
  - `[TEST] Syscall Gate security probes: SUCCESS.`
  - `[TEST] Syscall Gate SMP isolation: SUCCESS.`
  - `[TEST] Syscall Gate performance budget: SUCCESS.`
  - `PARADIGM: Boundary probes passed (safe failures confirmed).`

## 5. Conclusion
Syscall ABI v2 gate envelope is live and validated. Userspace no longer directly invokes legacy syscall numbers.
