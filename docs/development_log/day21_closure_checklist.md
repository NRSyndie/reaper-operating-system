# Day 21 Closure Checklist

## Build and Runtime Verification
- [x] `make -C user`
- [x] `make -C kernel`
- [x] `make -C kernel iso`
- [x] `make -C kernel verify_matrix`
- [x] `./tools/run_day21_closure_suite.sh`
- [x] Required Day 21 markers present in all matrix logs.
- [x] Forbidden Day 21 markers absent in all matrix logs.

## Implementation Deliverables
- [x] Hardened `SYS_FATE_READ` to require Auditor `CAP_RIGHT_READ`.
- [x] Hardened `SYS_FATE_READ` to fail closed on kernel/user copy-count mismatch.
- [x] Added deterministic Day 21 closure markers and fail markers in Paradigm probes.
- [x] Added negative probes for non-auditor access and invalid read mode rejection.
- [x] Extended matrix required/forbidden markers for Day 21 closure.
- [x] Added Day 21 closure suite script.

## Contract and Reviews
- [x] Vision review completed.
- [x] Security review completed.
- [x] Performance review completed.

## Documentation Sync
- [x] Updated `docs/components/day21/day21_closure_contract.md`
- [x] Updated `docs/reports/day21_closure_report.md`
- [x] Updated `docs/reports.md`
- [x] Updated `docs/development_log/TODO.rst`
- [x] Updated `docs/development_log/epoch_three_plan.md`
- [x] Updated `docs/development_log/release_checklist.md`
- [x] Updated `docs/conformance_matrix.md`
- [x] Updated `docs/components/syscalls/syscall_contracts.md`
- [x] Updated `docs/development_log/versions.rst`
