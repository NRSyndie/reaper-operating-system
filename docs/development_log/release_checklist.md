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
- [ ] Envelope runtime markers observed in serial logs (`[ENV_COMPILE]`, `[ENV_VERIFY]`, `[ENV_APPLY]`, `[ENV_ATTEST]`).
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

## 8. ESAK Scheduler Hardening Gates
- [ ] `CAP_TYPE_SCHED_AUTH_ROOT` and `CAP_TYPE_SCHED_AUTH_THREAD` contracts validated.
- [ ] No ambient non-system enqueue path remains.
- [ ] Root/thread ceiling derivation probes pass.
- [ ] Deterministic weighted RR probe passes.
- [ ] Atomic process-budget integrity probe passes.
- [ ] Revocation immediate dequeue probe passes.
- [ ] Cross-mode scheduling rejection probe passes.
- [ ] Scheduler metrics ABI (`SYS_SCHED_METRICS`) includes expanded ESAK counters.
- [ ] Fate-compatible scheduler security events are observable (deny/revoke/budget paths).
- [ ] Product profile is explicit (`[TEST] ESAK IPI profile: BSP_ONLY` or validated SMP IPI activation evidence).

## 9. Day 12 Closure Ratification Gates
- [ ] Day 12 closure contract reviewed: `docs/components/day12/day12_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 12 Fault Isolation: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 12 Rendezvous Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 12 Reaper Lifecycle: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 12 Process Annihilation: SUCCESS.` present.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 10. Day 13 Closure Ratification Gates
- [ ] Day 13 closure contract reviewed: `docs/components/day13/day13_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 13 Extended-State Init: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 13 Context Preservation: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 13 Cross-Thread FPU Isolation: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 13 Crucible Stability: SUCCESS.` present.
- [ ] Forbidden marker `[DAY13-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day13_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 11. Day 14 Closure Ratification Gates
- [ ] Day 14 closure contract reviewed: `docs/components/day14/day14_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 14 Wait Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 14 Yield Gate: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 14 Lifecycle ABI Surface: SUCCESS.` present.
- [ ] Runtime marker `PARADIGM: Lifecycle gate probe PASS.` present.
- [ ] Forbidden marker `[DAY14-FAIL]` absent from matrix serial logs.
- [ ] Forbidden marker `PARADIGM: Lifecycle gate probe FAIL.` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day14_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 12. Day 15 Closure Ratification Gates
- [ ] Day 15 closure contract reviewed: `docs/components/day15/day15_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 15 Genesis Module Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 15 Genesis Capability Injection: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 15 Bootinfo Bridge: SUCCESS.` present.
- [ ] Runtime marker `PARADIGM: Genesis bridge probe PASS.` present.
- [ ] Forbidden marker `[DAY15-FAIL]` absent from matrix serial logs.
- [ ] Forbidden marker `PARADIGM: Genesis bridge probe FAIL.` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day15_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 13. Day 16 Closure Ratification Gates
- [ ] Day 16 closure contract reviewed: `docs/components/day16/day16_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 16 Capability-Scoped Mapping: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 16 Strict Rights Enforcement: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 16 Unmap/Remap Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY16-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day16_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 14. Day 17 Closure Ratification Gates
- [ ] Day 17 closure contract reviewed: `docs/components/day17/day17_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 17 IRQ-Safe Spinlocks: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 17 Stack Canary Guard: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 17 Spurious IRQ Filter: SUCCESS.` present.
- [ ] Forbidden marker `[DAY17-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day17_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 15. Day 18 Closure Ratification Gates
- [ ] Day 18 closure contract reviewed: `docs/components/day18/day18_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 18 ELF Header Validation: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 18 ELF Loader Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 18 Paradigm C Daemon Bootstrap: SUCCESS.` present.
- [ ] Forbidden marker `[DAY18-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day18_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 16. Day 19 Closure Ratification Gates
- [ ] Day 19 closure contract reviewed: `docs/components/day19/day19_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 19 Mode Mask Validation: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 19 Conditional Runes: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 19 Mint Monotonicity: SUCCESS.` present.
- [ ] Forbidden marker `[DAY19-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day19_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 17. Day 20 Closure Ratification Gates
- [ ] Day 20 closure contract reviewed: `docs/components/day20/day20_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 20 Lattice Create Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 20 Lattice Rights Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 20 Lattice Lifecycle Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY20-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day20_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 18. Day 21 Closure Ratification Gates
- [ ] Day 21 closure contract reviewed: `docs/components/day21/day21_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 21 Auditor Access Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 21 Fate Integrity Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 21 Fault Forensics Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY21-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day21_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 19. Day 22 Closure Ratification Gates
- [ ] Day 22 closure contract reviewed: `docs/components/day22/day22_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 22 Recursive Revocation Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 22 Deep Derivation Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY22-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day22_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 20. Day 23 Closure Ratification Gates
- [ ] Day 23 closure contract reviewed: `docs/components/day23/day23_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 23 Foundation Allocator Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY23-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day23_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 21. Day 24 Closure Ratification Gates
- [ ] Day 24 closure contract reviewed: `docs/components/day24/day24_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 24 Foundation Hardening Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 24 Ocular Projection Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY24-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day24_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 22. Day 25 Closure Ratification Gates
- [ ] Day 25 closure contract reviewed: `docs/components/day25/day25_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 25 PCID Partition Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 25 TLB Scrub Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 25 Secure Context Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY25-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day25_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 23. Day 26 Closure Ratification Gates
- [ ] Day 26 closure contract reviewed: `docs/components/day26/day26_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 26 Prismatic Substrate Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 26 Void Wall Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 26 Attunement Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY26-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day26_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.

## 24. Day 27 Closure Ratification Gates
- [ ] Day 27 closure contract reviewed: `docs/components/day27/day27_closure_contract.md`
- [ ] Runtime marker `[TEST] Day 27 Boundary Hardening Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 27 Strict Foundation Contract: SUCCESS.` present.
- [ ] Runtime marker `[TEST] Day 27 Syscall Rejection Contract: SUCCESS.` present.
- [ ] Forbidden marker `[DAY27-FAIL]` absent from matrix serial logs.
- [ ] Repeat-run closure gate passes: `./tools/run_day27_closure_suite.sh`.
- [ ] Vision/Security/Performance review artifacts captured in day closure report.
