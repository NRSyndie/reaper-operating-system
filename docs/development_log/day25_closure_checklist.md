# Day 25 Closure Checklist

## Build and Runtime Verification
- [x] `make -C user`
- [x] `make -C kernel`
- [x] `make -C kernel iso`
- [x] `make -C kernel verify_matrix`
- [x] `./tools/run_day25_closure_suite.sh`
- [x] Required Day 25 markers present in all matrix logs.
- [x] Forbidden Day 25 markers absent in all matrix logs.

## Implementation Deliverables
- [x] Hardened `vmm_switch` with fail-closed PCID/mode/alignment validation.
- [x] Added deterministic TLB context scrub on `pcid_free` before reuse.
- [x] Integrated secure transition context (`PCID_KERNEL_SECURE`) into mode-apply path.
- [x] Added deterministic Day 25 closure markers and fail marker path (`[DAY25-FAIL]`).
- [x] Extended matrix required/forbidden markers for Day 25 closure.
- [x] Added Day 25 closure suite script.

## Contract and Reviews
- [x] Vision review completed.
- [x] Security review completed.
- [x] Performance review completed.

## Documentation Sync
- [x] Updated `docs/components/day25/day25_closure_contract.md`
- [x] Updated `docs/reports/day25_closure_report.md`
- [x] Updated `docs/reports.md`
- [x] Updated `docs/development_log/TODO.rst`
- [x] Updated `docs/development_log/epoch_three_plan.md`
- [x] Updated `docs/development_log/release_checklist.md`
- [x] Updated `docs/conformance_matrix.md`
- [x] Updated `docs/development_log/versions.rst`
