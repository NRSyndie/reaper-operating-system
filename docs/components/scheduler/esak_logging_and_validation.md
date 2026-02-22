# ESAK Logging and Validation Strategy

This document defines mandatory observability and validation gates for the
Envelope-Scheduled Authority Kernel (ESAK) scheduler model.

## 0. Frozen Implementation Phases

### Phase 1 - Capability Model Finalization

- Define `CAP_TYPE_SCHED_AUTH_ROOT` with:
  - `mode_binding`
  - `max_total_budget`
  - `refill_period`
  - `max_accumulated`
- Store as a real capability object in process C-Space.
- Define `CAP_TYPE_SCHED_AUTH_THREAD` derived from root with:
  - `max_slice`
  - `weight`
  - `local_max_accumulated`
- Derivation constraints:
  - must not exceed root authority
  - must inherit root mode binding
- Ambient scheduling is forbidden.
- Threads without valid derived authority enter `THREAD_BLOCKED_AUTH`.
- Processes without root authority cannot spawn runnable threads.

### Phase 2 - Budget Accounting Integration

- Add process-level budget state in `process_t`:
  - `remaining_process_budget`
  - `last_process_refill`
- Shared across CPUs (atomic updates).
- Add thread-level budget state in `thread_t`:
  - `remaining_thread_budget`
  - `last_thread_refill`
- Dispatch must deduct from both thread and process budgets.
- If either reaches zero, remove from runnable state.
- Refill is deterministic and heuristic-free:
  - refill process first
  - refill thread bounded by process remaining capacity

### Phase 3 - Scheduler Enforcement Completion

- Replace legacy ordering with deterministic weighted RR per envelope.
- No dynamic priority boosts.
- Stable rotation pointer per envelope.
- Immediate revocation path:
  - mark auth invalid
  - atomically dequeue if runnable
  - move to `THREAD_BLOCKED_AUTH`
- Root revocation cascade invalidates all derived thread auths and makes
  process unschedulable.

### Phase 4 - Mode Integration Hardening

- Enforce mode binding:
  - thread auth mode == process root mode == active envelope mode
- Mismatch must reject enqueue.
- On mode switch:
  - freeze inactive envelopes
  - preserve budgets/authorities
  - do not destroy threads/authorities
- Ghost mode:
  - force PCID switch
  - reset envelope runtime counters
  - preserve authority objects

### Phase 5 - SMP Completion

- Atomic process-budget deduction with underflow prevention.
- Same-mode work stealing only.
- Never steal across mode envelopes.
- Validate thread auth before migration.
- Lock order: `CPU -> envelope -> process -> thread`.

### Phase 6 - Syscall Layer

- Add root-mint syscall:
  - privileged caller only
  - explicit mode binding required
- Add thread-derive syscall:
  - requires root authority
  - enforces reduction-only semantics
- Use capability revoke path for explicit authority revocation.
- Scheduler must observe revocation immediately.

### Phase 7 - Runtime Validation Markers

Permanent required test markers:

- `[TEST] No authority -> no execution`
- `[TEST] Root ceiling enforced`
- `[TEST] Thread explosion prevented`
- `[TEST] Revocation immediate dequeue`
- `[TEST] Cross-mode scheduling rejected`
- `[TEST] Deterministic RR rotation stable`
- `[TEST] SMP atomic budget integrity`

`verify_matrix` must cover all markers.

### Phase 8 - Final Invariants Lock

Runtime assertions:

- runnable thread must have valid thread authority
- thread authority must have valid root ancestor
- process budget never negative
- envelope mode matches thread mode

Final cleanup:

- remove legacy fallback scheduling paths
- remove ambient enqueue logic
- freeze ESAK model as final

## 1. Logging Principles

- Emit structured `klog` markers with stable key/value fields.
- Use one canonical marker per invariant violation class.
- Keep secure/lockdown mode output coarse; do not expose high-resolution
  per-thread timing details.

