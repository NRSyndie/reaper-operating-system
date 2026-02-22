# Reaper-OS Epoch III Plan: Final-Product Track

## 1) Mission
Epoch III now runs as a final-product delivery program: preserve ABI/runtime stability while migrating core isolation and policy behavior to a hardware-aware execution-envelope model.

## 2) Product Gates (Non-Negotiable)
- ABI and boot compatibility preserved for existing Paradigm/userland paths.
- Deterministic behavior under repeated `make -C kernel verify_matrix` runs.
- Every architecture slice has a rollback path and explicit kill criteria.
- Forensic evidence remains queryable through current Fate interfaces during migration.
- No release claim without concrete serial/build evidence.

## 3) Groundwork Already Implemented (Carry Forward)
- Capability lineage + recursive revocation (Law 1) is active.
- Mode-aware capability gating (Law 4) is active.
- Strict shadow mapping rollout (Law 2) is active and runtime-matrix validated.
- PCID colorization (Law 5) and temporal scouring (Law 9) are active.
- Auditor path for Epoch II (`CAP_TYPE_AUDITOR + SYS_FATE_READ`) is active.
- Fault-to-Fate forensic chain (`#GP/#PF` metadata path) is active.
- ReadOnly lattice broadcast + attach/detach forensics are active.
- Runtime confidence harness exists (`make -C kernel verify_matrix` via `tools/run_law2_fate_matrix.sh`).

## 4) Architecture Re-Baseline
### 4.1 Core Contract
- Promote `execution_envelope` as the canonical runtime contract.
- Treat `mode` and `pcid` as implementation details of envelope backends.

### 4.2 Required Transition Pipeline
- All sensitive context transitions must use:
  - `compile_envelope(...)`
  - `verify_envelope(...)`
  - `apply_envelope(...)`
  - `attest_envelope(...)`
- Direct legacy transition paths remain only as compatibility shims until retirement.

### 4.3 Backend Tiers
- Tier A (preferred): MPK/PKRU-backed envelope apply path.
- Tier B (baseline/fallback): PCID-backed envelope path.
- Tier C (opt-in high assurance): VMFUNC/EPT-backed envelope path for selected realities.

## 5) Phased Delivery Plan
1. **Phase 0: Architecture Freeze**
   - Deliver envelope RFC/invariants and compatibility boundaries.
   - Gate: sign-off on ABI and rollback model.
2. **Phase 1: Core Runtime Migration**
   - Route kernel transition points through compile/verify/apply/attest using PCID backend first.
   - Gate: parity with current runtime behavior and matrix passes.
3. **Phase 2: Capability and Scheduler Rebinding**
   - Bind capabilities and scheduling decisions to envelope constraints.
   - Gate: deterministic negative-path tests for unauthorized cross-envelope actions.
4. **Phase 3: Hardware Fast Path**
   - Add MPK tier with PCID fallback preserved.
   - Gate: feature-detection fallback validated on non-MPK path.
5. **Phase 4: Evidence Re-Baseline**
   - Keep Fate as compatibility projection; add measured transition records as canonical evidence.
   - Gate: one-to-one mapping between measured events and Fate-facing projections.
6. **Phase 5: Optional VMFUNC Pilot**
   - Enable one reality (initially Ghost) on VMFUNC/EPT tier.
   - Gate: opt-in only, no regressions outside pilot scope.

## 6) Kill Criteria (Per Phase)
- Any regression in determinism, ABI compatibility, or operability blocks promotion.
- Any slice that cannot be cleanly rolled back is not release-eligible.
- Any undocumented divergence between claims and runtime evidence blocks closure.

## 7) Exit Criteria
- Security: No unresolved capability escalation paths in new audit/scheduler/bootinfo/envelope surfaces.
- Forensics: Transition evidence is deterministic and queryable through both canonical and compatibility paths.
- Performance: Measured improvement or neutral impact versus current baseline under controlled runs.
- Stability: 10 consecutive clean matrix passes with no forbidden markers.
- Productization: Release docs, roadmap state, and version ledger are synchronized.

## 8) Execution Backlog and Artifacts
- See `docs/development_log/epoch_three_backlog.md` for milestone sequencing, risk register, and acceptance discipline.
- Day 43 security contract freeze artifacts:
  - `docs/components/syscalls/syscall_contracts.md` (`SYS_AUDIT` fail-closed freeze contract)
  - `docs/components/modes/zero_residue_policy.md` (zero-residue baseline policy)
- Day 44 PMM migration artifact:
  - `docs/components/memory/pmm_zoned_buddy_strategy.md` (policy/zone-to-buddy transition plan)
- Day 45 VMM migration artifact:
  - `docs/components/memory/vmm_region_contracts.md` (region-contract compile/apply bridge)
- Day 47 Day 5R migration artifact:
  - `docs/components/modes/day5r_envelope_multimode_logic.md` (multi-mode logic mapped to execution-envelope contract)
- Day 54 ESAK scheduler hardening artifacts:
  - `docs/components/scheduler/esak_logging_and_validation.md`
  - `docs/components/scheduler/scheduler_lock_order.md`
  - `docs/components/syscalls/syscall_contracts.md` (sched-auth gate ops + metrics)

## 9) Current Status Checkpoint (2026-02-22)

- Active slice: Epoch III closure ratification (Days 46/47/54).
- Completed in current slice:
  - kernel transition envelope pipeline with compile/verify/apply/attest marker evidence
  - Paradigm accepted/rejected transition probes through `GATE_OP_MODE_TRANSITION`
  - root/thread scheduling authority capability model
  - deterministic weighted RR token rotation
  - atomic process budget consume/refill contract
  - revoke-immediate dequeue path with forced-reschedule flags
  - expanded runtime markers and matrix gate assertions
- Product boundary:
  - BSP-only ESAK profile is explicitly ratified for this closure (`[TEST] ESAK IPI profile: BSP_ONLY`).
