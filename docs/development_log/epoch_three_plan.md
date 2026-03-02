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

## 9) Current Status Checkpoint (2026-03-02)

- Active slice: Epoch III closure ratification (Days 46/47/54/55 + Day 12 closure ratification + Day 13 closure ratification + Day 14 closure ratification + Day 15 closure ratification + Day 16 closure ratification + Day 17 closure ratification + Day 18 closure ratification + Day 19 closure ratification + Day 20 closure ratification + Day 21 closure ratification + Day 22 closure ratification + Day 23 closure ratification + Day 24 closure ratification + Day 25 closure ratification + Day 26 closure ratification + Day 27 closure ratification).
- Completed in current slice:
  - kernel transition envelope pipeline with compile/verify/apply/attest marker evidence
  - Paradigm accepted/rejected transition probes through `GATE_OP_MODE_TRANSITION`
  - root/thread scheduling authority capability model
  - deterministic weighted RR token rotation
  - atomic process budget consume/refill contract
  - revoke-immediate dequeue path with forced-reschedule flags
  - expanded runtime markers and matrix gate assertions
  - **Day 11 Void Gate:**
    - 4-stage Entry Pipeline (Compile/Verify/Apply/Attest) for initial genesis and scheduler jumps.
    - Redacted `SYS_MODE_QUERY` for occupant reality isolation.
    - Epoch-aware lease verification in scheduler gates with performance caching.
    - Deterministic `[ENTRY_*]` audit markers verified in matrix.
  - **Day 12 Closure Ratification:**
    - deterministic closure markers for fault isolation, rendezvous, reaper lifecycle, and process annihilation added to kernel self-tests.
    - matrix required-marker gate extended to include all Day 12 closure markers.
    - Vision/Security/Performance closure artifacts synchronized in roadmap/report/version docs.
  - **Day 13 Closure Ratification:**
    - deterministic closure markers for extended-state init, context preservation, cross-thread isolation, and crucible stability added to kernel self-tests.
    - matrix gate extended with Day 13 required markers and forbidden `[DAY13-FAIL]` marker.
    - repeat-run Day 13 closure suite introduced for multi-run confidence validation.
  - **Day 14 Closure Ratification:**
    - deterministic closure markers for wait/yield/lifecycle ABI surface added to kernel self-tests.
    - matrix gate extended with Day 14 required markers and forbidden lifecycle failure markers.
    - repeat-run Day 14 closure suite introduced for multi-run confidence validation.
  - **Day 15 Closure Ratification:**
    - deterministic closure markers for genesis module contract, capability injection, and bootinfo bridge added in `kernel/genesis.c`.
    - Paradigm bridge probe pass/fail markers added for user-space validation in `user/paradigm/main.c`.
    - matrix gate extended with Day 15 required/forbidden markers and repeat-run Day 15 closure suite.
  - **Day 16 Closure Ratification:**
    - deterministic closure markers for capability-scoped mapping, strict rights enforcement, and unmap/remap lifecycle added in `user/paradigm/main.c`.
    - fail-closed Day 16 marker path added for map-path contract violations.
    - matrix gate extended with Day 16 required/forbidden markers and repeat-run Day 16 closure suite.
  - **Day 17 Closure Ratification:**
    - deterministic closure markers for IRQ-safe lock semantics, stack canary integrity, and spurious IRQ accounting added in `kernel/main.c`.
    - fail-closed Day 17 marker path added for hardening contract violations.
    - matrix gate extended with Day 17 required/forbidden markers and repeat-run Day 17 closure suite.
  - **Day 18 Closure Ratification:**
    - deterministic closure markers for ELF header validation, ELF loader contract, and C-daemon bootstrap added in `kernel/elf.c` and `user/paradigm/main.c`.
    - fail-closed Day 18 marker path added for ELF/bootstrap contract violations.
    - matrix gate extended with Day 18 required/forbidden markers and repeat-run Day 18 closure suite.
  - **Day 19 Closure Ratification:**
    - mode-mask contract hardened via `CAP_MODE_VALID_MASK` and strict `CAP_MODE_ALL` narrowing in shared/kernel capability headers.
    - fail-closed mode-mask validation added in `cap_identity_create(...)`, `cap_mint(...)`, and `SYS_CAP_MINT`.
    - deterministic Day 19 closure markers and forbidden `[DAY19-FAIL]` path added with matrix and repeat-run closure suite coverage.
  - **Day 20 Closure Ratification:**
    - hardened lattice create topology checks and attach/detach boundary validation in syscall path.
    - hardened process lattice-attach path with overlap/duplicate rejection.
    - deterministic Day 20 closure markers and forbidden `[DAY20-FAIL]` path added with matrix and repeat-run closure suite coverage.
  - **Day 21 Closure Ratification:**
    - hardened Fate read path to require explicit auditor read rights and fail-closed on copy-count mismatches.
    - added deterministic Day 21 audit integrity markers and forbidden `[DAY21-FAIL]` path.
    - matrix gate extended with Day 21 required/forbidden markers plus repeat-run Day 21 closure suite.
  - **Day 22 Closure Ratification:**
    - added deterministic Day 22 lineage/revocation closure markers in kernel self-tests.
    - added explicit forbidden `[DAY22-FAIL]` path for recursive/deep-derivation regressions.
    - matrix gate extended with Day 22 required/forbidden markers plus repeat-run Day 22 closure suite.
  - **Day 23 Closure Ratification:**
    - added deterministic Day 23 allocator closure marker in kernel self-tests.
    - added explicit forbidden `[DAY23-FAIL]` path for allocator contract regressions.
    - matrix gate extended with Day 23 required/forbidden markers plus repeat-run Day 23 closure suite.
  - **Day 24 Closure Ratification:**
    - added deterministic Day 24 PMM/Law9/Ocular closure markers in kernel self-tests.
    - added explicit forbidden `[DAY24-FAIL]` path for Day 24 contract regressions.
    - matrix gate extended with Day 24 required/forbidden markers plus repeat-run Day 24 closure suite.
  - **Day 25 Closure Ratification:**
    - hardened `vmm_switch` with fail-closed PCID/mode/alignment checks.
    - added deterministic PCID free-path TLB scrub contract and Day 25 closure markers in kernel self-tests.
    - matrix gate extended with Day 25 required/forbidden markers plus repeat-run Day 25 closure suite.
  - **Day 26 Closure Ratification:**
    - added deterministic Day 26 Law 6 markers for prismatic substrate, Void Wall, and attunement path.
    - added explicit forbidden `[DAY26-FAIL]` path and repeat-run Day 26 closure suite.
    - hardened kernel lattice fault-window/index validation in `lattice_handle_fault`.
  - **Day 27 Closure Ratification:**
    - added deterministic Day 27 markers for syscall boundary hardening and strict foundation probes.
    - added explicit forbidden `[DAY27-FAIL]` path and repeat-run Day 27 closure suite.
    - elevated Day 27 boundary/strict evidence from generic logs to closure-gated runtime contracts.
