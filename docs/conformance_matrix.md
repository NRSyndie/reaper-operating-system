# Reaper-OS Conformance Matrix

This document tracks the mapping between high-level architectural policies (Laws), their enforcement points in the system, and the evidence (markers/tests) proving their compliance.

## Enforcement Owners
- **kernel_enforced**: Hard-coded in ReaperCore; bypass is impossible without kernel compromise.
- **daemon_enforced**: Enforced by a privileged daemon (e.g., Paradigm, Aegis); bypass requires compromising the daemon.
- **deferred**: Policy exists but automated enforcement is not yet implemented.

## Conformance Table

| Law / Policy | Enforcement Owner | Enforcement Point | Evidence (Marker/Test) |
| :--- | :--- | :--- | :--- |
| **Occupant Reality Isolation** (Occupants never know the raw system mode) | kernel_enforced | `kernel/syscall.c` (SYS_MODE_QUERY) | `PARADIGM: Reality is CASUAL` (PID 1 truth) + redaction for PID > 1 |
| **Atomic Entry Pipeline** (Transitions follow compile->verify->apply->attest) | kernel_enforced | `kernel/entry.c` (entry_pipeline_run) | `[ENTRY_COMPILE]`, `[ENTRY_VERIFY]`, `[ENTRY_APPLY]`, `[ENTRY_ATTEST]` |
| **Epoch-Aware Entry Leases** (Occupants must hold a fresh lease for the current security epoch) | kernel_enforced | `kernel/scheduler.c` (scheduler_lease_valid) | `[ENTRY_REJECT] EPOCH_STALE` |
| **Mode-Bounded Scheduling** (Threads cannot be dispatched in the wrong mode) | kernel_enforced | `kernel/scheduler.c` (dispatch loop) | `[ENTRY_REJECT] MODE_MISMATCH` |
| **Zero-Residue Syscall Entry** (Registers scrubbed on transition to kernel) | kernel_enforced | `kernel/interrupts.s` (syscall_entry) | `[TEST] Syscall Gate security probes` |
| **Universal Capability Enforcement** (No access without explicit capability) | kernel_enforced | `kernel/capability.c` (cap_lookup) | `[TEST] Capability system redesign validation` |
| **Recursive Revocation** (Revoking a capability annihilates all derived descendants) | kernel_enforced | `kernel/capability.c` (cap_revoke) | `[TEST] Recursive Revocation: SUCCESS` |
| **Day 22 Derivation Trees Closure Contract** (Recursive revocation + deep derivation invalidation are closure-gated) | kernel_enforced | `kernel/main.c` lineage probes + `kernel/capability.c` revoke path | `[TEST] Day 22 Recursive Revocation Contract: SUCCESS.` + `[TEST] Day 22 Deep Derivation Contract: SUCCESS.` + absence of `[DAY22-FAIL]` |
| **Day 23 Foundation Allocator Closure Contract** (Allocator hardening invariants are closure-gated) | kernel_enforced | `kernel/main.c` (`test_slab_allocator`) | `[TEST] Day 23 Foundation Allocator Contract: SUCCESS.` + absence of `[DAY23-FAIL]` |
| **Day 24 Foundation Hardening + Ocular Closure Contract** (PMM/Law9/Ocular invariants are closure-gated) | kernel_enforced | `kernel/main.c` (`test_day24_closure_contracts`), `kernel/ocular.c` (`ocular_is_ready`) | `[TEST] Day 24 Foundation Hardening Contract: SUCCESS.` + `[TEST] Day 24 Ocular Projection Contract: SUCCESS.` + absence of `[DAY24-FAIL]` |
| **Law 2: Staged Shadow Mapping** (Page tables must be linked via strict capability sequence) | kernel_enforced | `kernel/syscall.c` (syscall_map_execute) | `PARADIGM: Law 2 strict negative probes passed` |
| **Law 6: Lattice Attunement** (Asynchronous doorbell IPC requires explicit crystal strike) | kernel_enforced | `kernel/lattice.c` (lattice_attune) | `PARADIGM: Lattice Attunement SUCCESS` |
| **Temporal Scouring** (Memory is proactively bleached on every Reality Shift) | kernel_enforced | `kernel/mode.c` (env_apply_transition -> ocular_bleach) | `[LAW9] Temporal scouring active` |
| **Day 12 Fault Isolation** (Ring 3 faults terminate offender, not kernel) | kernel_enforced | `kernel/idt.c` (`isr_handler` user-fault path) | `[TEST] Day 12 Fault Isolation: SUCCESS.` |
| **Day 12 Rendezvous Contract** (Endpoint capability-gated synchronous handoff) | kernel_enforced | `kernel/syscall.c` (`SYS_CAP_INVOKE` endpoint path) | `[TEST] Day 12 Rendezvous Contract: SUCCESS.` |
| **Day 12 Reaper Lifecycle** (Exited threads transition through zombie queue and bounded reap) | kernel_enforced | `kernel/thread.c`, `kernel/scheduler.c` | `[TEST] Day 12 Reaper Lifecycle: SUCCESS.` |
| **Day 12 Process Annihilation** (Last-thread teardown releases process address space safely) | kernel_enforced | `kernel/process.c`, `kernel/vmm.c` | `[TEST] Day 12 Process Annihilation: SUCCESS.` |
| **Day 13 Extended-State Init** (CPU extended-state path must initialize to supported mode) | kernel_enforced | `kernel/cpu.c` (`cpu_init_extended_state`) | `[TEST] Day 13 Extended-State Init: SUCCESS.` |
| **Day 13 Context Preservation** (Thread switch preserves FPU/SSE state via save/restore) | kernel_enforced | `kernel/interrupts.s` (`context_switch`) | `[TEST] Day 13 Context Preservation: SUCCESS.` |
| **Day 13 Cross-Thread FPU Isolation** (No cross-thread SSE/FPU contamination) | kernel_enforced | `kernel/main.c` (FPU crucible threads) | `[TEST] Day 13 Cross-Thread FPU Isolation: SUCCESS.` + absence of `[DAY13-FAIL]` |
| **Day 13 Crucible Stability** (FPU context remains stable under scheduler churn) | kernel_enforced | `kernel/main.c`, `tools/run_day13_closure_suite.sh` | `[TEST] Day 13 Crucible Stability: SUCCESS.` |
| **Day 14 Yield Contract** (Yield is a valid lifecycle gate and returns success) | kernel_enforced | `kernel/syscall.c` (`SYS_YIELD`) + `kernel/thread.c` (`thread_yield`) | `[TEST] Day 14 Yield Gate: SUCCESS.` |
| **Day 14 Wait Contract** (Wait status is deterministic and fail-closed on invalid use) | kernel_enforced | `kernel/syscall.c` (`SYS_WAIT`) + `kernel/process.h` wait fields | `[TEST] Day 14 Wait Contract: SUCCESS.` |
| **Day 14 Lifecycle ABI Surface** (Lifecycle gate ops remain stable and observable) | kernel_enforced + daemon_enforced | `kernel/syscall.c`, `user/paradigm/main.c` | `[TEST] Day 14 Lifecycle ABI Surface: SUCCESS.` + `PARADIGM: Lifecycle gate probe PASS.` |
| **Day 15 Genesis Module Contract** (Genesis module handoff must exist before launch) | kernel_enforced | `kernel/genesis.c` (module lookup path) | `[TEST] Day 15 Genesis Module Contract: SUCCESS.` + absence of `[DAY15-FAIL]` |
| **Day 15 Genesis Capability Injection** (Root bridge capability setup must complete before user handoff) | kernel_enforced | `kernel/genesis.c` (cap mint/derive/inject path) | `[TEST] Day 15 Genesis Capability Injection: SUCCESS.` + absence of `[DAY15-FAIL]` |
| **Day 15 Bootinfo Bridge** (Boot info bridge is validated end-to-end in kernel and Paradigm) | kernel_enforced + daemon_enforced | `kernel/genesis.c`, `user/paradigm/main.c` | `[TEST] Day 15 Bootinfo Bridge: SUCCESS.` + `PARADIGM: Genesis bridge probe PASS.` |
| **Day 16 Capability-Scoped Mapping** (Address-space mutation requires explicit capability lineage) | kernel_enforced + daemon_enforced | `kernel/syscall.c` (`SYS_MAP`/`SYS_UNMAP`), `user/paradigm/main.c` | `[TEST] Day 16 Capability-Scoped Mapping: SUCCESS.` |
| **Day 16 Strict Rights Enforcement** (Strict map rights/shape violations are rejected fail-closed) | kernel_enforced + daemon_enforced | `kernel/syscall.c` (`syscall_map_execute`), `user/paradigm/main.c` strict negative probes | `[TEST] Day 16 Strict Rights Enforcement: SUCCESS.` + absence of `[DAY16-FAIL]` |
| **Day 16 Unmap/Remap Contract** (Leaf unmap semantics and deterministic remap lifecycle) | kernel_enforced + daemon_enforced | `kernel/syscall.c` (`SYS_UNMAP` path), `user/paradigm/main.c` leaf unmap/remap probe | `[TEST] Day 16 Unmap/Remap Contract: SUCCESS.` + absence of `[DAY16-FAIL]` |
| **Day 17 IRQ-Safe Spinlocks** (Lock acquire/release preserves interrupt-state contract) | kernel_enforced | `kernel/include/utils.h`, `kernel/main.c` Day 17 closure probe | `[TEST] Day 17 IRQ-Safe Spinlocks: SUCCESS.` + absence of `[DAY17-FAIL]` |
| **Day 17 Stack Canary Guard** (Thread stack canary remains intact at guard boundary) | kernel_enforced | `kernel/thread.c`, `kernel/scheduler.c`, `kernel/main.c` closure probe | `[TEST] Day 17 Stack Canary Guard: SUCCESS.` + absence of `[DAY17-FAIL]` |
| **Day 17 Spurious IRQ Filter** (Spurious IRQ7/IRQ15 handling/accounting remains deterministic) | kernel_enforced | `kernel/interrupts.s`, `kernel/idt.c`, `kernel/main.c` closure probe | `[TEST] Day 17 Spurious IRQ Filter: SUCCESS.` + absence of `[DAY17-FAIL]` |
| **Day 18 ELF Header Validation** (ELF metadata must satisfy x86_64/Little-Endian/64-bit contract) | kernel_enforced | `kernel/elf.c` (`check_headers`) | `[TEST] Day 18 ELF Header Validation: SUCCESS.` + absence of `[DAY18-FAIL]` |
| **Day 18 ELF Loader Contract** (Loadable segments are mapped safely with fail-closed behavior) | kernel_enforced | `kernel/elf.c` (`elf_load`) | `[TEST] Day 18 ELF Loader Contract: SUCCESS.` + absence of `[DAY18-FAIL]` |
| **Day 18 C Daemon Bootstrap** (C-based Paradigm daemon reaches runtime entrypoint) | daemon_enforced + kernel_enforced | `user/paradigm/main.c`, `kernel/genesis.c` launch path | `[TEST] Day 18 Paradigm C Daemon Bootstrap: SUCCESS.` |
| **Day 19 Conditional Runes Contract** (Mode masks are valid, fail-closed, and derivation remains monotonic) | kernel_enforced | `kernel/capability.c`, `kernel/syscall.c`, `kernel/main.c` | `[TEST] Day 19 Mode Mask Validation: SUCCESS.` + `[TEST] Day 19 Conditional Runes: SUCCESS.` + `[TEST] Day 19 Mint Monotonicity: SUCCESS.` + absence of `[DAY19-FAIL]` |
| **Day 20 Lattice Bridge Contract** (Lattice create/rights/lifecycle invariants are fail-closed and deterministic) | kernel_enforced + daemon_enforced | `kernel/syscall.c`, `kernel/process.c`, `user/paradigm/main.c` | `[TEST] Day 20 Lattice Create Contract: SUCCESS.` + `[TEST] Day 20 Lattice Rights Contract: SUCCESS.` + `[TEST] Day 20 Lattice Lifecycle Contract: SUCCESS.` + absence of `[DAY20-FAIL]` |
| **Day 21 Fatal Forensics Contract** (Auditor-gated Fate reads and forensic integrity checks are fail-closed and deterministic) | kernel_enforced + daemon_enforced | `kernel/syscall.c`, `user/paradigm/main.c` | `[TEST] Day 21 Auditor Access Contract: SUCCESS.` + `[TEST] Day 21 Fate Integrity Contract: SUCCESS.` + `[TEST] Day 21 Fault Forensics Contract: SUCCESS.` + absence of `[DAY21-FAIL]` |

## Verification Procedure
Conformance is verified via the **Law 2 + Fate Matrix** harness:
`tools/run_law2_fate_matrix.sh`

Every `kernel_enforced` law MUST have a corresponding negative probe (intentional violation) in `user/paradigm/main.c` or a kernel self-test in `kernel/main.c` that triggers a deterministic rejection marker.
