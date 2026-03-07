# Reaper-OS Syscall Gate Testing Strategy (Final-Product)

This strategy defines the minimum required test coverage for the syscall gate redesign and ABI freeze.

Related scheduler validation baseline:

- `docs/components/scheduler/esak_logging_and_validation.md`

## 1. Scope

The strategy covers:

- syscall entry/exit path (`SYSCALL/SYSRET` gate)
- syscall ABI decoding and validation
- dispatcher behavior and handler contract enforcement
- security rejection paths
- SMP correctness and performance budgets

Release closure is blocked until all required gates pass.

## 2. Required Test Layers

### A. Build and Static Invariants

- `make -C user`
- `make -C kernel`
- `make -C kernel iso`
- Zero new warnings in touched syscall-gate files.
- ABI consistency checks:
  - syscall register contract in headers matches dispatcher signature
  - syscall table metadata contains no missing handler/validator fields

### B. Boot Self-Tests (Kernel)

- Gate entry setup:
  - `MSR_LSTAR`, `MSR_STAR`, `MSR_SFMASK`, `MSR_EFER` programmed correctly.
- Kernel stack isolation:
  - syscall entry cannot execute on user-controlled stack.
- Return-path safety:
  - return RIP/RSP canonicality and masked RFLAGS invariants enforced.
- Dispatch invariants:
  - unknown syscall/opcode denied fail-closed.
- Metrics invariants:
  - counters update consistently for success/fail classes.

### C. ABI and Validation Contract Tests

- Argument decode contract for all supported arg slots.
- Pointer/range validation:
  - kernel-space pointer rejection
  - overflow and negative-length rejection
  - unmapped/readonly/writeonly boundary rejection
- Capability and rights validation:
  - wrong type rejected
  - missing rights rejected
  - mode-policy violations rejected
- Error model validation:
  - each required reject class maps to deterministic result code.

### D. Integration Tests (User + Kernel)

- Paradigm boot flow passes on current ABI surface.
- Boundary probes pass and expected safe failures are preserved.
- Capability lifecycle flows pass:
  - mint/copy/delete/revoke/retype
- Mapping flows pass:
  - strict map/unmap behavior + negative probes
  - Day 28 strict-adoption closure probes:
    - strict negative-path rejections
    - strict recursive chain link success
    - strict unmap/remap lifecycle stability
  - Day 29 strict-runtime closure probes:
    - strict unmap adoption in active boundary/runtime probes
    - strict map/unmap runtime behavior validated in matrix runs
  - Day 30 rejection-auditing closure probes:
    - fate hash-chain integrity remains valid
    - rejected transition evidence is visible in audit reads
    - fate result-code semantics are runtime-validated
- Lattice flows pass:
  - create/attach/attune/detach + listener access constraints
- Fate/audit flows pass:
  - fate read contracts and filtering.

### E. Concurrency and SMP Tests

- Multi-threaded syscall storm does not corrupt gate state.
- Per-CPU or per-thread gate state remains isolated.
- No deadlocks or scheduler starvation induced by syscall hot paths.

### F. Security Regression Tests

- Deny-by-default behavior for unknown/unimplemented calls.
- No kernel panic from malformed user input.
- Fail-closed behavior for frozen contracts (`SYS_AUDIT` until implemented).
- Zero-residue verification for register scrubbing on return path.

### G. Performance Tests

- Entry+dispatch latency microbench baseline captured.
- Regression budget per release slice:
  - no unbounded growth vs previous baseline
  - no obvious hot-path cache-thrashing changes
- Mapping and syscall-heavy workloads remain within accepted runtime envelope.

### H. Soak and Resilience

- Long-run runtime loop with sustained syscalls (no crashes, no stuck state).
- Fault-injection probes on validation-heavy paths remain fail-closed.

## 3. Coverage Requirements

All of the following are required:

- 100% syscall opcode coverage for the active ABI surface.
- 100% validator branch coverage for pointer/length/rights/mode checks.
- 100% deterministic error/reject class coverage.
- Positive-path and negative-path probes both present for every high-risk syscall.

## 4. Mandatory Runtime Markers

Serial logs must contain, at minimum:

- `PARADIGM: Boundary probes passed (safe failures confirmed).`
- `[TEST] Day 8 Gatekeeper redesign: SUCCESS.`
- `[TEST] VMM contract engine: SUCCESS.`
- `[TEST] Day 9 Void Gate redesign: SUCCESS.`
- `[TEST] Syscall Gate ABI v2: SUCCESS.`
- `[TEST] Syscall Gate validation invariants: SUCCESS.`
- `[TEST] Syscall Gate security probes: SUCCESS.`
- `[TEST] Syscall Gate SMP isolation: SUCCESS.`
- `[TEST] Syscall Gate performance budget: SUCCESS.`
- `PARADIGM: Envelope transition acceptance probe PASS.`
- `PARADIGM: Envelope transition rejection probe PASS.`
- `PARADIGM: Fate Strings include transition reject reason codes.`
- `[MODE_LEGACY_SHIM]`
- `[ENV_COMPILE]`
- `[ENV_VERIFY]`
- `[ENV_APPLY]`
- `[ENV_ATTEST]`
- `[TEST] No authority -> no execution`
- `[TEST] Root ceiling enforced`
- `[TEST] Thread explosion prevented`
- `[TEST] Revocation immediate dequeue`
- `[TEST] Cross-mode scheduling rejected`
- `[TEST] Deterministic RR rotation stable`
- `[TEST] SMP atomic budget integrity`
- `[TEST] ESAK IPI profile: BSP_ONLY`
- `[TEST] Day 28 Strict Adoption Contract: SUCCESS.`
- `[TEST] Day 28 Strict Negative Path Contract: SUCCESS.`
- `[TEST] Day 28 Strict Chain Contract: SUCCESS.`
- `[TEST] Day 29 Strict Unmap Adoption Contract: SUCCESS.`
- `[TEST] Day 29 Runtime Validation Contract: SUCCESS.`
- `[TEST] Day 29 Strict Path Runtime Contract: SUCCESS.`
- `[TEST] Day 30 Rejection Auditing Contract: SUCCESS.`
- `[TEST] Day 30 Fate Result-Code Contract: SUCCESS.`
- `[TEST] Day 30 Rejected Evidence Contract: SUCCESS.`

Serial logs must not contain:

- `[DAY28-FAIL]`
- `[DAY29-FAIL]`
- `[DAY30-FAIL]`

## 5. Final-Product Exit Criteria

The syscall gate is release-ready only when:

- all required layers in this strategy pass
- matrix runtime verification passes 3/3 runs
- Day 28 closure suite passes (`./tools/run_day28_closure_suite.sh`)
- Day 29 closure suite passes (`./tools/run_day29_closure_suite.sh`)
- Day 30 closure suite passes (`./tools/run_day30_closure_suite.sh`)
- required runtime markers are present and forbidden markers are absent
- docs/version logs are synchronized with exact commands and evidence paths
- no unresolved syscall-gate TODOs remain in code or release checklists
- ESAK scheduler authority/runtime markers are present in matrix logs for current release slice
