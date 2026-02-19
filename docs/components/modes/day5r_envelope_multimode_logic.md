# Day 5R: Multi-Mode Envelope Logic (Final-Product Track)

## Objective
Recast the original Day 5 multi-mode logic into execution-envelope terms so mode transitions, capability constraints, scheduler behavior, and Fate evidence are enforced through one deterministic pipeline.

## Scope (Phase 2 Bridge)
- Define how legacy modes map to envelope classes.
- Define envelope-aware transition legality rules.
- Define capability/scheduler binding requirements.
- Define required attestation markers for each transition path.

## Legacy-to-Envelope Mapping
- `CASUAL` -> `ENV_CLASS_BASELINE`
- `SECURE` -> `ENV_CLASS_DEFENSIVE`
- `LOCKDOWN` -> `ENV_CLASS_FAIL_CLOSED`
- `GHOST` -> `ENV_CLASS_EPHEMERAL`

This mapping preserves current semantics while allowing backend selection (MPK preferred, PCID fallback, optional VMFUNC tier).

## Transition Contract
Every transition follows:
1. `compile_envelope(from, to, reason, authority)`
2. `verify_envelope(compiled)` (legality + authority + capability constraints)
3. `apply_envelope(compiled)` (backend-specific hardware apply path)
4. `attest_envelope(compiled, result)` (deterministic evidence emission)

## Required Invariants
- Illegal legacy paths remain illegal under envelope mapping.
- Capability monotonicity remains fail-closed during transitions.
- Scheduler cannot run a task under an envelope class it is not bound to.
- Fate projection reflects every accepted/rejected transition emitted by attestation.

## Day 47 Deliverables
- Add envelope transition matrix doc section to `mode` component docs.
- Define kernel marker schema for envelope transition attestation.
- Add Paradigm probes for envelope transition rejection and acceptance visibility.
- Keep ABI-compatible mode/syscall interfaces until explicit cutover.

## Acceptance Gates
- Build parity: `make -C user`, `make -C kernel`, `make -C kernel iso`.
- Runtime parity: `make -C kernel verify_matrix` pass with no forbidden markers.
- Audit parity: existing Fate-read flows still return deterministic transition evidence.
