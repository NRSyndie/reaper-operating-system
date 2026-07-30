# Epoch III Backlog: Security, Performance, Vision

## Objective
Execute the approved Epoch III strategy with security-first sequencing, measurable performance guardrails, and explicit vision continuity milestones.

## 2026-04-29 Progress Snapshot
- Audit Subsystem hardening landed (Day 83):
  - 128-byte aligned `audit_record_t` implemented and verified.
  - SMP-safe atomic ring buffer (1024 slots) with Acquire/Release semantics.
  - BLAKE3-backed record chaining and reality-bound seed rotation on Phase Shift.
  - Instrumentation for Threads, Capabilities (Mint/Denial), Scheduler Stall, and Phase Shifts.
  - Overflow transparency via explicit `AUDIT_EVENT_OVERFLOW` and `gap_seq` tracking.
  - `RDRAND`-first root seed path with explicit weak fallback retained as the remaining Ghost-mode hardening item.
- ACPI Layer 1/2 foundation landed (Day 84):
  - Limine RSDP handoff integrated into `acpi_init()`.
  - Static MADT, FADT, HPET, MCFG, and DMAR parsing added for later hardware bring-up.
  - Boot self-test marker `[TEST] ACPI Layer 1+2: SUCCESS.` verified in serial logs.
- DMA authority contract + DMAR truth freeze landed (Day 86):
  - canonical IOMMU inventory state model added
  - explicit degraded/unavailable classification added
  - ACPI DMAR export became the sole firmware truth path for IOMMU inventory
  - boot self-test markers for inventory and degraded policy verified in serial logs
- Paradigm stack baseline closure landed (Day 87):
  - serial evidence confirmed the early user fault below `rsp=0x800000`
  - Genesis stack mapping widened from 1 page to 8 pages
  - runtime confidence revalidated with three consecutive clean matrix invocations
- Slot 1 standard microkernel Step 3 landed:
  - baseline `rwlock` primitives added
  - baseline `seqlock` primitives added
  - baseline `rcu` primitives added
  - deterministic boot self-tests added in kernel for all three primitives
- Slot 1 standard microkernel Step 4 landed:
  - buffered IPC endpoint queues added
  - split send/receive wait queues added
  - rights-aware endpoint invocation validated
- Runtime confidence check rerun:
  - `make -C kernel verify_matrix` passed (3/3) after Slot 1 Step 3 changes.
- Runtime confidence baseline refreshed:
  - `make -C kernel verify_matrix` passed in three consecutive invocations after the Day 87 stack fix.
- Current standard slot follow-up:
  - Slot 1 Step 5 memory-path rollout evidence is landed.
  - Remaining slot work is now priority/policy expansion, multicore runtime activation, and later IPC call-chain tracking.
- Area 1: Genesis & Process Restoration — Phase 1 (DONE 2026-06-15):
  - Surgically cleaned `process.c`, `syscall.c`, and `genesis.c` corruption.
  - Reimplemented process registry (`process_find_by_pid`, `process_register_live`, `process_unregister_live`) with spinlock protection.
  - Restored `genesis_syscall_dispatch` with `GENESIS_OP_SPAWN` and `GENESIS_OP_DESTROY`.
  - Added `cap_genesis_is_exhausted` and `cap_genesis_exhaust` atomic one-way flag in `capability.c`.
  - Verified with three consecutive clean Law2+Fate matrix runs.
- Area 1: Genesis Syscall Dispatch Closure — Phase 2 (DONE 2026-07-19):
  - Added `GENESIS_OP_DELEGATE` case with four-type whitelist (`genesis_is_delegatable_type`), `process_find_by_pid` lookup, direct `cap_identity_create` + `cap_insert` into target cspace, and dual kernel/userspace (`genesis_copy_from_user`) copy paths.
  - Fixed `GENESIS_OP_SPAWN` userspace path: replaced stub `return -1` with `genesis_copy_from_user`.
  - Fixed `GENESIS_OP_SPAWN` caps struct: `genesis_cap_slot = 0` (no Genesis authority on spawn), `out_sched_root_slot` and `out_sched_thread_slot` from request fields, `owner->mode` replaces `MODE_CASUAL`.
  - Added `genesis_inject_initial_caps` guard: `genesis_cap_slot == 0` skips Genesis cap injection.
  - Extended `test_genesis_lifecycle` to 8 steps: Step 3b delegates `CAP_TYPE_REALITY_CTRL` to spawned process, verifies via `cap_lookup`, then verifies `CAP_TYPE_RAM` is rejected by whitelist.
  - Verified with headless boot serial evidence and `make -C kernel verify_matrix` 3/3 PASS.