## 2. Required Scheduler Markers

- `SCHED_ENVELOPE_SWITCH cpu=<id> from=<mode> to=<mode> epoch=<n>`
- `SCHED_BUDGET_EXHAUSTED tid=<tid> mode=<mode>`
- `SCHED_PCID_VIOLATION cpu=<id> tid=<tid> mode=<mode> active=<mode>`
- `SCHED_GHOST_FLUSH enter=<1|0> exit=<1|0>`

Follow-on implementation markers required before final closure:

- `SCHED_DISPATCH cpu=<id> mode=<mode> tid=<tid> grant=<ticks>`
- `SCHED_AUTH_DENY tid=<tid> reason=<no_auth|revoked|mode_mismatch>`
- `SCHED_REVOKE_CASCADE root=<cap> affected=<n>`
- `SCHED_STEAL cpu=<dst> src=<src> mode=<mode> tid=<tid>`
- `SCHED_STEAL_DENY cpu=<dst> src=<src> reason=<cross_mode|cpu_mask>`
- `SCHED_FAIRNESS_FLOOR_OK window=<n>`
- `SCHED_FAIRNESS_FLOOR_FAIL tid=<tid>`
- `SCHED_GHOST_FLUSH enter=<1|0> exit=<1|0>`

## 3. Required Metrics Surface

`SYS_SCHED_METRICS` must export at least:

- `schedule_count`, `switch_count`, `remote_enqueue`, `migrations`
- `denied_enqueue`, `denied_wake`, `denied_dispatch`
- `denied_no_auth`, `denied_mode_mismatch`
- `budget_exhaustions`, `envelope_switches`
- `active_security_epoch`, `active_mode`
- `cpu_id`, `ready_depth`, `zombie_depth`

## 4. Test Matrix Requirements

1. Build gates:
- `make -C user`
- `make -C kernel`

2. Runtime gate:
- `make -C kernel run`
- Confirm required scheduler markers in `kernel/serial.log`
- Confirm no panic/fail markers

3. Matrix gate:
- `make -C kernel verify_matrix`
- Require all runs pass and scheduler markers appear in every
  `kernel/serial_matrix_run*.log`

4. Functional scheduler gates:
- Envelope switch correctness across modes
- Budget depletion/refill determinism
- Deterministic weighted dispatch order checks
- Same-mode steal allowed and cross-mode steal denied

5. Security gates:
- No authority -> no dispatch
- Revoked authority -> immediate eligibility loss
- Mode mismatch denied at enqueue/wake/dispatch
- Mode/PCID mismatch triggers illegal-state path
- Ghost transition flush markers on enter/exit

## 5. Release Exit Criteria

ESAK scheduling is release-ready only when:

- no `SCHED_PCID_VIOLATION` events occur
- no successful cross-mode dispatch/steal events occur
- required markers and metrics are present in runtime and matrix logs
- docs/checklists/version records are synchronized with exact evidence paths

## 6. Current Runtime Status (2026-02-22)

- Implemented:
  - root/thread scheduling authorities (`CAP_TYPE_SCHED_AUTH_ROOT`, `CAP_TYPE_SCHED_AUTH_THREAD`)
  - deterministic weighted RR token rotation
  - atomic process-budget consume/refill primitives
  - immediate revoke dequeue + forced-reschedule flag path
  - scheduler security runtime markers:
    - `[TEST] No authority -> no execution`
    - `[TEST] Root ceiling enforced`
    - `[TEST] Thread explosion prevented`
    - `[TEST] Revocation immediate dequeue`
    - `[TEST] Cross-mode scheduling rejected`
    - `[TEST] Deterministic RR rotation stable`
    - `[TEST] SMP atomic budget integrity`
    - `[TEST] ESAK IPI profile: BSP_ONLY`
- Final boundary:
  - Product profile is ratified as BSP-only for current closure scope.
