# Epoch III Backlog: Security, Performance, Vision

## Objective
Execute the approved Epoch III strategy with security-first sequencing, measurable performance guardrails, and explicit vision continuity milestones.

## Workstream 1 — Security Contracts (Priority 0)
### 1.1 `SYS_AUDIT` semantics
- Define target identity model and delegation rules.
- Define capability requirements and monotonic authority constraints.
- Add syscall contract notes and ABI-facing error semantics.

**Acceptance gate**
- Contract doc updated and reviewed.
- Negative-path probes defined for unauthorized target access and malformed arguments.

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
### 2.1 Mode-weighted quantum (guardrailed)
- Implement weighted scheduling policy with deterministic fallback.
- Add fairness/starvation probe cases.

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
- Define minimal capability surfaces for Aegis/Archivist/Sage/Cipher/Sentinel.
- Document derivation boundaries and delegation flow.

**Acceptance gate**
- Authority maps documented with explicit least-privilege boundaries.

---

## Suggested Execution Milestones
1. **M1 (Security Contract Freeze):** `SYS_AUDIT` contract + zero-residue policy finalized.
2. **M2 (Security Runtime Validation):** new probes integrated; matrix expanded for security invariants.
3. **M3 (Deterministic Scheduler):** weighted quantum merged with fairness and regression evidence.
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