- Area 2: PID-Privilege Removal Closure (DONE 2026-07-23):
  - Added `process_has_capability_at(proc, slot, expected_type, required_rights)` — O(1) direct slot lookup in `capability.c`/`capability.h`; validates epoch aliveness, type, mode mask, and rights bitmask.
  - Replaced `pid == 1` check in `SYS_MODE_QUERY` with `sys_mode_query_handler`: requires `CAP_TYPE_REALITY_CTRL` + `CAP_RIGHT_READ` at caller-supplied slot `a0`; slot 0 sentinel returns `MODE_CASUAL` unconditionally.
  - Replaced `pid != 1` reject in `SYS_SCHED_AUTH_ROOT_MINT` with `sys_sched_auth_root_mint_handler`: requires `CAP_TYPE_REALITY_CTRL` + `CAP_RIGHT_GRANT` at caller-supplied slot `a0`; slot 0 sentinel returns error unconditionally.
  - Added `SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER 4ULL` named constant with inline policy documentation replacing the former silent `max_total_budget * 4` expression.
  - Genesis injects `CAP_TYPE_REALITY_CTRL` at slot 7 into Paradigm's CNode during `genesis_bridge_spawn` (`reality_ctrl_slot = 7`) — resolves bootstrap chicken-and-egg for `SYS_SCHED_AUTH_ROOT_MINT`.
  - `GENESIS_OP_SPAWN` preserves `reality_ctrl_slot = 0` sentinel — child daemons do not inherit reality control by default.
  - Added `test_pid_privilege_removal()` in `kernel/main.c`: creates two dynamically-PID'd test processes, verifies positive and negative paths for both `sys_mode_query_handler` and `sys_sched_auth_root_mint_handler` by direct function call, destroys both processes at end.
  - Verified with `[KERNEL] pid-privilege-removal: PASS` serial marker and `make -C kernel verify_matrix` 3/3 PASS.

## Workstream 1 — Security Contracts (Priority 0)
### 1.1 `SYS_AUDIT` / Fate Strings Foundation
- [DONE] Define 128-byte record and 1024-slot lattice.
- [DONE] Implement SMP-safe atomic ring buffer.
- [DONE] Implement Reality-bound seed rotation.
- [DONE] Instrument Scheduler Stall, Thread Lifecycle, and Capability Mint/Denial.
- [OPEN-RISK] Durable sealed-storage or equivalent Ghost-mode root seed hardening.
- [OPEN-RISK] VT-d translation enablement and invalidation lifecycle remain deferred after inventory freeze.

**Acceptance gate**
- [DONE] Contract doc updated and reviewed.
- [DONE] static_assert verified.
- [DONE] Reality seed rotation confirmed in logs.

### 1.2 Zero-residue hardening completion
- Finalize cross-color context scrub policy.
- Add deterministic runtime markers for scrub events and policy enforcement.

**Acceptance gate**
- Runtime probes confirm no cross-color residue leakage markers.
- Existing Fate/Law2 markers remain stable.

### 1.3 Annihilation archives policy
- Define retention policy, memory budget ceiling, and query constraints.
- Specify capability-bounded access model.

**Acceptance gate**
- Policy/spec document added.
- Explicit non-goals captured for anything beyond Epoch III scope.

---

## Workstream 2 — Deterministic Performance (Priority 1)
### 2.1 ESAK deterministic scheduler hardening
- [DONE] Implement root/thread scheduling authority model.
- [DONE] Enforce atomic process-budget consume/refill invariants.
- [DONE] Deterministic weighted RR with fairness probes.
- [OPEN-RISK] Add starvation upper-bound marker under mixed workload.
  Source: mixed-workload tail-latency bounds are not yet exposed as a dedicated runtime marker in `kernel/scheduler.c`.
- [DONE] Finalized BSP-only ESAK profile marker (`[TEST] ESAK IPI profile: BSP_ONLY`) for closure scope.

**Acceptance gate**
- Scheduler probes pass across mixed workload scenarios.
- No regression in existing runtime matrix required markers.

### 2.2 Performance baselining
- Add repeatable timing markers for scheduler and syscall hot paths.
- Record baseline vs post-change comparison in report artifacts.

**Acceptance gate**
- Baseline methodology documented.
- Performance deltas are measurable and reproducible.

---

## Workstream 3 — Vision Continuity (Priority 2)
### 3.1 Reality-aware bootinfo evolution
- Define v1 compatibility bridge and fragment-aware extension path.
- Stage rollout to avoid Paradigm bootstrap breakage.

**Acceptance gate**
- Compatibility contract documented and validated in runtime boot checks.

### 3.2 Daemon authority partitioning
- Define minimal capability surfaces for Genesis, Paradigm, Archive, Sage, Tunnel, Veil, and Paradigm's embedded Sentinel subsystem.
- Document derivation boundaries and delegation flow.

**Acceptance gate**
- Authority maps documented with explicit least-privilege boundaries.

---