- Product boundary:
  - BSP-only ESAK profile is explicitly ratified for this closure (`[TEST] ESAK IPI profile: BSP_ONLY`).
  - Day 12 closure now requires deterministic matrix markers for fault isolation, rendezvous contract, reaper lifecycle, and process annihilation.
  - Day 13 closure now requires deterministic matrix markers for extended-state initialization/context isolation and absence of Day 13 failure markers.
  - Day 14 closure now requires deterministic lifecycle syscall markers and absence of Day 14 failure markers.
  - Day 15 closure now requires deterministic genesis bridge markers and absence of Day 15 failure markers.
  - Day 16 closure now requires deterministic map/unmap contract markers and absence of Day 16 failure markers.
  - Day 17 closure now requires deterministic hardening markers and absence of Day 17 failure markers.
  - Day 18 closure now requires deterministic ELF/bootstrap markers and absence of Day 18 failure markers.
  - Day 19 closure now requires deterministic Conditional Rune markers and absence of Day 19 failure markers.
  - Day 20 closure now requires deterministic Lattice Bridge markers and absence of Day 20 failure markers.
  - Day 21 closure now requires deterministic Fatal Forensics markers and absence of Day 21 failure markers.
  - Day 22 closure now requires deterministic Derivation Trees markers and absence of Day 22 failure markers.
  - Day 23 closure now requires deterministic Foundation Allocator markers and absence of Day 23 failure markers.
  - Day 24 closure now requires deterministic Foundation Hardening/Ocular markers and absence of Day 24 failure markers.
  - Day 25 closure now requires deterministic PCID colorization/scrub/secure-context markers and absence of Day 25 failure markers.
  - Day 26 closure now requires deterministic Law 6 substrate/Void Wall/attunement markers and absence of Day 26 failure markers.
  - Day 27 closure now requires deterministic boundary/strict foundation markers and absence of Day 27 failure markers.
