# Zero-Residue Policy (Epoch III Finalization Baseline)

## Purpose
Define a single, auditable policy for data remanence control across syscall boundaries, memory lifecycle, and context transitions.

## Scope
This policy applies to:
- syscall entry/exit register exposure boundaries
- physical frame allocation/free paths
- thread/process context handoff surfaces
- mode/color transition boundaries where residue can cross trust domains

## Enforced Baseline (Current)
1. **Syscall boundary scrubbing**
   - syscall return path clears non-return GPR state before returning to user mode.
2. **Frame lifecycle clearing**
   - PMM zeroes or scrubs frame contents on allocation/free according to security color and epoch rules.
3. **Temporal scouring**
   - epoch mismatch triggers `hyper_scrub`; same-epoch allocation still zeroes.
4. **Extended state isolation**
   - per-thread extended state save/restore is active.

## Epoch III Day 43 Freeze Decisions
1. **Fail-closed over best-effort**
   - if residue-invariant preconditions are unknown, reject/stop rather than continue silently.
2. **Cross-color hardening target**
   - full cross-color context scrub policy remains required implementation scope.
3. **Deterministic observability**
   - residue policy transitions and failures must be marker-visible in serial logs.
4. **No policy weakening for performance**
   - performance tuning is allowed only after residue invariants are preserved.

## Required Markers
- Existing subsystem markers (`[LAW9]`, boundary probes) remain mandatory.
- New PMM hardening markers (`[PMM-PROFILE]`, `[PMM-AUDIT]`, `[PMM-QUAR]`, `[PMM-FAIL]`) are part of incident traceability.

## Deferred Implementation Items (Epoch III)
- explicit cross-color context scrub enforcement mechanism
- scheduler integration points for zero-residue on policy-driven quantum transitions
- expanded runtime matrix checks for residue invariants under mixed mode workloads

## Validation Requirements
- build gates: `make -C user`, `make -C kernel`, `make -C kernel iso`
- runtime gate: `make -C kernel verify_matrix`
- failures must produce actionable marker evidence and be recorded in daily report artifacts