## Suggested Execution Milestones
1. **M1 (Security Contract Freeze):** `SYS_AUDIT` contract + zero-residue policy finalized.
2. **M2 (Security Runtime Validation):** new probes integrated; matrix expanded for security invariants.
3. **M3 (Deterministic Scheduler):** ESAK authority + weighted RR + atomic budget merged with fairness and regression evidence.
4. **M4 (Vision Bridge):** reality-aware bootinfo compatibility bridge validated.
5. **M5 (Daemon Partition Plan):** authority surfaces documented and approved for implementation phase.

## Validation Discipline
- Keep using `make -C kernel verify_matrix` as baseline gate.
- Extend matrix markers only when new invariants are introduced.
- Every milestone must include:
  - changed files list
  - explicit pass/fail probe markers
  - known limits and deferred follow-ups

## Risk Register and Counters
### R1: Boot-time denial of service via over-quarantine
- **Risk:** excessive quarantine leaves too few allocatable frames.
- **Counter:** enforce minimum free-frame floor; cap quarantine ratio; define emergency fallback tier with explicit degraded-mode marker.

### R2: Boot-time performance regression from attestation
- **Risk:** startup latency increases from region verification passes.
- **Counter:** staged verification (critical-first), strictness profiles, and cached verification state where valid.

### R3: PMM arithmetic/policy implementation bugs
- **Risk:** overflow or boundary mistakes corrupt allocator state.
- **Counter:** overflow-safe arithmetic helpers, invariant asserts, and hard fail-closed path with explicit reason markers.

### R4: Security leakage through verbose diagnostics
- **Risk:** logs expose sensitive physical layout details.
- **Counter:** redact precise addresses in normal/release mode; allow full detail only in debug mode.

### R5: False confidence in “verified” memory labels
- **Risk:** incomplete checks classify unsafe memory as trusted.
- **Counter:** multi-tier confidence labels (`verified`, `unverified`, `quarantined`) and default allocation from highest trust tier only.

### R6: Compatibility regressions across hardware/firmware variants
- **Risk:** stricter policy fails on systems with unusual memmap layouts.
- **Counter:** compatibility mode profile, firmware-type telemetry markers, and per-platform allowlist exceptions with audit trail.

### R7: Metadata overhead and memory pressure
- **Risk:** additional state tracking reduces usable RAM on low-memory targets.
- **Counter:** keep fixed-size metadata invariants; budget checks during init; fail with explicit guidance when below floor.

### R8: Nondeterministic verification outcomes
- **Risk:** intermittent boot pass/fail due to timing-sensitive probes.
- **Counter:** deterministic probe ordering and retry policy with bounded attempts; classify flaky regions as quarantined.

### R9: Regression spillover into existing features
- **Risk:** Law2/Fate/Lattice behavior regresses while changing PMM boot policy.
- **Counter:** preserve existing matrix required/forbidden markers and add PMM-specific markers without removing older gates.

### R10: Misconfiguration/operator error
- **Risk:** incorrect strictness level deployed unintentionally.
- **Counter:** compile-time default-safe profile, boot log of active profile, and explicit marker when non-default profile is used.

### R11: Malicious/corrupt boot metadata input
- **Risk:** crafted memmap drives unsafe allocator decisions.
- **Counter:** strict entry validation, overlap checks, range sanity constraints, and immediate fail-closed on invalid map topology.

### R12: Logging channel failure during fault scenarios
- **Risk:** failure occurs before logs flush, reducing forensic visibility.
- **Counter:** early-boot ring buffer for critical markers + serial flush checkpoints at phase boundaries.

## Failure Logging and Incident Workflow
### Logging Schema
- Prefix all new markers with stable domains:
  - `[PMM-AUDIT]` for map validation and region classification
  - `[PMM-VERIFY]` for attestation/probe outcomes
  - `[PMM-QUAR]` for quarantine decisions and thresholds
  - `[PMM-FAIL]` for fail-closed reasons
  - `[PMM-PROFILE]` for active strictness/compatibility profile
- Every fatal marker must include:
  - policy phase
  - reason code
  - confidence tier impacted

### Logging Levels
- **Release level:** concise reason codes, redacted addresses.
- **Debug level:** full addresses and detailed per-region evidence.

### Incident Flow (when anything goes wrong)
1. Capture serial + matrix logs (`kernel/serial.log`, `kernel/serial_matrix_run*.log`).
2. Locate first `[PMM-FAIL]` marker and reason code.
3. Correlate with `[PMM-PROFILE]` and verification/quarantine markers.
4. Classify incident:
   - policy mismatch
   - arithmetic/invariant violation
   - compatibility/firmware variance
   - resource floor breach
5. Apply countermeasure:
   - bug fix and re-run matrix
   - compatibility rule update with audit note
   - threshold/profile adjustment with explicit rationale
6. Record outcome in day report/checklist/version entry with required/forbidden marker evidence.
