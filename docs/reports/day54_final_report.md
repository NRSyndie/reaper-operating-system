# Epoch III, Day 54: ESAK Scheduler Authority + Atomic Budget Hardening

**Date:** Saturday, February 21, 2026  
**Status:** IN PROGRESS (core slice complete; SMP IPI activation deferred)  
**Modules:** `kernel/scheduler.c`, `kernel/capability.c`, `kernel/mode.c`, `kernel/main.c`, `shared/include/capability.h`, `shared/include/syscall.h`, `kernel/syscall.c`

## 1. Executive Summary
Implemented the core ESAK hardening slice for scheduler authority and budget integrity: root/thread scheduling capabilities, deterministic weighted RR rotation, atomic process budget accounting, and immediate revocation dequeue semantics.

## 2. What Was Implemented

- Capability model split:
  - `CAP_TYPE_SCHED_AUTH_ROOT`
  - `CAP_TYPE_SCHED_AUTH_THREAD`
- Scheduler authority APIs:
  - root mint path
  - thread derive path
  - immediate revoke handlers for thread/process authorities
- Atomic process budget contract:
  - CAS-based consume/refill helpers
  - dispatch path consumes budget without underflow
- Deterministic RR:
  - token-based weighted rotation with stable queue behavior
- Revocation race hardening:
  - running/ready threads are moved to blocked-auth state immediately
  - forced-reschedule request flags consumed in timer path
- Runtime markers:
  - `[TEST] No authority -> no execution`
  - `[TEST] Root ceiling enforced`
  - `[TEST] Thread explosion prevented`
  - `[TEST] Revocation immediate dequeue`
  - `[TEST] Cross-mode scheduling rejected`
  - `[TEST] Deterministic RR rotation stable`
  - `[TEST] SMP atomic budget integrity`

## 3. Why It Was Added

- To enforce strict reduction-only authority semantics for scheduler control.
- To prevent process-budget race/underflow behaviors before SMP activation.
- To make revocation behavior auditable and fail-closed in scheduling hot paths.

## 4. Verification Evidence

- [PASS] `make -C user`
- [PASS] `make -C kernel`
- [PASS] `make -C kernel iso`
- [PASS] `./tools/run_law2_fate_matrix.sh --runs 1 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel`
- [PASS] Required syscall gate and ESAK marker set observed in `kernel/serial_matrix_run1.log`

## 5. Known Limitations / Follow-Up

- Runtime is currently BSP-only (`cpu_get_id() == 0` scaffolding), so true cross-core IPI remote preemption cannot be exercised yet.
- Forced-reschedule request flags are in place and ready for SMP activation.
