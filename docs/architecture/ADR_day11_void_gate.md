# Architecture Decision Record: Day 11 - The Void Gate Redesign

## Status
Accepted

## Context
Previous versions of Reaper-OS used direct jumps into user-space and allowed occupants to query the raw system mode. This violated the core philosophy that "Occupants never know reality" and created a weak binding between scheduling authority and the current security epoch.

## Decisions

### 1. Contract/Lease-Based Entry
We have replaced the `user_mode_jump` primitive with a formal 4-stage pipeline: **Compile -> Verify -> Apply -> Attest**.
- **Impact:** Every entry into user-space is now a trackable, audited event with deterministic failure markers.

### 2. Redacted Reality (SYS_MODE_QUERY)
The `SYS_MODE_QUERY` syscall is now redacted for all processes except PID 1 (Paradigm).
- **Behavior:** Non-privileged processes always receive `MODE_CASUAL`.
- **Reasoning:** To prevent "reality leakage" where an occupant could infer the system state (e.g., detecting they are in Ghost Mode) and adapt their behavior to avoid detection or audit.

### 3. Epoch-Aware Scheduler Gates
Scheduling decisions (enqueue, wake, dispatch) now require a valid `entry_lease_t` bound to the thread.
- **Verification:** The scheduler checks `lease.epoch == current_security_epoch`.
- **Enforcement:** Stale leases (from a previous Reality/Mode) are immediately rejected, causing the thread to block until Paradigm re-authorizes or destroys it.

### 4. Performance Caching
To avoid a $O(N)$ penalty on every dispatch, we introduced a 1-slot validation cache in `thread_t`.
- **Mechanism:** `lease_validated_epoch` stores the last epoch where the lease was found valid. If the epoch hasn't changed, the check is $O(1)$.

## Consequences
- **Security:** Immediate "fail-closed" behavior during Reality Shifts. When Paradigm advances the Security Epoch, all existing occupant leases are instantly invalidated across all CPUs.
- **Observability:** High-fidelity audit markers (`[ENTRY_*]`) allow the Fate Matrix to verify entry invariants.
- **Complexity:** Increased complexity in `thread_create` and `scheduler.c`, now requiring explicit lease management.
