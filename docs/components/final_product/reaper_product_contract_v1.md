# Reaper Product Contract v1

This contract is the release authority for the final redesign path. A change is incomplete unless code, tests, runtime markers, and documentation all satisfy this document.

## 1. Vision/Security/Performance Non-Negotiables
- Capability-only authority: no ambient privilege path for scheduler, memory, or forensic operations.
- Fail-closed syscall behavior for malformed input, invalid authority, or partial state.
- Deterministic runtime evidence: required markers must appear and forbidden markers must not appear on repeated headless boots.
- Bounded operations only: no unbounded retry loops in kernel validation paths.
- Auditable closure: each shipped claim maps to test evidence and synchronized docs.

## 2. Product Invariants
- Syscall gate is the single sanctioned userspace entry path.
- Map/unmap lifecycle remains strict-rights and rollback-safe.
- Mode transition legality and rejection evidence remain recorded in Fate strings.
- Fault forensics must include real exception-path evidence (`#PF/#GP`) and remain queryable.
- Law 9 PMM temporal scouring observability remains visible through runtime marker/counters.

## 3. One-Way Release Gates
- Build gate: `make -C user`, `make -C kernel`, `make -C kernel iso`.
- Determinism gate: `make -C kernel verify_matrix` (3 runs).
- Security-critical closure suites: Day 28 through Day 34 closure scripts pass.
- Release-lock gate: 5 consecutive runtime matrix runs with required/forbidden marker checks.
- Documentation gate: roadmap/report/version/checklist/contract synchronization completed.

## 4. Required Test Surfaces
- ABI/contract tests for syscall gate operation and boundary validation.
- Negative security probes for malformed args, invalid caps, and boundary count/path handling.
- Forensic integrity tests for Fate record availability, filtering, and failure-marker absence.
- Stability tests across repeated headless boots using matrix harness.
- Performance budget tests with explicit regression thresholds.

## 5. Evidence Policy
- A closure claim is invalid without concrete command history and serial log artifacts.
- Required and forbidden marker outcomes must be retained in serial logs.
- Version entries must list touched files and exact validation commands.

## 6. Rollback and Cutover
- Compatibility mode is temporary and must have a scheduled hard removal milestone.
- No final-release declaration while compatibility-only behavior remains unverified.
- If any gate fails, rollback to previous known-good release candidate and reopen closure work.
