# Scheduler Lock Order (Final Product Baseline)

This document freezes the lock acquisition discipline for scheduler-adjacent paths.

## Canonical Order

1. `scheduler_cpu_state_t.runq_lock`
2. Subsystem-local wait-queue lock (for example `lattice->lock`)
3. Mode transition lock (`kernel_mode_state.transition_lock`) only in non-scheduling control paths

Never acquire a scheduler run-queue lock while holding `kernel_mode_state.transition_lock`.

## Required Rules

- All runnable/blocking transitions must go through scheduler APIs:
  - `scheduler_set_state`
  - `scheduler_block`
  - `scheduler_wake`
  - `scheduler_add_zombie`
- Direct `thread->state = ...` writes are forbidden outside:
  - thread bootstrap initialization
  - static boot-thread initialization inside `scheduler_init`
- Queue mutation must be protected by the owning run-queue lock.
- Reaper destroys happen outside the run-queue lock after victim extraction.

## Current Implementation Notes

- Runtime currently executes on CPU0, but scheduler state is indexed by CPU ID and stored in an array (`SCHED_MAX_CPUS`) to preserve SMP lock ownership semantics.
- Remote enqueue/wake is recorded as telemetry (`remote_enqueue`) and currently uses direct target-queue insertion without IPI.
- Mode-aware runnable gating is enforced at enqueue/wake/dispatch boundaries; denied events are counted (`denied_enqueue`, `denied_wake`, `denied_dispatch`).

## Audit Checklist

- No lock-order inversion between run-queue lock and subsystem wait-queue locks.
- No unbounded work while holding run-queue lock.
- No reaper free/scrub while holding run-queue lock.
- Metrics and denial counters remain readable through `SYS_SCHED_METRICS` without violating lock order.
