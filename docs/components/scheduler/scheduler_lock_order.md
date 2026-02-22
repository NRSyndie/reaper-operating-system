# Scheduler Lock Order (Final Product Baseline)

This document freezes the lock acquisition discipline for scheduler-adjacent paths.

Complementary validation and runtime marker requirements:

- `docs/components/scheduler/esak_logging_and_validation.md`

## Canonical Order

1. `scheduler_cpu_state_t.runq_lock`
2. Envelope queue ownership (implicit by run-queue lock + active envelope selection)
3. Subsystem-local wait-queue lock (for example `lattice->lock`)
4. Mode transition lock (`kernel_mode_state.transition_lock`) only in non-scheduling control paths

Never acquire a scheduler run-queue lock while holding `kernel_mode_state.transition_lock`.

### SMP Target Order (Activation Contract)

When full SMP dispatch/IPI preemption is activated, scheduler code must preserve:

1. `CPU`
2. `envelope`
3. `process`
4. `thread`

Current runtime is BSP-only (`cpu_get_id() == 0`), so process-level scheduler lock layering is not active yet.

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
- Non-system threads must not be enqueued through ambient/legacy paths without scheduling authority.

## Current Implementation Notes

- Runtime currently executes on CPU0, but scheduler state is indexed by CPU ID and stored in an array (`SCHED_MAX_CPUS`) to preserve SMP lock ownership semantics.
- Same-mode steal scaffolding is active at queue-selection level; full SMP cross-core dispatch remains pending runtime bring-up.
- Revocation path now requests forced reschedule flags per target CPU; IPI delivery is deferred until SMP activation.
- Mode-aware runnable gating is enforced at enqueue/wake/dispatch boundaries; denied events are counted (`denied_enqueue`, `denied_wake`, `denied_dispatch`, `denied_no_auth`, `denied_mode_mismatch`).

## Audit Checklist

- No lock-order inversion between run-queue lock and subsystem wait-queue locks.
- No unbounded work while holding run-queue lock.
- No reaper free/scrub while holding run-queue lock.
- Metrics and denial counters remain readable through `SYS_SCHED_METRICS` without violating lock order.
- No non-system runnable thread exists without valid root+thread scheduling authority.
