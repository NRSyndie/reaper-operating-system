# Reaper-OS Release Consistency Checklist

Use this checklist for every implementation day before declaring completion.

## 1. Build and Runtime Verification
- [ ] `make -C user`
- [ ] `make -C kernel`
- [ ] `make -C kernel iso`
- [ ] `make -C kernel verify_matrix` (when Law 2/Fate runtime paths are in scope)
- [ ] Headless boot validation completed (`qemu-system-x86_64 ... -display none -serial file:...`)
- [ ] Required runtime markers captured in serial logs.

## 2. Day Report Completion
- [ ] Created/updated `docs/reports/dayXX_final_report.md`.
- [ ] Created/updated `docs/development_log/dayXX_checklist.md` with checked completion items.
- [ ] Included:
  - [ ] What changed
  - [ ] Why it changed
  - [ ] Test matrix and outcomes
  - [ ] Known limitations or follow-up risks

## 3. Rolling Report and Roadmap Sync
- [ ] Added day summary block to `docs/reports.md`.
- [ ] Added/updated day status row in `docs/development_log/TODO.rst`.
- [ ] Updated `Current Phase` in `docs/development_log/TODO.rst`.
- [ ] Updated strategic notes in `docs/development_log/epoch_two_plan.md` and/or `docs/development_log/epoch_three_plan.md` when scope/status changed.

## 4. Version Log Entry
- [ ] Added new version row in `docs/development_log/versions.rst`.
- [ ] Confirmed:
  - [ ] Date is accurate.
  - [ ] Version token increments (`0,A,0000NN`).
  - [ ] `Files Changed` includes all touched implementation + doc files.
  - [ ] Test summary references concrete outcomes/log paths.

## 5. Evidence and Traceability
- [ ] Runtime log file paths are real and inspectable.
- [ ] Negative checks (absence of failure markers) documented when relevant.
- [ ] Claims in reports match observable serial/build output.

## 6. Architecture Migration Gates (Epoch III Final-Product Track)
- [ ] Execution-envelope changes preserve existing syscall ABI behavior.
- [ ] Transition pipeline path (`compile -> verify -> apply -> attest`) is used where scope requires it.
- [ ] Compatibility shim behavior is documented for any retained legacy path.
- [ ] Rollback path is defined and tested for the current migration slice.
- [ ] Kill criteria review completed (determinism, compatibility, operability).

## 7. Syscall Gate Final-Product Test Gates
- [ ] Strategy reviewed and applied: `docs/components/syscalls/syscall_gate_testing_strategy.md`
- [ ] Syscall gate build/static invariants passed.
- [ ] Syscall gate boot self-tests passed.
- [ ] Syscall ABI/validator contract tests passed.
- [ ] Syscall integration tests (Paradigm + capability/memory/lattice/fate flows) passed.
- [ ] Syscall concurrency/SMP tests passed.
- [ ] Syscall security regression tests passed.
- [ ] Syscall performance budget checks passed.
- [ ] Required syscall gate runtime markers present in serial logs.
