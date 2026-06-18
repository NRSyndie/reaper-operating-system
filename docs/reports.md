# Reaper-OS Project Reports

## January 9, 2026: The Establishment of the Void and Sight

### Epoch I, Day 1: Physical Memory Audit
*   **What was changed:** 
    *   Implemented the `limine_memmap_request` in `kernel/main.c`.
    *   Created a diagnostic loop to iterate through the bootloader’s memory map.
    *   Upgraded the kernel’s `kprintf` function in `console.c` to support `%lx` (64-bit hex) and `%p` (pointers).
*   **Why it was changed:** 
    *   To establish a "Source of Truth" regarding available physical RAM before building managers. 
    *   `kprintf` was upgraded because debugging memory management is impossible without the ability to print 64-bit addresses.
*   **Test Results:** 
    *   Successfully identified 124 MB of usable RAM fragments.
    *   Verified that reserved regions (ACPI, Framebuffer) were correctly ignored.

### Epoch I, Day 2: Physical Memory Manager (PMM)
*   **What was changed:** 
    *   Created `kernel/pmm.c` and `kernel/include/pmm.h`.
    *   Implemented a **Flat Bitmap Ledger** for O(1) frame tracking.
    *   Implemented a **16-byte Frame Metadata Array** to store Security Colors and Ownership Tokens for every 4KB frame.
    *   Added a "Zero-Residue" policy (auto-zeroing on allocation).
*   **Why it was changed:** 
    *   To enforce the "Iron Gate" security model at the hardware level. The metadata array ensures every frame has a verifiable identity and security context.
    *   Auto-zeroing fulfills the "Voidborn" vision of preventing cross-reality data leakage.
*   **Test Results:** 
    *   PMM successfully "carved" 2MB of RAM for the metadata database.
    *   Test allocation of a `COLOR_SECURE` frame at `0x59000` was verified and successfully released.

### Epoch I, Day 3: Virtual Memory Manager (VMM)
*   **What was changed:** 
    *   Created `kernel/vmm.c` and `kernel/include/vmm.h`.
    *   Implemented 4-level x86_64 page table walking (PML4 to PT).
    *   Established **Higher-Half Isolation**, moving the kernel to `0xffffffff80000000`.
    *   Implemented **PML4 Cloning** to safely transition from the bootloader's memory space to the kernel's private tables.
    *   Created `vmm_self_test` suite and verified the `initcall` registration mechanism.
*   **Why it was changed:** 
    *   To enable the "Sight" system, ensuring the kernel is isolated from future user-space domains.
    *   Cloning was implemented after discovering that sparse manual mapping caused reboots; cloning ensures all bootloader-provided environment data remains accessible during the "Phase Shift."
### Epoch I, Day 5: PCID & INVPCID Integration
*   **What was changed:** 
    *   Implemented `kernel/pcid.c` and `kernel/include/pcid.h` for PCID management.
    *   Updated `vmm_switch` in `kernel/vmm.c` to support the 12-bit PCID field and the `NOFLUSH` (Bit 63) optimization.
    *   Implemented `kernel/cpu.c` with CPUID feature detection and Control Register accessors (`CR3`, `CR4`).
    *   Added `vmm_fork_pml4` to allow address space cloning for benchmarks.
    *   Implemented a 7-step PCID verification suite and a context-switch benchmark.
*   **Why it was changed:** 
    *   To eliminate the "Microkernel Tax." Standard context switches flush the entire TLB, wasting ~1000 cycles. PCID allows preserving translations across switches, a prerequisite for high-performance IPC in Epoch III.
*   **Test Results:** 
    *   **Allocator:** Verified unique ID generation and bitmap consistency.
    *   **VMM Switch:** Verified correct CR3 construction (`PML4 | PCID`).
    *   **Performance:** Established a baseline context switch cost of **~11,000 cycles** in the current emulation environment.
    *   **Hardware Fallback:** Graceful degradation verified on systems reporting no PCID support.

### Epoch I, Day 6: The Soul Forge (Slab Allocator)
*   **What was changed:**
    *   Implemented `kernel/slab.c` and `kernel/include/slab.h`.
    *   Created a mode-aware slab allocator with object segregation.
    *   Implemented "Bulk Annihilation" for instant memory reclamation on mode exit.
*   **Why it was changed:**
    *   To provide O(1) allocation for internal kernel objects.
    *   To enforce physical segregation of objects belonging to different security modes.
*   **Test Results:**
    *   [PASS] Verified O(1) allocation and reuse.
    *   [PASS] Verified that `slab_annihilate` zeroed memory for the target mode.

### Epoch I, Day 7: The Rune Loom (Capabilities)
*   **What was changed:**
    *   Implemented `kernel/capability.c` and `kernel/include/capability.h`.
    *   Defined the `capability_t` (Rune) and `cnode_t` structures.
    *   Implemented `cap_lookup`, `cap_insert`, and `cap_delete`.
*   **Why it was changed:**
    *   To move from identity-based security to capability-based security.
    *   To enable the "Car Key" model where possession of a handle is the sole authority.
*   **Test Results:**
    *   [PASS] Verified O(1) handle-to-object translation.
    *   [PASS] Verified "Explicit Destruction Rule" (no overwriting slots).

### Epoch I, Day 8: The Gatekeeper (IDT & Exceptions)
*   **What was changed:**
    *   Implemented `kernel/idt.c`, `kernel/gdt.c`, and `kernel/include/idt.h`.
    *   Set up 256 IDT entries and low-level ISR stubs in `kernel/interrupts.s`.
    *   Configured the TSS (Task State Segment) with a valid RSP0 for stack safety.
*   **Why it was changed:**
    *   To handle CPU exceptions (Page Faults, GPFs) gracefully.
    *   To prevent "Triple Faults" by providing a known-good stack for the kernel during transitions.
*   **Test Results:**
    *   [PASS] Verified `int $3` breakpoint handling.
    *   [PASS] Verified TSS stack initialization.

### Epoch I, Day 9: The Void Gate (Syscall & IPC)

*   **What was changed:**

    *   Implemented `kernel/syscall.c` and `kernel/include/syscall.h`.

    *   Configured MSRs (`LSTAR`, `STAR`, `SFMASK`) for `SYSCALL`/`SYSRET` support.

    *   Implemented `syscall_entry` assembly stub with `SWAPGS` for kernel stack isolation.

    *   Created the central `syscall_dispatcher`.

*   **Why it was changed:**

    *   To establish the primary interface between user-space and the kernel.

    *   `SWAPGS` ensures that the kernel never executes using a user-controlled stack, a vital security boundary.

*   **Test Results:**

    *   [PASS] Verified `SYS_VOID_LOG` (0) prints messages from simulated user contexts.

    *   [PASS] Verified `SYS_MODE_QUERY` (5) returns the correct global mode ID.



### Epoch I, Day 10: The Process Substrate (Scheduler)

*   **What was changed:**

    *   Implemented `kernel/thread.c`, `kernel/process.c`, and `kernel/scheduler.c`.

    *   Defined the **Soul** (`thread_t`) and **World** (`process_t`) structures.

    *   Implemented a **100Hz Round-Robin Scheduler**.

    *   Integrated PCID-aware "Phase Shifts" into context switching.

*   **Why it was changed:**

    *   To allow multiple execution streams to coexist. 

    *   The Round-Robin "Pulse" ensures no single thread can hog the CPU, enforcing the "Law of Time."

*   **Test Results:**

    *   [PASS] Intra-process switching: Verified interleaving of threads A and B.

    *   [PASS] Inter-process switching: Verified "Reality Shifts" between Process X and Process Y with PCID persistence.

    *   [PASS] Final-product closure addendum (2026-02-19): Per-CPU scheduler scaffolding, mode-aware scheduling gates, scrubbed teardown, and `SYS_SCHED_METRICS` user probe validated in 3/3 runtime matrix runs.



### Epoch I, Day 11: The Genesis Handshake (User Mode Leap)

*   **What was changed:**

    *   Implemented `user_mode_jump` using `IRETQ` to drop privilege to Ring 3.

    *   Upgraded `vmm_fork_pml4` to isolate the user-half (0-255) of the address space.

    *   Fixed `syscall_entry` to correctly map user registers to the C-ABI.

    *   Implemented per-thread kernel stacks for syscall stability.

*   **Why it was changed:**

    *   To enable the first true user-mode execution.

    *   Lower-half isolation prevents Limine's bootloader data (Supervisor-only) from causing Page Faults in user-space.

*   **Test Results:**

    *   [PASS] Successfully executed `dummy_code` at virtual address `0x400000`.

    *   [PASS] Verified that user-mode code can invoke kernel syscalls and receive return values.



### Epoch I, Day 12: Base Solidification (The Final Seal)
*   **What was changed:**
    *   Implemented **User-Mode Fault Isolation**: ISRs now terminate offending threads instead of panicking.
    *   Implemented **The Reaper**: A resource reclamation system for `THREAD_ZOMBIE` souls.
    *   Implemented **Synchronous IPC (Rendezvous)**: Added `CAP_TYPE_ENDPOINT` and `SYS_CAP_INVOKE` matching logic.
    *   Implemented **Process Annihilation**: Recursive page table walking to free entire address spaces on exit.
*   **Why it was changed:**
    *   To achieve a "Minimal Working Product" where the system is robust against crashes and can reclaim its own resources.
    *   IPC is the heartbeat of a microkernel; this rendezvous path is the foundation for all future daemons.
*   **Test Results (ULTIMATE INTEGRATION TEST):**
    *   [PASS] **Alpha Process** sent a message to **Omega Process** via Endpoint.
    *   [PASS] **Alpha Process** deliberately faulted; the kernel terminated it without panicking.
    *   [PASS] **The Reaper** purified Alpha's soul and annihilated its entire universe (physical frames returned to Ledger).
    *   [PASS] **Omega Process** and the rest of the system remained stable and running.

### Epoch I, Day 15: The Genesis Bridge
*   **What was changed:**
    *   Implemented `kernel/genesis.c` and `kernel/include/bootinfo.h`.
    *   Created the **Genesis Bridge** logic: Spawns the "Paradigm" process (PID 1) and injects the `GENESIS_CAP` (Infinite Authority) into its C-Space.
    *   Mapped the `boot_info` structure (containing Memory Map, HHDM offset, etc.) into Paradigm's address space at `0x1000`.
    *   Optimized the `Makefile` to avoid redundant ISO rebuilds.
    *   Cleaned up "Zero-Residue" violations (unused variables and functions) in `main.c`, `syscall.c`, `cpu.c`, and `idt.c`.
*   **Why it was changed:**
    *   To bridge the gap between the Voidborn Kernel and User Space. The kernel must hand over authority to Paradigm to "construct reality."
    *   The build system optimization saves significant time during the iterative testing loop.
*   **Test Results:**
    *   [PASS] **Paradigm Spawn:** Kernel successfully forked, allocated C-Space, and minted the Genesis Cap.
    *   [PASS] **Bridge Crossing:** Paradigm thread executed and accessed the mapped Boot Info region.
    *   [PASS] **Clean Build:** No warnings generated during compilation.

### Epoch I, Day 16: Authority over Memory
*   **What was changed:**
    *   Defined `CAP_TYPE_PML4` to represent authority over an address space.
    *   Implemented `SYS_MAP` (9) and `SYS_UNMAP` (10) syscalls.
    *   Upgraded `SYS_MAP` to strictly check that requested VMM flags (Write/Execute) are subsets of the Capability's rights.
    *   Modified `genesis.c` to inject a PML4 capability (Slot 2) and a free RAM capability (Slot 3) into Paradigm.
    *   Updated `main.c` to initialize the PIT timer (100Hz), enabling the scheduler's heartbeat which was previously dormant.
*   **Why it was changed:**
    *   To fulfill the "Voidborn" promise: The kernel provides the mechanism (Mapping), but the User (Paradigm) provides the policy and authority.
    *   Without `SYS_MAP`, Paradigm cannot grow, load ELF sections, or allocate heap.
*   **Test Results:**
    *   [PASS] **Capability Injection:** Paradigm correctly received PML4 and RAM caps.
    *   [PASS] **Self-Construction:** Paradigm successfully invoked `SYS_MAP` to map a physical frame to `0x20000000`.
    *   [PASS] **Verification:** Paradigm wrote a signature to the new page and read it back.
    *   [PASS] **Diagnostic:** `SYS_VOID_LOG` confirmed success from User Space.

### Epoch I, Day 17: Safety & Stability Finalization (The Diamond Seal)
*   **What was changed:**
    *   **IRQ-Safe Spinlocks:** Implemented `spinlock_irqsave` and `spinlock_irqrestore`. This pattern ensures that acquiring a lock disables local interrupts and restores the previous state on release, preventing deadlocks during interrupt handling.
    *   **The Abyss Sentry (Stack Canaries):** Implemented 64-bit magic values (`0x535441434B444541`) at the bottom of every managed kernel stack. The scheduler now verifies this canary on every context switch.
    *   **Phantom Filter (Spurious Handling):** Added assembly-level logic to `interrupts.s` to catch and correctly handle spurious IRQ 7 and IRQ 15 interrupts without panicking or corrupting PIC state.
    *   **Atomic Refactor:** Integrated locks into `PMM`, `SLAB`, `PCID`, and `MODE` subsystems to prepare for future SMP (Multi-core) support.
*   **Why it was changed:**
    *   To harden the kernel against the "Invisible Chaos" of race conditions and stack overflows before expanding the user-space surface area.
    *   To fix a critical bug in the timer ISR where stack unbalancing caused GPFs when returning to kernel threads.
*   **Test Results:**
    *   [PASS] **Spurious Silence:** No panics during intensive I/O or timer activity.
    *   [PASS] **Lock Integrity:** Verified that interrupt-driven re-entrancy does not hang the kernel.
    *   [PASS] **Canary Verification:** Confirmed that stack limits are respected during multitasking.

## Epoch II: The Architect's Vision (Alignment)

### Epoch II, Day 18: Paradigm Evolution (The First Form)
*   **What was changed:**
    *   Transitioned the **Paradigm** process from a minimal assembly stub to a full C-based System Daemon.
    *   Implemented a User-Space Build System (`user/Makefile`, `user/linker.ld`) to compile Paradigm as a separate ELF binary.
    *   Implemented a kernel-level **ELF64 Loader** in `kernel/elf.c` to parse and load Paradigm segments.
    *   Integrated Paradigm as a Limine Boot Module to solve the "Chicken and Egg" filesystem problem.
*   **Why it was changed:**
    *   To allow complex logic in User Space. Assembly is insufficient for "constructing reality."
    *   To validate the ABI (Application Binary Interface) between the kernel and C code (stack alignment, calling conventions).
*   **Test Results:**
    *   [PASS] **Loader:** Kernel correctly parsed the ELF header and loaded segments to `0x400000`.
    *   [PASS] **Execution:** Paradigm's `main()` function executed and successfully called `sys_log` to print "Hello from C-Space!".

### Epoch II, Day 19: Law 4 - Conditional Runes
*   **What was changed:**
    *   Implemented **Mode-Aware Capabilities** in `kernel/capability.c`.
    *   Added `allowed_modes` bitmask to `cap_identity_t`.
    *   Updated `cap_lookup` to return `NULL` (invisibility) if the capability's allowed mode does not match the system's current Reality.
    *   Updated `SYS_CAP_MINT` to allow restricting derived capabilities to specific modes.
*   **Why it was changed:**
    *   To implement the "Quantum Split" pillar. Different Realities must have strictly different available powers.
    *   This allows a process to hold powerful capabilities (like `PML4_WRITE`) that only "activate" when the system enters `MODE_SECURE`, preventing damage during `MODE_CASUAL`.
*   **Test Results:**
    *   [PASS] **Invisibility:** A `CASUAL`-only capability vanished when the system Phase Shifted to `SECURE`.
    *   [PASS] **Rematerialization:** The capability reappeared when the system shifted back to `CASUAL`.

### Epoch II, Day 20: Law 6 - The Lattice Bridge
*   **What was changed:**
    *   Implemented `CAP_TYPE_LATTICE` and `lattice_t` structure.
    *   Implemented `SYS_LATTICE_CREATE` (12) to allocate shared physical frames tracked by a capability.
    *   Updated `SYS_MAP` to accept `CAP_TYPE_LATTICE`, allowing multiple processes to map the same underlying physical frames into their own address spaces.
    *   Fixed a critical bug in `syscall_entry` where `RBP` was not preserved, causing user-space crashes after syscall returns.
*   **Why it was changed:**
    *   To solve the "Microkernel Tax." IPC via message passing (copying) is too slow for bulk data (e.g., video, audio, physics).
    *   Lattices provide zero-copy, shared-memory communication channels.
*   **Test Results:**
    *   [PASS] **Creation:** Paradigm successfully created a 2-page Lattice.
    *   [PASS] **Mapping:** Paradigm mapped the Lattice to `0x2000000`.
    *   [PASS] **Data Persistence:** Data written to the Lattice remained consistent across accesses.

### Epoch II, Day 21: Fatal Forensics (The Auditor)
*   **What was changed:**
    *   Implemented `CAP_TYPE_AUDITOR` to grant read-only access to the Fate String ledger.
    *   Implemented `SYS_FATE_READ` (14) to allow User Space to retrieve the system's immutable history.
    *   Secured the syscall to require a valid Auditor capability in the caller's C-Space.
    *   Implemented cryptographic verification logic in Paradigm to validate the "Hash Chain" of the retrieved records.
*   **Why it was changed:**
    *   To prove the "Fatal Forensics" pillar. The history of the system must be auditable by authorized user-space agents, not just the kernel.
    *   This enables "Trust but Verify" security architectures.
*   **Test Results:**
    *   [PASS] **Access Control:** `SYS_FATE_READ` failed without the Auditor Cap.
    *   [PASS] **Integrity:** Paradigm successfully retrieved and verified the SHA-256 hash chain of the kernel's recent mode transitions.

### Epoch II, Day 24: Foundation Hardening & Ocular Projection
*   **What was changed:**
    *   **PMM O(1) Allocation:** Replaced the O(N) bitmap scan with an O(1) free-stack integrated into `frame_metadata`.
    *   **Kmalloc Subsystem:** Implemented a bucket-based `kmalloc` and `kfree` system, optimizing kernel memory usage.
    *   **Cascading Revocation:** Refactored the capability system to recursively free "ghost" parent identities.
    *   **Resonance Pact (Doorbell IPC):** Implemented asynchronous signaling via Page Fault "Strikes" on non-present Lattice pages.
    *   **Ocular Projection Engine:** Created a hardware-isolated, asynchronous display system that projects user-space Lattices to the framebuffer during idle cycles.
    *   **The Great Bleaching:** Integrated proactive memory and framebuffer scrubbing (`hyper_scrub`) into the Mode Transition logic.
*   **Why it was changed:**
    *   To repay foundational debt and ensure the system meets its high-performance and extreme-isolation mandates.
    *   Ocular Projection and Doorbell IPC move beyond conventional OS design to provide a uniquely secure and efficient architecture.
*   **Test Results:**
    *   [PASS] **Build:** Kernel successfully compiled and linked with all new subsystems.
    *   [PASS] **PMM:** Verified $O(1)$ allocation timing in the stable test suite.
    *   [PASS] **Revocation:** Confirmed recursive cleanup of derivation trees.
    *   [PASS] **Display:** Ocular Engine initialized and detected the LFB.

### Epoch II, Day 22: Law 1 - Derivation Trees (The Snap)
*   **What was changed:**
    *   Implemented capability lineage-aware derivation graph semantics:
        *   `cap_identity_t` parent pointer and sibling-linked child list (`first_child`, `next_sibling`, `prev_sibling`)
        *   parent-epoch snapshot tracking for descendant liveness checks
    *   Implemented recursive revocation in capability core:
        *   `cap_revoke_tree(...)` recursively marks revoked subtree roots and descendants
        *   `cap_revoke(...)` performs epoch advance and lineage-locked subtree revoke
    *   Added runtime lineage validation probes in kernel self-tests:
        *   recursive revocation (`Parent -> Child -> Grandchild`)
        *   deep derivation invalidation (`A -> B -> C`)
*   **Why it was changed:**
    *   To enforce Law 1 authority hierarchy with deterministic root-level revocation semantics.
    *   To guarantee delegated authority cannot survive ancestor revocation.
*   **Test Results:**
    *   [PASS] **Recursive Revocation:** Parent/child/grandchild invalidated after root revoke.
    *   [PASS] **Deep Lineage:** Verified correctly across a 3-level derivation chain (A -> B -> C).

### Epoch II, Day 23: Foundation Hardening (Allocator Contract Reconciliation)
*   **What was changed:**
    *   Established allocator hardening baseline around slab policy invariants and `kmalloc` large-allocation fallback.
    *   This day had no standalone finalized report artifact in the historical ledger; closure ratification now binds it to concrete allocator runtime evidence.
*   **Why it was changed:**
    *   To eliminate chronology drift between historical implementation sequence and closure-grade enforceable evidence.
    *   To ensure foundational memory-hardening behavior is represented in the same governance model as adjacent days.
*   **Test Results:**
    *   [PASS] **Allocator Contract:** Kernel allocator validation probes passed in boot self-tests.
    *   [PASS] **Closure Gate:** Day 23 closure suite and matrix marker checks passed.

### Epoch II, Day 25: Law 5 - PCID Colorization (Reality Binding)
*   **What was changed:**
    *   **Mode-Partitioned PCID Ranges:** Divided the 12-bit PCID space (0-4095) into dedicated ranges for each security Reality (`CASUAL`, `SECURE`, `LOCKDOWN`, `GHOST`).
    *   **Specialized Kernel PCIDs:** Reserved `PCID_KERNEL` (0) for standard operations and `PCID_KERNEL_SECURE` (4095) for sensitive transitions and secure wipes.
    *   **PCID Colorization Validation:** Upgraded `vmm_switch` to strictly enforce that a process's PCID belongs to its assigned Mode's range, triggering a kernel panic on violation.
    *   **GHOST Flush Enforcement:** Hardcoded mandatory TLB flushes (`NOFLUSH=0`) for `GHOST` mode transitions to ensure zero-residue isolation.
    *   **VMM Metrics Integration:** Added PCID usage tracking (high-water marks, reuse counts) to `vmm_stats`.
*   **Why it was changed:**
    *   To prevent cross-Reality side-channel attacks (e.g., Spectre/Meltdown variants leveraging TLB state).
    *   To provide architectural "Air Gapping" between isolated address spaces without sacrificing the performance of PCID-optimized switching within a Reality.
*   **Test Results:**
    *   [PASS] **Partitioning:** Verified that `pcid_alloc` returns IDs within the correct Mode-specific bounds.
    *   [PASS] **Security Invariants:** Manual verification confirmed that attempting to switch to a mismatched PCID correctly triggers a system panic.
    *   [PASS] **Kernel Specialization:** Verified successful transitions between `PCID_KERNEL` and `PCID_KERNEL_SECURE` contexts.
    *   [PASS] **Stability:** Successfully handled "Reality Shifts" across all 5 modes without corruption.

### Epoch II, Day 26: Law 6 - Prismatic Lattices (Resonance)
*   **What was changed:**
    *   **Prismatic Lattice Substrate:** Implemented `lattice_t` with "Crystallization" physics.
    *   **Asymmetric Mapping:** Enforced unidirectional data flow (Source/Producer is RW, Echo/Consumer is RO).
    *   **The Void Wall (Implicit Sync):** Integrated Page Fault handling (`lattice_handle_fault`) to automatically block consumers attempting to read non-crystallized frames.
    *   **Lattice API:** Implemented `SYS_LATTICE_CREATE`, `SYS_LATTICE_ATTACH`, and `SYS_ATTUNE`.
    *   **Process Integration:** Added `lattice_attachment_t` to `process_t` to track virtual-to-physical reality bindings.
*   **Why it was changed:**
    *   To eliminate the "Syscall Tax" for data synchronization. Using the MMU as a synchronization primitive allows for passive, zero-copy communication.
    *   Asymmetry prevents consumers from corrupting producer state, fulfilling the "Unidirectional Physics" mandate of the Voidborn vision.
*   **Test Results:**
    *   [PASS] **Lattice Creation:** Paradigm successfully allocated shared physical substrate.
    *   [PASS] **Prismatic Attachment:** Verified correct registration of Source mappings at `0x20000000`.
    *   [PASS] **Attunement:** Verified that `SYS_ATTUNE` correctly advances the crystal index and satisfies the Void Wall.
    *   [PASS] **Shadow Mapping Integration:** Confirmed stable operation alongside Law 2 (Shadow Mapping) in User Space.

### Epoch II, Day 27: Syscall Boundary Hardening + Law 2 Strict Path (Foundation Pass)
*   **What was changed:**
    *   Added canonical syscall contract documentation in `docs/components/syscalls/syscall_contracts.md`.
    *   Hardened user-pointer handling paths in `kernel/syscall.c` through unified range validation helpers and copy wrappers.
    *   Introduced strict rollout path for mapping APIs:
        *   `SYS_MAP` now supports strict mode through `a4 bit0`.
        *   `SYS_UNMAP` accepts strict marker via `a2 bit0`.
    *   Added strict-map invariants for Law 2 groundwork:
        *   Parent must be writable `CAP_TYPE_PAGETABLE`.
        *   Child must be `CAP_TYPE_RAM` or `CAP_TYPE_PAGETABLE`.
        *   Non-leaf strict links require `USER|WRITABLE`.
        *   Strict mode rejects unknown user PTE bits.
    *   Added syscall observability counters and periodic diagnostics (`fault`, `invalid`, `perm`, map/unmap failure reasons, TLB flush count).
    *   Added userland strict wrappers in `user/lib/reaper.c` and `user/include/reaper.h`:
        *   `sys_map_strict(...)` (historical Day 27 wrapper; removed in Day 72 when strict path became default `sys_map(...)`)
        *   `sys_unmap_strict(...)` (historical Day 27 wrapper; removed in Day 72 when strict path became default `sys_unmap(...)`)
*   **Why it was changed:**
    *   To reduce kernel fault risk at the user-kernel boundary before further feature expansion.
    *   To introduce Law 2 hardening as an incremental rollout rather than a destabilizing all-at-once cutover.
    *   To provide measurable diagnostics for correctness and performance impact.
*   **Test Results:**
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C user` successful.
    *   [PASS] Legacy syscall ABI compatibility preserved while strict mode path is available for staged adoption.

### Epoch II, Day 28: Law 2 Strict Adoption Pass (Paradigm Migration)
*   **What was changed:**
    *   Migrated Paradigm Shadow Mapping path in `user/paradigm/main.c` from legacy `sys_map(...)` calls to `sys_map_strict(...)` (historical Day 28 wrapper; removed in Day 72 when strict path became default `sys_map(...)`).
    *   Added explicit strict negative-path probes before the mapping chain:
        *   invalid index rejection in strict mode
        *   unknown flag-bit rejection
        *   invalid child type rejection (non-RAM/non-pagetable)
        *   non-leaf link rejection when `USER|WRITABLE` is not present
    *   Added local mapping-flag constants in Paradigm for readable, centralized test inputs.
*   **Why it was changed:**
    *   To move Law 2 from "strict path available" to "strict path actively exercised by default user-space flow."
    *   To validate strict mapping invariants through deterministic user-space probes, reducing regression risk for future full cutover.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [NOTE] QEMU runtime verification (`make -C kernel run`) remains pending for this pass.

### Epoch II, Day 29: Law 2 Runtime Validation + Strict Unmap Adoption
*   **What was changed:**
    *   Migrated Paradigm boundary probe from legacy `sys_unmap(...)` to `sys_unmap_strict(...)` for strict API coverage (historical Day 29 wrapper; removed in Day 72 when strict path became default `sys_unmap(...)`).
    *   Executed headless QEMU validation (`-nographic`) to verify runtime behavior in this environment.
*   **Why it was changed:**
    *   To complete strict map/unmap usage in Paradigm's active path.
    *   To close the Day 28 runtime validation gap with serial-log evidence.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] Headless runtime boot log confirms:
        *   `PARADIGM: Boundary probes passed (safe failures confirmed).`
        *   `PARADIGM: Law 2 strict negative probes passed.`
        *   `PARADIGM: Shadow Mapping SUCCESS. The Architect is pleased.`

### Epoch II, Day 30: Fate Strings Rejection Auditing
*   **What was changed:**
    *   Extended `struct mode_transition` with `result_code` (`accepted` or `rejected`) in both kernel and shared headers.
    *   Refactored fate-record append logic in `kernel/mode.c` into a shared helper and now logs illegal transition attempts before returning `-1`.
    *   Updated history availability logic to use ledger head count (`fate_head_index`) so rejected records are retrievable.
    *   Added runtime checks in both kernel/user tests to verify that Fate Strings include rejected transition evidence.
*   **Why it was changed:**
    *   Illegal transition attempts are security-relevant events; excluding them created an audit blind spot.
    *   A unified append path reduces divergence risk between accepted and rejected event hashing/chaining.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] Headless runtime boot log confirms:
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Fate Strings include rejected transition evidence.`

### Epoch II, Day 31: Fate Strings Revalidation Pass
*   **What was changed:**
    *   No functional code changes; executed a clean revalidation pass after Fate Strings Day 30 changes.
*   **Why it was changed:**
    *   To confirm repeatability and stability of Fate String auditing behavior across consecutive runs.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] Headless runtime serial log reconfirmed:
        *   `PARADIGM: Boundary probes passed (safe failures confirmed).`
        *   `PARADIGM: Law 2 strict negative probes passed.`
        *   `PARADIGM: Shadow Mapping SUCCESS. The Architect is pleased.`
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Fate Strings include rejected transition evidence.`

### Epoch II, Day 32: Fault-to-String Integration (Vectors 13/14)
*   **What was changed:**
    *   Extended Fate record schema with record discriminator and fault metadata fields (`record_type`, `fault_vector`, `fault_error_code`, `fault_rip`).
    *   Added filtered history retrieval in kernel mode subsystem and exposed read-mode filtering through `SYS_FATE_READ` argument `a3`.
    *   Integrated fault event logging in IDT path for:
        *   `#GP` (vector 13)
        *   `#PF` (vector 14)
    *   Added userland API `sys_fate_read_ex(...)` and Paradigm validation for fault-record visibility.
    *   Fixed userland startup reliability by compiling user code with `-fno-pie` in `user/Makefile`.
*   **Why it was changed:**
    *   To convert fault handling into auditable Fate records instead of transient console output only.
    *   To let auditors query transition vs fault events directly from user-space.
*   **Test Results:**
    *   [PASS] `make -C kernel clean && make -C kernel iso` successful.
    *   [PASS] Headless runtime serial validation confirms:
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Fate Strings include rejected transition evidence.`
        *   `PARADIGM: Fault Fate records visible with vector/RIP metadata.`

### Epoch II, Day 33: Full Fault Context Capture
*   **What was changed:**
    *   Extended Fate fault records with full execution context fields:
        *   `fault_cr2` (`#PF` address)
        *   `fault_rsp`
        *   `fault_cs`
        *   `fault_rflags`
    *   Updated IDT integration to pass full context into `mode_log_fault_event(...)` for `#GP/#PF`.
    *   Updated synthetic fault test input and userland audit checks to require full-context metadata presence.
*   **Why it was changed:**
    *   Vector/error/RIP alone are insufficient for robust post-mortem reconstruction.
    *   Full context enables higher-confidence forensic replay and policy diagnostics.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] Headless runtime serial validation confirms:
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Fate Strings include rejected transition evidence.`
        *   `PARADIGM: Fault Fate records include full context metadata.`

### Epoch II, Day 34: Real Fault Probe Validation (No Synthetic Injection)
*   **What was changed:**
    *   Removed synthetic fault-event injection from kernel mode self-test path.
    *   Updated user page-fault handler flow to always append Fate fault records before recoverable lattice handling.
    *   Added a post-lattice audit probe in Paradigm that verifies a real `#PF` (first-touch lattice fault at `0x20000000` range) is present in `FATE_READ_FAULTS`.
*   **Why it was changed:**
    *   To prove fault forensics on actual exception paths rather than synthetic test-only records.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] Headless runtime serial validation confirms:
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Fate Strings include rejected transition evidence.`
        *   `PARADIGM: Real fault probe captured in Fate Strings.`

### Epoch II, Day 35: Refinement Pass (Law 2 Completion + Fate Read Hardening)
*   **What was changed:**
    *   Marked Law 2 as complete in Epoch II planning/status docs.
    *   Hardened `SYS_FATE_READ` scratch handling:
        *   replaced single-page PMM scratch buffer with size-aware `kzalloc`/`kfree` buffer
        *   retained bounded request cap (`count <= 128`)
*   **Why it was changed:**
    *   To eliminate stale “in progress” status drift after successful Law 2 rollout.
    *   To remove overflow risk introduced by larger Fate record size.
*   **Test Results:**
    *   [PASS] `make -C kernel clean && make -C kernel iso` successful.
    *   [PASS] Headless runtime serial validation confirms:
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Real fault probe captured in Fate Strings.`

### Epoch II, Day 36: Fate Read Stability Revalidation
*   **What was changed:**
    *   Right-sized Paradigm Fate audit buffers from 64 to 16 records.
    *   Updated Fate read calls (`sys_fate_read`, `sys_fate_read_ex`) to use bounded count `16`.
    *   Performed a clean ISO rebuild to ensure updated userland payload was embedded.
*   **Why it was changed:**
    *   To eliminate user-buffer range pressure observed during Fate retrieval after schema growth.
    *   To reconfirm Law 2 strict mapping and Fate forensics behavior under repeated runtime boots.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel clean && make -C kernel iso` successful.
    *   [PASS] Headless QEMU runtime revalidation (3 repeated boots) confirms, each run:
        *   `PARADIGM: Boundary probes passed (safe failures confirmed).`
        *   `PARADIGM: Shadow Mapping SUCCESS. The Architect is pleased.`
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Lattice Attunement SUCCESS.`
        *   `PARADIGM: Real fault probe captured in Fate Strings.`
    *   [PASS] No occurrences of:
        *   `PARADIGM: Failed to read Fate Strings.`
        *   `PARADIGM: Fault ledger empty after real fault probe.`

### Epoch II, Day 37: Release Discipline Checklist
*   **What was changed:**
    *   Added a standardized release checklist at `docs/development_log/release_checklist.md`.
    *   Checklist defines required steps for build/runtime validation and synchronized doc/version updates.
*   **Why it was changed:**
    *   To prevent documentation/version drift while closing Epoch II MVP scope.
    *   To establish a repeatable handoff process before major Epoch III/IV architectural changes.
*   **Test Results:**
    *   [PASS] Checklist file added and validated in repository docs structure.
    *   [PASS] Day report and development logs updated to reference this release-discipline addition.

### Epoch II, Day 38: Automated Law 2 + Fate Runtime Matrix
*   **What was changed:**
    *   Added `tools/run_law2_fate_matrix.sh` to automate repeated headless QEMU runtime verification.
    *   Added `make -C kernel verify_matrix` target to run a 3-boot runtime matrix with required/forbidden marker checks.
*   **Why it was changed:**
    *   To replace manual repeated runtime checks with a deterministic, reusable validation gate for MVP closure.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel verify_matrix` successful.
    *   [PASS] 3/3 matrix runs confirmed required markers:
        *   `PARADIGM: Boundary probes passed (safe failures confirmed).`
        *   `PARADIGM: Shadow Mapping SUCCESS. The Architect is pleased.`
        *   `PARADIGM: Hash Chain Integrity VERIFIED.`
        *   `PARADIGM: Lattice Attunement SUCCESS.`
        *   `PARADIGM: Real fault probe captured in Fate Strings.`
    *   [PASS] 3/3 matrix runs confirmed no:
        *   `PARADIGM: Failed to read Fate Strings.`
        *   `PARADIGM: Fault ledger empty after real fault probe.`

### Epoch II, Day 39: Law 9 Documentation Closure (Temporal Scouring)
*   **What was changed:**
    *   Added dedicated Law 9 documentation: `docs/components/memory/law9_temporal_scouring.md`.
    *   Documented PMM enforcement behavior, mode epoch coupling, runtime marker, counters, validation flow, and design limits.
    *   Updated mode API documentation to explicitly describe `mode_get_security_epoch()` as Law 9 anchor.
    *   Updated planning/roadmap/status logs to mark Law 9 documentation closure and link to the new component spec.
*   **Why it was changed:**
    *   Law 9 was implemented in kernel code but lacked a single operational spec for maintainers and reviewers.
    *   A centralized doc prevents drift between implementation, validation expectations, and project status artifacts.
*   **Test Results:**
    *   [PASS] Documentation consistency pass completed across component docs, roadmap, and version log entries.
    *   [PASS] Existing runtime evidence retained and referenced (`[LAW9]` marker in matrix serial logs).

### Epoch II, Day 40: ReadOnly Lattices + Lattice Forensics Detach Path
*   **What was changed:**
    *   Extended `SYS_LATTICE_CREATE` to support broadcast topology creation (one source cap + up to two read-only listener caps).
    *   Added lattice forensic Fate schema support:
        *   `FATE_RECORD_LATTICE`
        *   `FATE_READ_LATTICE`
        *   attach/detach action markers
    *   Added explicit `SYS_LATTICE_DETACH` syscall and process detach path.
    *   Added Paradigm runtime probes to validate:
        *   read-only listener behavior
        *   listener attune rejection
        *   explicit detach success
        *   visibility of attach/detach lattice Fate records
    *   Updated syscall/mode documentation with lattice record and detach ABI semantics.
*   **Why it was changed:**
    *   To complete ReadOnly Lattice broadcast mechanics and make lattice lifecycle events auditable in Fate Strings.
    *   To provide deterministic detach semantics instead of relying only on process teardown.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel verify_matrix` successful.
    *   [PASS] Matrix serial logs confirm:
        *   `PARADIGM: Broadcast Lattice ReadOnly Listener PASS.`
        *   `PARADIGM: ReadOnly Listener Detach SUCCESS.`
        *   `PARADIGM: Lattice Forensics attach records visible.`
        *   `PARADIGM: Lattice Forensics detach records visible.`

### Epoch II, Day 41: Syscall Boundary Closure Pass (Fate Count Guard + Probe Auditability)
*   **What was changed:**
    *   Hardened `SYS_FATE_READ` against signed count wrap by rejecting `count > INT32_MAX` before kernel-side cast/clamp.
    *   Added explicit Paradigm boundary probe for negative Fate count: `sys_fate_read(..., -1, ...)` must fail safely.
    *   Upgraded boundary probe logging to emit per-probe PASS/FAIL markers before aggregate summary.
*   **Why it was changed:**
    *   To close a real syscall boundary gap where negative user `int count` could sign-wrap into large unsigned input.
    *   To make boundary regressions immediately diagnosable from `kernel/serial.log` without code spelunking.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] Headless runtime serial validation confirms:
        *   `PARADIGM: Probe PASS (fate_read negative count rejected).`
        *   `PARADIGM: Boundary probes passed (safe failures confirmed).`

### Epoch II, Day 42: Closure Ratification + Epoch III Planning Kickoff
*   **What was changed:**
    *   Ratified Epoch II closure status in planning artifacts.
    *   Updated roadmap phase status to reflect Epoch III kickoff.
    *   Added an explicit Epoch III execution plan centered on:
        *   Vision continuity (decentralized Void Workspace trajectory)
        *   Security hardening (audit semantics, zero-residue completion)
        *   Performance discipline (deterministic scheduling and runtime confidence gates)
*   **Why it was changed:**
    *   To prevent status drift between closure decisions and roadmap metadata.
    *   To begin Epoch III with concrete, security-first sequencing and measurable performance constraints.
*   **Test Results:**
    *   [PASS] Documentation synchronization completed across `TODO.rst`, `epoch_two_plan.md`, and architecture roadmap notes.
    *   [PASS] Epoch III planning artifact added: `docs/development_log/epoch_three_plan.md`.

### Epoch III, Day 43: PMM Safety + Failure Logging Foundation
*   **What was changed:**
    *   Froze `SYS_AUDIT` contract in ABI surfaces with fail-closed kernel stub behavior (`-1` until delegation semantics are implemented).
    *   Added zero-residue policy baseline artifact: `docs/components/modes/zero_residue_policy.md`.
    *   Implemented fail-closed PMM init checks with explicit reason markers (`[PMM-FAIL]`).
    *   Added PMM marker taxonomy foundations:
        *   `[PMM-PROFILE]`
        *   `[PMM-AUDIT]`
        *   `[PMM-QUAR]`
        *   `[PMM-FAIL]`
    *   Hardened PMM sizing and alignment arithmetic:
        *   bitmap round-up sizing
        *   overflow guards for metadata/ledger calculations
        *   aligned usable-region iteration
    *   Added quarantine/candidate accounting with strict profile threshold enforcement.
*   **Why it was changed:**
    *   To remove silent allocator-init failure modes and improve forensic diagnosability.
    *   To align kernel behavior with Epoch III risk counters and incident response workflow.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).

### Epoch III, Day 44: PMM Zoned-Policy Migration Groundwork
*   **What was changed:**
    *   Added policy-aware PMM API surface (`pmm_alloc_ex`, `pmm_free_ex`) with zone/trust/order semantics.
    *   Replaced order-0 compatibility delegation with zoned buddy-backed order allocation/free paths.
    *   Added buddy free-list initialization from audited free frames plus split/merge behavior.
    *   Added strict fail-closed guards for unsupported policy combinations.
    *   Added migration strategy spec: `docs/components/memory/pmm_zoned_buddy_strategy.md`.
*   **Why it was changed:**
    *   To complete the first usable zoned buddy slice while preserving current runtime behavior expectations.
    *   To allow caller-intent expression (zone/trust) before backend replacement.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).

### Epoch III, Day 45: VMM Region-Contract Compile/Apply Bridge
*   **What was changed:**
    *   Added region-contract VMM model (`vmm_region_contract_t`) and compiled intent model (`vmm_compiled_mapping_t`).
    *   Added bridge APIs:
        *   `vmm_compile_region_contract(...)`
        *   `vmm_apply_compiled_mapping(...)`
        *   `vmm_read_recent_contracts(...)`
    *   Routed `vmm_map(...)` through contract -> compile -> apply pipeline.
    *   Added recent-contract ring logging in `vmm.c`.
    *   Added architecture spec: `docs/components/memory/vmm_region_contracts.md`.
*   **Why it was changed:**
    *   To enforce deterministic intent validation before raw page-table mutation.
    *   To advance VMM toward policy-first architecture without breaking existing call sites.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).

### Epoch III, Day 46: Final-Product Envelope Re-Baseline Closure
*   **What was changed:**
    *   Implemented kernel transition envelope pipeline in `kernel/mode.c`:
        *   compile/verify/apply/attest marker flow (`[ENV_COMPILE]`, `[ENV_VERIFY]`, `[ENV_APPLY]`, `[ENV_ATTEST]`)
        *   rollback marker (`[ENV_ROLLBACK]`) and legacy compatibility shim marker (`[MODE_LEGACY_SHIM]`)
    *   Added mode transition gate operation (`GATE_OP_MODE_TRANSITION`) in shared/kernel/user syscall surfaces.
*   **Why it was changed:**
    *   To replace planning-only envelope intent with runtime-enforced transition stages and explicit evidence.
    *   To preserve existing compatibility while hardening determinism and traceability.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).
    *   [PASS] Matrix output confirmed:
        *   `[matrix] run 1 PASS (./serial_matrix_run1.log)`
        *   `[matrix] run 2 PASS (./serial_matrix_run2.log)`
        *   `[matrix] run 3 PASS (./serial_matrix_run3.log)`

### Epoch III, Day 47: Day 5R Multi-Mode Envelope Logic Closure
*   **What was changed:**
    *   Added Paradigm accepted/rejected transition probes via `sys_mode_transition(...)`.
    *   Extended runtime matrix required markers to include envelope stage markers and Paradigm probe pass logs.
*   **Why it was changed:**
    *   To verify Day 5 legality/escalation semantics through user-visible evidence, not only design artifacts.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).

### Epoch III, Day 48: Day 6/Day 7 Final-Product Redesign Closure
*   **What was changed:**
    *   Completed a final-product closure pass for Day 6 allocator and Day 7 capability subsystems.
    *   Day 6 updates:
        *   policy-driven slab contracts
        *   `free/partial/full` per-mode slab state machine
        *   hardened free-path validation and redzone checks
        *   `kmalloc` large-allocation PMM fallback
    *   Day 7 updates:
        *   C-Node locking and identity validation hardening
        *   recursive subtree revocation marking
        *   capability metrics + stricter mint/copy/retype policy checks
        *   expanded capability redesign self-tests
    *   Added closure artifact:
        *   `docs/reports/day48_final_report.md`
*   **Why it was changed:**
    *   To finalize foundational memory/authority primitives in one release-quality pass, with no planned revisit.
    *   To align core kernel primitives with final-product expectations for deterministic behavior, security posture, and runtime confidence.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] Headless runtime serial markers include:
        *   `[TEST] Allocator redesign: SUCCESS.`
        *   `[TEST] Capability redesign: SUCCESS.`
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).

### Epoch III, Day 49: Day 8 Gatekeeper Final-Product Redesign Closure
*   **What was changed:**
    *   Added bootstrap-safe TSS stack defaults in GDT initialization (`RSP0` + `IST1`).
    *   Added explicit `tss_set_ist(...)` API for controlled IST routing updates.
    *   Hardened IDT gate programming:
        *   explicit `#DB` (vector 1) kernel trap-gate semantics
        *   explicit `#BP` (vector 3) user trap-gate semantics
        *   IST assignment for critical vectors (`NMI`, `#DF`, `#MC`)
    *   Added Day 8 observability:
        *   `idt_metrics_t` counters
        *   `idt_get_metrics(...)`
        *   `idt_self_test(...)`
    *   Added runtime Day 8 redesign test marker in kernel boot self-tests.
    *   Added closure artifact:
        *   `docs/reports/day49_final_report.md`
*   **Why it was changed:**
    *   To harden fault-entry stack safety and interrupt gate correctness for final-product readiness.
    *   To provide deterministic, non-destructive release-gate validation for Day 8 behavior.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel iso` successful.
    *   [PASS] Headless runtime serial marker:
        *   `[TEST] Day 8 Gatekeeper redesign: SUCCESS.`
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).

### Epoch III, Day 50: Day 8 Gate-Semantics Rectification
*   **What was changed:**
    *   Added `IDT_TA_USER_TG` (`0xEF`) in `kernel/include/idt.h` for explicit user trap-gate programming.
    *   Corrected IDT vector policy in `kernel/idt.c`:
        *   vector 1 (`#DB`) now uses `IDT_TA_TRAP_GATE`
        *   vector 3 (`#BP`) now uses `IDT_TA_USER_TG`
    *   Extended `idt_self_test()` coverage to assert both vector-specific gate attributes.
    *   Updated documentation artifacts to reflect the explicit final gate contract.
*   **Why it was changed:**
    *   To eliminate documentation/implementation drift in a release-critical exception path.
    *   To lock debug/breakpoint gate semantics behind deterministic boot-time checks.
*   **Test Results:**
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).

### Epoch III, Day 51: Day 45 VMM Contract Final-Closure Pass
*   **What was changed:**
    *   Finalized VMM contract model with explicit operation/result semantics:
        *   `vmm_contract_op_t`
        *   `vmm_contract_result_t`
        *   `vmm_contract_metrics_t`
    *   Hardened compile/apply flow in `kernel/vmm.c`:
        *   operation-specific compile validation
        *   map preflight collision checks
        *   transactional rollback on partial map failure
        *   unmap parity via `vmm_unmap_region(...)`
    *   Hardened walk helpers:
        *   non-allocating leaf walk for read/unmap paths
        *   corrected physical-address masking in `vmm_virt_to_phys(...)`
    *   Added runtime gate:
        *   `vmm_contract_self_test(...)`
        *   serial marker: `[TEST] VMM contract engine: SUCCESS.`
    *   Updated closure artifacts:
        *   `docs/reports/day45_final_report.md`
        *   `docs/components/memory/vmm_region_contracts.md`
*   **Why it was changed:**
    *   To close Day 45 as final-product complete (map/unmap symmetry, failure determinism, release-gate confidence).
    *   To remove partial-bridge behavior that could leave ambiguous MMU mutation outcomes.
*   **Test Results:**
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).
    *   [PASS] Matrix serial confirms:
        *   `[TEST] VMM contract engine: SUCCESS.`

### Epoch III, Day 52: Syscall Gate Final-Product Test Strategy Freeze
*   **What was changed:**
    *   Added dedicated syscall-gate final-product testing strategy:
        *   `docs/components/syscalls/syscall_gate_testing_strategy.md`
    *   Added syscall-gate release gates to:
        *   `docs/development_log/release_checklist.md`
    *   Linked syscall contract baseline to the strategy document:
        *   `docs/components/syscalls/syscall_contracts.md`
    *   Added day artifact:
        *   `docs/reports/day52_final_report.md`
*   **Why it was changed:**
    *   To define closure-grade mandatory test coverage before ABI-v2 syscall redesign implementation.
    *   To prevent partial verification from being treated as release-ready.
*   **Test Results:**
    *   [PASS] Documentation consistency review completed.
    *   [PASS] Strategy and release checklist now encode required syscall-gate test layers and runtime markers.

### Epoch III, Day 53: Syscall ABI v2 Gate Envelope Implementation
*   **What was changed:**
    *   Replaced direct userspace syscall-number invocation with a single gate entry:
        *   `SYS_GATE_CALL`
    *   Added gate op ids (`GATE_OP_*`) and payload envelope:
        *   `gate_call_msg_t`
    *   Updated kernel dispatch path in `kernel/syscall.c`:
        *   reject non-gate syscall numbers from userspace
        *   decode/copy gate payload from userspace
        *   translate gate ops to internal legacy handler ids
    *   Migrated userspace syscall shim in `user/lib/reaper.c`:
        *   all public wrappers now marshal `gate_call_msg_t` and call `SYS_GATE_CALL`
    *   Added explicit ABI-v2 runtime marker:
        *   `[TEST] Syscall Gate ABI v2: SUCCESS.`
*   **Why it was changed:**
    *   To enforce a single controlled gate surface for userspace->kernel entry.
    *   To decouple external ABI from internal handler numbering and support redesign without legacy direct-call exposure.
*   **Test Results:**
    *   [PASS] `make -C user` successful.
    *   [PASS] `make -C kernel` successful.
    *   [PASS] `make -C kernel verify_matrix` successful (3/3 runs).
    *   [PASS] Matrix serial confirms:
        *   `[TEST] Syscall Gate ABI v2: SUCCESS.`
        *   `[TEST] Syscall Gate validation invariants: SUCCESS.`
        *   `[TEST] Syscall Gate security probes: SUCCESS.`
        *   `[TEST] Syscall Gate SMP isolation: SUCCESS.`
        *   `[TEST] Syscall Gate performance budget: SUCCESS.`
        *   `PARADIGM: Boundary probes passed (safe failures confirmed).`

### Epoch III, Day 54: ESAK Scheduler Authority + Atomic Budget Hardening
*   **What was changed:**
    *   Added scheduling authority capability split:
        *   `CAP_TYPE_SCHED_AUTH_ROOT`
        *   `CAP_TYPE_SCHED_AUTH_THREAD`
    *   Added ESAK scheduler APIs for authority mint/derive and immediate revocation handling.
    *   Added deterministic weighted RR token rotation in scheduler selection flow.
    *   Added atomic process-budget consume/refill primitives with dual budget enforcement at dispatch.
    *   Added revoke-driven immediate dequeue and forced-reschedule request flags.
    *   Expanded runtime marker coverage and matrix required markers for ESAK invariants.
    *   Added explicit final-boundary marker:
        *   `[TEST] ESAK IPI profile: BSP_ONLY`
    *   Added/updated artifacts:
        *   `docs/reports/day54_final_report.md`
        *   `docs/development_log/day54_checklist.md`
*   **Why it was changed:**
    *   To lock scheduler authority, budget ceilings, and revocation behavior behind deterministic, auditable contracts.
    *   To close non-atomic budget/race exposure with explicit BSP-only product boundary ratification.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `./tools/run_law2_fate_matrix.sh --runs 1 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel`
    *   [PASS] Matrix serial markers include:
        *   `[TEST] No authority -> no execution`
        *   `[TEST] Root ceiling enforced`
        *   `[TEST] Thread explosion prevented`
        *   `[TEST] Revocation immediate dequeue`
        *   `[TEST] Cross-mode scheduling rejected`
        *   `[TEST] Deterministic RR rotation stable`
        *   `[TEST] SMP atomic budget integrity`
        *   `[TEST] ESAK IPI profile: BSP_ONLY`

### Epoch III, Day 55: Day 11 Void Gate Redesign Closure
* **What was changed:**
    * Implemented 4-stage Entry Pipeline (Compile/Verify/Apply/Attest) in `kernel/entry.c`.
    * Redacted `SYS_MODE_QUERY` for non-privileged processes (occupant isolation).
    * Enforced epoch-aware lease verification in `kernel/scheduler.c`.
    * Added deterministic Entry Pipeline markers and rejection markers.
    * Added artifact: `docs/reports/day55_final_report.md`.
* **Why it was changed:**
    * To ensure absolute occupant isolation and fail-closed reality shifts.
* **Test Results:**
    * [PASS] `make -C kernel verify_matrix` (3/3)
    * [PASS] Matrix serial confirms `[ENTRY_*]` pipeline markers.

### Epoch III, Day 56: Day 12 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 12 closure markers in kernel boot self-tests:
        *   `[TEST] Day 12 Fault Isolation: SUCCESS.`
        *   `[TEST] Day 12 Rendezvous Contract: SUCCESS.`
        *   `[TEST] Day 12 Reaper Lifecycle: SUCCESS.`
        *   `[TEST] Day 12 Process Annihilation: SUCCESS.`
    *   Added Day 12 marker gates to matrix harness:
        *   `tools/run_law2_fate_matrix.sh`
    *   Added artifact: `docs/reports/day56_closure_report.md`
*   **Why it was changed:**
    *   To convert Day 12 from narrative completion into enforceable runtime closure criteria.
    *   To align Day 12 evidence with Vision/Security/Performance governance used by current Epoch III closure slices.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix`
    *   [PASS] Matrix serial confirms all required Day 12 markers.

### Epoch III, Day 57: Day 13 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 13 closure markers in kernel boot self-tests:
        *   `[TEST] Day 13 Extended-State Init: SUCCESS.`
        *   `[TEST] Day 13 Context Preservation: SUCCESS.`
        *   `[TEST] Day 13 Cross-Thread FPU Isolation: SUCCESS.`
        *   `[TEST] Day 13 Crucible Stability: SUCCESS.`
    *   Added explicit fail-closed corruption marker path in FPU crucible threads:
        *   `[DAY13-FAIL]`
    *   Added Day 13 matrix gates:
        *   required Day 13 success markers
        *   forbidden Day 13 failure marker
    *   Added repeat-run Day 13 closure suite:
        *   `tools/run_day13_closure_suite.sh`
    *   Added Day 13 closure contract artifact:
        *   `docs/components/day13/day13_closure_contract.md`
*   **Why it was changed:**
    *   To move Day 13 from historical claim to enforceable closure-grade runtime evidence.
    *   To ensure FPU/SSE context corruption cannot remain silent in production validation paths.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix`
    *   [PASS] `./tools/run_day13_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel`

### Epoch III, Day 58: Day 14 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 14 closure markers in kernel boot self-tests:
        *   `[TEST] Day 14 Wait Contract: SUCCESS.`
        *   `[TEST] Day 14 Yield Gate: SUCCESS.`
        *   `[TEST] Day 14 Lifecycle ABI Surface: SUCCESS.`
    *   Added fail-closed Day 14 failure marker path:
        *   `[DAY14-FAIL]`
    *   Added Paradigm lifecycle probe marker path:
        *   `PARADIGM: Lifecycle gate probe PASS.`
        *   `PARADIGM: Lifecycle gate probe FAIL.`
    *   Added Day 14 matrix required/forbidden marker gates.
    *   Added repeat-run Day 14 closure suite:
        *   `tools/run_day14_closure_suite.sh`
    *   Added Day 14 closure contract artifact:
        *   `docs/components/day14/day14_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 14 lifecycle claims into enforceable runtime closure criteria.
    *   To ensure lifecycle syscall regressions are release-blocking through required/forbidden marker gates.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix`
    *   [PASS] `./tools/run_day14_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel`

### Epoch III, Day 59: Day 15 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 15 closure markers in Genesis bridge runtime path:
        *   `[TEST] Day 15 Genesis Module Contract: SUCCESS.`
        *   `[TEST] Day 15 Genesis Capability Injection: SUCCESS.`
        *   `[TEST] Day 15 Bootinfo Bridge: SUCCESS.`
    *   Added explicit fail-closed Day 15 marker path:
        *   `[DAY15-FAIL]`
    *   Added Paradigm genesis probe marker path:
        *   `PARADIGM: Genesis bridge probe PASS.`
        *   `PARADIGM: Genesis bridge probe FAIL.`
    *   Extended matrix required/forbidden marker gates for Day 15.
    *   Added repeat-run Day 15 closure suite:
        *   `tools/run_day15_closure_suite.sh`
    *   Added Day 15 closure contract artifact:
        *   `docs/components/day15/day15_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 15 bridge behavior from historical claim into enforced final-product closure gates.
    *   To ensure genesis bridge regressions are release-blocking through deterministic required/forbidden marker checks.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day15_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 60: Day 16 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 16 closure markers in Paradigm map/unmap probes:
        *   `[TEST] Day 16 Capability-Scoped Mapping: SUCCESS.`
        *   `[TEST] Day 16 Strict Rights Enforcement: SUCCESS.`
        *   `[TEST] Day 16 Unmap/Remap Contract: SUCCESS.`
    *   Added explicit fail-closed Day 16 marker path:
        *   `[DAY16-FAIL]`
    *   Extended matrix required/forbidden marker gates for Day 16.
    *   Added repeat-run Day 16 closure suite:
        *   `tools/run_day16_closure_suite.sh`
    *   Fixed ISO rebuild freshness:
        *   `kernel/reaper-os.iso` now depends on `../user/init.elf` in `kernel/Makefile`.
    *   Added Day 16 closure contract artifact:
        *   `docs/components/day16/day16_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 16 map/unmap behavior from historical claim into enforced final-product closure gates.
    *   To ensure map rights and unmap lifecycle regressions become release-blocking through deterministic marker checks.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day16_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 61: Day 17 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 17 closure markers in kernel hardening self-tests:
        *   `[TEST] Day 17 IRQ-Safe Spinlocks: SUCCESS.`
        *   `[TEST] Day 17 Stack Canary Guard: SUCCESS.`
        *   `[TEST] Day 17 Spurious IRQ Filter: SUCCESS.`
    *   Added explicit fail-closed Day 17 marker path:
        *   `[DAY17-FAIL]`
    *   Added public declarations for spurious IRQ accounting probes:
        *   `idt_note_spurious39`, `idt_note_spurious47` in `kernel/include/idt.h`
    *   Extended matrix required/forbidden marker gates for Day 17.
    *   Added repeat-run Day 17 closure suite:
        *   `tools/run_day17_closure_suite.sh`
    *   Added Day 17 closure contract artifact:
        *   `docs/components/day17/day17_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 17 hardening behavior from historical claim into enforced final-product closure gates.
    *   To ensure lock/canary/spurious-IRQ regressions are release-blocking through deterministic marker checks.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day17_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 62: Day 18 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 18 closure markers in ELF/bootstrap path:
        *   `[TEST] Day 18 ELF Header Validation: SUCCESS.`
        *   `[TEST] Day 18 ELF Loader Contract: SUCCESS.`
        *   `[TEST] Day 18 Paradigm C Daemon Bootstrap: SUCCESS.`
    *   Added explicit fail-closed Day 18 marker path:
        *   `[DAY18-FAIL]`
    *   Extended matrix required/forbidden marker gates for Day 18.
    *   Added repeat-run Day 18 closure suite:
        *   `tools/run_day18_closure_suite.sh`
    *   Added Day 18 closure contract artifact:
        *   `docs/components/day18/day18_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 18 ELF/bootstrap behavior from historical claim into enforced final-product closure gates.
    *   To ensure ELF validation/loader/bootstrap regressions are release-blocking through deterministic marker checks.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day18_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 63: Day 19 Closure Ratification
*   **What was changed:**
    *   Hardened Day 19 mode-mask constants:
        *   Added `CAP_MODE_VALID_MASK`.
        *   Narrowed `CAP_MODE_ALL` to valid Law 4 mode bits only.
    *   Added fail-closed mode-mask validation in capability core:
        *   `cap_identity_create(...)` rejects zero/invalid masks.
        *   `cap_mint(...)` rejects invalid masks and preserves mode monotonicity.
        *   `cap_lookup(...)` fail-closes malformed identity masks.
    *   Added fail-closed syscall boundary validation:
        *   `SYS_CAP_MINT` rejects zero/invalid mode masks before `cap_mint(...)`.
    *   Added deterministic Day 19 closure markers in kernel self-tests:
        *   `[TEST] Day 19 Mode Mask Validation: SUCCESS.`
        *   `[TEST] Day 19 Conditional Runes: SUCCESS.`
        *   `[TEST] Day 19 Mint Monotonicity: SUCCESS.`
    *   Added explicit fail marker path:
        *   `[DAY19-FAIL]`
    *   Extended matrix required/forbidden marker gates for Day 19.
    *   Added repeat-run Day 19 closure suite:
        *   `tools/run_day19_closure_suite.sh`
    *   Added Day 19 closure contract artifact:
        *   `docs/components/day19/day19_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 19 from historical implementation status into enforced final-product closure gates.
    *   To guarantee fail-closed mode-mask behavior and deterministic Reality-gating evidence under matrix verification.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day19_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 64: Day 20 Closure Ratification
*   **What was changed:**
    *   Hardened Day 20 lattice create validation in `SYS_LATTICE_CREATE`:
        *   strict page-count validation
        *   strict source/listener non-zero + distinct topology checks
    *   Hardened Day 20 lattice attach/detach validation:
        *   requires lattice `CAP_RIGHT_READ`
        *   rejects unaligned/invalid user-range attach addresses
    *   Hardened process lattice attachment model:
        *   rejects duplicate `(lattice, vaddr)` attaches
        *   rejects overlapping lattice windows per process
    *   Added deterministic Day 20 closure markers in Paradigm probes:
        *   `[TEST] Day 20 Lattice Create Contract: SUCCESS.`
        *   `[TEST] Day 20 Lattice Rights Contract: SUCCESS.`
        *   `[TEST] Day 20 Lattice Lifecycle Contract: SUCCESS.`
    *   Added explicit fail marker path:
        *   `[DAY20-FAIL]`
    *   Added explicit negative probes:
        *   invalid broadcast topology rejection
        *   unaligned attach rejection
    *   Extended matrix required/forbidden marker gates for Day 20.
    *   Added repeat-run Day 20 closure suite:
        *   `tools/run_day20_closure_suite.sh`
    *   Added Day 20 closure contract artifact:
        *   `docs/components/day20/day20_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 20 lattice behavior from historical implementation status into enforced final-product closure gates.
    *   To guarantee fail-closed lattice topology/rights/lifecycle behavior with deterministic matrix evidence.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day20_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 65: Day 21 Closure Ratification
*   **What was changed:**
    *   Hardened Day 21 Fate-read authority checks in `SYS_FATE_READ`:
        *   requires `CAP_TYPE_AUDITOR`
        *   requires `CAP_RIGHT_READ`
    *   Hardened Day 21 kernel copy safety:
        *   fail-closed if Fate copy count is negative or exceeds requested count
    *   Added deterministic Day 21 closure markers in Paradigm audit probes:
        *   `[TEST] Day 21 Auditor Access Contract: SUCCESS.`
        *   `[TEST] Day 21 Fate Integrity Contract: SUCCESS.`
        *   `[TEST] Day 21 Fault Forensics Contract: SUCCESS.`
    *   Added explicit fail marker path:
        *   `[DAY21-FAIL]`
    *   Added explicit Day 21 negative probes:
        *   non-auditor Fate read rejection
        *   invalid read-mode rejection
    *   Extended matrix required/forbidden marker gates for Day 21.
    *   Added repeat-run Day 21 closure suite:
        *   `tools/run_day21_closure_suite.sh`
    *   Added Day 21 closure contract artifact:
        *   `docs/components/day21/day21_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 21 auditing behavior from historical implementation status into enforced final-product closure gates.
    *   To guarantee fail-closed auditor authority and deterministic forensic evidence checks under matrix validation.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day21_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 66: Day 22 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 22 closure markers in kernel lineage tests:
        *   `[TEST] Day 22 Recursive Revocation Contract: SUCCESS.`
        *   `[TEST] Day 22 Deep Derivation Contract: SUCCESS.`
    *   Added explicit Day 22 fail marker path:
        *   `[DAY22-FAIL]`
    *   Extended matrix required/forbidden marker gates for Day 22.
    *   Added repeat-run Day 22 closure suite:
        *   `tools/run_day22_closure_suite.sh`
    *   Added Day 22 closure contract artifact:
        *   `docs/components/day22/day22_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 22 lineage/revocation behavior from historical implementation status into enforced final-product closure gates.
    *   To eliminate Day 22 documentation drift and ensure matrix-backed release evidence.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day22_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 67: Day 23 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 23 closure marker in allocator self-test path:
        *   `[TEST] Day 23 Foundation Allocator Contract: SUCCESS.`
    *   Added explicit Day 23 fail marker path:
        *   `[DAY23-FAIL]`
    *   Extended matrix required/forbidden marker gates for Day 23.
    *   Added repeat-run Day 23 closure suite:
        *   `tools/run_day23_closure_suite.sh`
    *   Added Day 23 closure contract artifact:
        *   `docs/components/day23/day23_closure_contract.md`
*   **Why it was changed:**
    *   To close the historical Day 23 evidence gap with deterministic runtime governance.
    *   To make allocator hardening regressions release-blocking via explicit marker gates.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day23_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 68: Day 24 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 24 closure markers in kernel PMM/Ocular probes:
        *   `[TEST] Day 24 Foundation Hardening Contract: SUCCESS.`
        *   `[TEST] Day 24 Ocular Projection Contract: SUCCESS.`
    *   Added explicit Day 24 fail marker path:
        *   `[DAY24-FAIL]`
    *   Added Ocular readiness API:
        *   `ocular_is_ready()`
    *   Extended matrix required/forbidden marker gates for Day 24.
    *   Added repeat-run Day 24 closure suite:
        *   `tools/run_day24_closure_suite.sh`
    *   Added Day 24 closure contract artifact:
        *   `docs/components/day24/day24_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 24 foundation-hardening/Ocular behavior from historical claim into enforceable closure gates.
    *   To make PMM/Law9/Ocular regressions release-blocking under deterministic matrix evidence.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day24_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 69: Day 25 Closure Ratification
*   **What was changed:**
    *   Hardened Day 25 `vmm_switch` fail-closed semantics:
        *   panic on invalid PCID
        *   panic on invalid mode
        *   panic on unaligned PML4 CR3 input
    *   Added Day 25 PCID switch observability counters:
        *   switch count
        *   forced-flush count
        *   reject count
    *   Added deterministic TLB scrub on `pcid_free(...)` before bitmap release.
    *   Integrated secure transition context usage in mode-apply pipeline:
        *   `mode_enter_secure_context()` before transition-critical flush/bleach operations
        *   `mode_exit_secure_context()` after completion
    *   Added deterministic Day 25 closure markers in kernel self-tests:
        *   `[TEST] Day 25 PCID Partition Contract: SUCCESS.`
        *   `[TEST] Day 25 TLB Scrub Contract: SUCCESS.`
        *   `[TEST] Day 25 Secure Context Contract: SUCCESS.`
    *   Added explicit Day 25 fail marker path:
        *   `[DAY25-FAIL]`
    *   Extended matrix required/forbidden marker gates for Day 25.
    *   Added repeat-run Day 25 closure suite:
        *   `tools/run_day25_closure_suite.sh`
    *   Added Day 25 closure contract artifact:
        *   `docs/components/day25/day25_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 25 from historical/manual validation into enforceable final-product closure gates.
    *   To eliminate fail-open switch behavior and enforce deterministic PCID recycle hygiene.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day25_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 70: Day 26 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 26 closure markers in Paradigm Law 6 runtime probes:
        *   `[TEST] Day 26 Prismatic Substrate Contract: SUCCESS.`
        *   `[TEST] Day 26 Void Wall Contract: SUCCESS.`
        *   `[TEST] Day 26 Attunement Contract: SUCCESS.`
    *   Added explicit Day 26 fail marker path:
        *   `[DAY26-FAIL]`
    *   Hardened `lattice_handle_fault(...)` in kernel:
        *   attach window overflow-safe range checks
        *   fault page-index bounds checks against attachment and lattice page counts
    *   Extended matrix required/forbidden marker gates for Day 26.
    *   Added repeat-run Day 26 closure suite:
        *   `tools/run_day26_closure_suite.sh`
    *   Added Day 26 closure contract artifact:
        *   `docs/components/day26/day26_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 26 Law 6 behavior from historical/manual validation into enforceable final-product closure gates.
    *   To ensure Void Wall and attunement behavior remains deterministic and release-blocking on regressions.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day26_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 71: Day 27 Closure Ratification
*   **What was changed:**
    *   Added deterministic Day 27 closure markers in Paradigm boundary/strict probe flow:
        *   `[TEST] Day 27 Boundary Hardening Contract: SUCCESS.`
        *   `[TEST] Day 27 Strict Foundation Contract: SUCCESS.`
        *   `[TEST] Day 27 Syscall Rejection Contract: SUCCESS.`
    *   Added explicit Day 27 fail marker path:
        *   `[DAY27-FAIL]`
    *   Extended matrix required/forbidden marker gates for Day 27.
    *   Added repeat-run Day 27 closure suite:
        *   `tools/run_day27_closure_suite.sh`
    *   Added Day 27 closure contract artifact:
        *   `docs/components/day27/day27_closure_contract.md`
*   **Why it was changed:**
    *   To convert Day 27 boundary-hardening/strict-foundation behavior from generic runtime logs into explicit final-product closure gates.
    *   To make Day 27 boundary and strict negative-path regressions release-blocking.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel iso`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `./tools/run_day27_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 72: Law 2 Closure Hardening (Kernel Attestation + Strict-Only Cutover)
*   **What was changed:**
    *   Added kernel-owned Law 2 attestation gate op and payload:
        *   `GATE_OP_LAW2_ATTEST`
        *   `gate_law2_attest_t`
    *   Added Fate attestation record/filter support:
        *   `FATE_RECORD_ATTEST`
        *   `FATE_READ_ATTEST`
        *   `mode_log_law2_attestation(...)`
    *   Hardened map/unmap strict semantics from tolerated-marker to strict-only control:
        *   `SYS_MAP` now requires `a4 == 1`
        *   `SYS_UNMAP` now requires `a2 == 1`
    *   Removed legacy strict-wrapper ambiguity in user API:
        *   removed `sys_map_strict(...)`, `sys_unmap_strict(...)`
        *   retained strict-only `sys_map(...)`, `sys_unmap(...)`
    *   Integrated kernel attestation call in Paradigm runtime flow:
        *   `sys_law2_attest(...)` executed before lattice probes
    *   Extended matrix and Day 28/29/30 closure suites to require kernel attestation PASS markers and reject attestation FAIL markers.
    *   Updated conformance + closure contracts + syscall contracts/testing strategy to make kernel attestation authoritative for Day 28/29/30 closure.
*   **Why it was changed:**
    *   To remove marker-only trust for Day 28/29/30 closure and shift to kernel-owned evidence.
    *   To eliminate strict/legacy ambiguity and finalize strict-only mapping API behavior.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `make -C kernel verify_day28`
    *   [PASS] `make -C kernel verify_day29`
    *   [PASS] `make -C kernel verify_day30`

### Epoch III, Day 73: Day 29 Enhancement Ratification (Reason-Coded Attestation + Performance Gate)
*   **What was changed:**
    *   Extended Day 29 kernel attestation payload and diagnostics with:
        *   reason coverage mask (`day29_reason_mask`)
        *   strict-unmap latency metrics (`day29_unmap_cycles_max`, `day29_unmap_cycles_avg`)
        *   explicit Day 29 performance budget (`day29_perf_budget_cycles`)
    *   Added strict-unmap reject/success counters in syscall metrics:
        *   control-word reject
        *   parent-type reject
        *   write-rights reject
        *   out-of-range index reject
        *   strict-unmap success count
    *   Hardened strict-unmap path to collect reason-coded evidence and latency budget data in kernel-owned attestation.
    *   Added explicit Day 29 reject-class probes in Paradigm:
        *   strict control word reject
        *   non-pagetable parent reject
        *   write-rights reject
        *   out-of-range index reject
    *   Added Day 29 closure markers:
        *   `[TEST] Day 29 Reason Coverage Contract: SUCCESS.`
        *   `[TEST] Day 29 Performance Budget Contract: SUCCESS.`
    *   Upgraded matrix and Day 29 closure suite gates to require the new Day 29 markers.
*   **Why it was changed:**
    *   To make Day 29 closure evidence reason-complete and kernel-authoritative rather than relying only on coarse pass/fail markers.
    *   To convert strict-unmap latency expectations into release-blocking performance evidence.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_day29` (3/3)
    *   [PASS] `./tools/run_day29_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 74: Day 30 Enhancement Ratification (Reason-Coverage + Scan-Budget Gate)
*   **What was changed:**
    *   Extended Day 30 kernel attestation payload and diagnostics with:
        *   reject-reason coverage mask (`day30_reason_mask`)
        *   attestation scan latency evidence (`day30_reject_scan_cycles`)
        *   explicit Day 30 scan budget (`day30_perf_budget_cycles`)
    *   Hardened Day 30 attestation evaluation to require:
        *   reject-with-reason evidence
        *   required reason-class mask coverage (`EDGE_ILLEGAL`, `AUTH_REQUIRED`, `SPECIAL_KEY_REQUIRED`)
        *   scan-latency budget pass
    *   Added deterministic Day 30 reject-reason probes in Paradigm to guarantee coverage of required reason classes.
    *   Added Day 30 closure markers:
        *   `[TEST] Day 30 Reason Coverage Contract: SUCCESS.`
        *   `[TEST] Day 30 Performance Budget Contract: SUCCESS.`
    *   Upgraded matrix and Day 30 closure suite gates to require new Day 30 markers.
*   **Why it was changed:**
    *   To make Day 30 closure evidence reason-complete and kernel-authoritative.
    *   To convert Day 30 attestation scan cost into a release-blocking performance contract.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_day30` (3/3)
    *   [PASS] `./tools/run_day30_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)

### Epoch III, Day 75: Day 31 Enhancement Ratification (Deterministic Revalidation + Drift Gate)
*   **What was changed:**
    *   Added deterministic Day 31 revalidation markers in Paradigm:
        *   `[TEST] Day 31 Revalidation Security Contract: SUCCESS.`
        *   `[TEST] Day 31 Revalidation Determinism Contract: SUCCESS.`
        *   `[TEST] Day 31 Revalidation Performance Contract: SUCCESS.`
    *   Added explicit Day 31 fail-marker path:
        *   `[DAY31-FAIL]`
    *   Implemented double-attestation revalidation logic in Paradigm:
        *   consecutive `GATE_OP_LAW2_ATTEST` snapshots are compared for status/reason-mask parity
        *   Day 29/30 budget compliance is rechecked per snapshot
        *   inter-snapshot Day 29/30 metric drift is bounded by Day 31 drift budgets
    *   Added Day 31 repeat-run closure suite:
        *   `tools/run_day31_closure_suite.sh`
    *   Added kernel convenience gate:
        *   `make -C kernel verify_day31`
    *   Extended matrix required/forbidden markers and synchronized closure docs:
        *   `tools/run_law2_fate_matrix.sh`
        *   `docs/components/day31/day31_closure_contract.md`
        *   conformance/syscall testing strategy and versioning artifacts
*   **Why it was changed:**
    *   To convert Day 31 from a historical one-off revalidation note into a release-blocking deterministic closure contract.
    *   To enforce both security parity and performance stability across immediate attestation snapshots.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_day31` (3/3)
    *   [PASS] `./tools/run_day31_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)
    *   [PASS] `make -C kernel verify_matrix` (3/3)

### Epoch III, Day 76: Day 32 Enhancement Ratification (Fault-Filter Integrity + Read-Budget Gate)
*   **What was changed:**
    *   Added deterministic Day 32 runtime markers in Paradigm:
        *   `[TEST] Day 32 Fault Filter Contract: SUCCESS.`
        *   `[TEST] Day 32 Fault Metadata Contract: SUCCESS.`
        *   `[TEST] Day 32 Fault Read Performance Contract: SUCCESS.`
    *   Added explicit Day 32 fail-marker path:
        *   `[DAY32-FAIL]`
    *   Implemented Day 32 closure probes in Paradigm for:
        *   `FATE_READ_*` mode isolation across transition/fault/lattice/attest views
        *   #GP/#PF forensic metadata completeness checks from fault records
        *   bounded fault-read runtime budget (`DAY32_FAULT_READ_BUDGET_CYCLES`)
    *   Added Day 32 repeat-run closure suite:
        *   `tools/run_day32_closure_suite.sh`
    *   Added kernel convenience gate:
        *   `make -C kernel verify_day32`
    *   Extended matrix required/forbidden marker gates and synchronized closure docs.
*   **Why it was changed:**
    *   To convert Day 32 from historical integration evidence into release-blocking deterministic closure gates.
    *   To make fault forensics filter correctness and read-path runtime budget explicitly enforceable.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_day32` (3/3)
    *   [PASS] `./tools/run_day32_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)
    *   [PASS] `make -C kernel verify_matrix` (3/3)

### Epoch III, Day 77: Day 33 Enhancement Ratification (Full-Context Integrity + Audit-Budget Gate)
*   **What was changed:**
    *   Added deterministic Day 33 runtime markers in Paradigm:
        *   `[TEST] Day 33 Full Context Coverage Contract: SUCCESS.`
        *   `[TEST] Day 33 Fault Vector Coverage Contract: SUCCESS.`
        *   `[TEST] Day 33 Full Context Performance Contract: SUCCESS.`
    *   Added explicit Day 33 fail-marker path:
        *   `[DAY33-FAIL]`
    *   Implemented Day 33 closure probes in Paradigm for:
        *   full-context integrity checks across sampled fault records
        *   sampled fault-vector/type sanity checks for closure audit windows
        *   bounded full-context audit runtime budget (`DAY33_FULL_CONTEXT_AUDIT_BUDGET_CYCLES`)
    *   Added Day 33 repeat-run closure suite:
        *   `tools/run_day33_closure_suite.sh`
    *   Added kernel convenience gate:
        *   `make -C kernel verify_day33`
    *   Extended matrix required/forbidden marker gates and synchronized closure docs.
*   **Why it was changed:**
    *   To convert Day 33 from historical context-expansion evidence into release-blocking deterministic closure gates.
    *   To guarantee that full fault context remains complete, type-safe, and budget-bounded under repeated runtime validation.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_day33` (3/3)
    *   [PASS] `./tools/run_day33_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)
    *   [PASS] `make -C kernel verify_matrix` (3/3)

### Epoch III, Day 78: Day 34 Enhancement Ratification (Real-Path Provenance + Audit-Budget Gate)
*   **What was changed:**
    *   Added deterministic Day 34 runtime markers in Paradigm:
        *   `[TEST] Day 34 Real Fault Path Contract: SUCCESS.`
        *   `[TEST] Day 34 User Fault Provenance Contract: SUCCESS.`
        *   `[TEST] Day 34 Real Fault Performance Contract: SUCCESS.`
    *   Added explicit Day 34 fail-marker path:
        *   `[DAY34-FAIL]`
    *   Implemented Day 34 closure probes in Paradigm for:
        *   real-path lattice first-touch `#PF` evidence in Fate fault windows
        *   sampled real-fault user provenance and context-integrity checks
        *   bounded real-fault audit runtime budget (`DAY34_REAL_FAULT_AUDIT_BUDGET_CYCLES`)
    *   Added Day 34 repeat-run closure suite:
        *   `tools/run_day34_closure_suite.sh`
    *   Added kernel convenience gate:
        *   `make -C kernel verify_day34`
    *   Extended matrix required/forbidden marker gates and synchronized closure docs.
*   **Why it was changed:**
    *   To convert Day 34 from historical real-path validation evidence into release-blocking deterministic closure gates.
    *   To guarantee real-fault provenance and bounded audit cost under repeated runtime verification.
*   **Test Results:**
    *   [PASS] `make -C user`
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_day34` (3/3)
    *   [PASS] `./tools/run_day34_closure_suite.sh --runs 5 --timeout 35 --iso kernel/reaper-os.iso --out-dir kernel` (5/5)
    *   [PASS] `make -C kernel verify_matrix` (3/3)

### Epoch III, Day 80: Fate Strings & Audit Pipeline Redesign Closure
*   **What was changed:**
    *   **Audit Subsystem Foundation**: 
        *   Implemented `kernel/audit.c` with a 128-byte, 1024-slot ring buffer.
        *   Integrated `stdatomic.h` for SMP-safe head/tail management with Acquire/Release semantics.
    *   **Cryptographic Chaining**:
        *   Migrated the audit chain to vendored **BLAKE3**.
        *   Implemented **Reality-Bound Seeding**: The chain is re-keyed on every Phase Shift using `BLAKE3(Root_Seed | Reality_ID | Epoch)`.
    *   **Instrumentation Matrix**:
        *   `THREAD_CREATE/DESTROY`: Track lifecycle entries/exits.
        *   `PHASE_SHIFT`: Record every Reality transition and its authority context.
        *   `CAP_DENIED/MINT`: Log both policy rejections and the creation of new authority.
        *   `SCHED_STALL`: Integrated starvation detection directly into the audit trail.
    *   **Overflow & Integrity**:
        *   Implemented an explicit `AUDIT_EVENT_OVERFLOW` announcement record.
        *   Introduced `gap_seq` tracking to quantify dropped records while maintaining chain integrity.
    *   **Security Documentation**:
        *   Documented sealed-storage seed hardening as the remaining Ghost-mode follow-up.
*   **Why it was changed:**
    *   To fulfill the "Fatal Forensics" mandate with a cryptographically bound, immutable audit trail.
    *   To provide Sentinel and Paradigm with the evidence required to build a verifiable provenance tree of system state.
*   **Test Results:**
    *   [PASS] `static_assert(sizeof(audit_record_t) == 128)` verified.
    *   [PASS] Seed rotation verified during `VOID -> CASUAL` and `CASUAL -> SECURE` transitions.
    *   [PASS] Overflow logic verified: `AUDIT_EVENT_OVERFLOW` correctly consumes a reserved slot and increments `gap_seq` for the next record.

### Epoch III, Day 84: ACPI Layer 1/2 Foundation Closure
*   **What was changed:**
    *   Added a boot-valid `acpi_init()` path and removed the earlier ACPI panic stub.
    *   Kept Limine RSDP discovery as the firmware handoff path and validated RSDP/root-SDT checksums.
    *   Implemented `acpi_find_table(const char *signature)` as the stable public ACPI lookup interface.
    *   Added static parsing for MADT, FADT, HPET, MCFG, and DMAR.
    *   Wired `acpi_self_test()` into the boot sequence.
*   **Why it was changed:**
    *   To establish the firmware-table substrate required for later IOMMU, PCIe, timer, and CPU-topology work.
    *   To keep ACPI table parsing centralized and self-contained rather than duplicating raw table walks in downstream subsystems.
*   **Test Results:**
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel run`
    *   [PASS] Serial markers:
        *   `[ACPI] RSDP found at 0xffff8000000f5290`
        *   `[ACPI] Found APIC at 0x1ffe1b7a`
        *   `[TEST] ACPI Layer 1+2: SUCCESS.`

### Epoch III, Day 85: Architecture and Documentation Synchronization
*   **What was changed:**
    *   Updated the main architecture document to reflect the newer daemon model from `docs/details.txt`:
        *   Genesis as bootstrapper
        *   Paradigm as ongoing Reality/Security authority
        *   Sentinel as a semi-independent subsystem inside Paradigm
        *   Sage, Archive, Tunnel, Veil, and Nexus with updated roles
    *   Corrected audit design docs to describe the current BLAKE3 implementation.
    *   Corrected Day 84 reporting/status artifacts so they no longer overclaim full memory-hardening completion.
    *   Added synchronized roadmap/report/version entries for the Day 83-85 state.
*   **Why it was changed:**
    *   To eliminate contradictions between the codebase, the roadmap, and the architectural narrative.
    *   To preserve traceability now that the implementation and the design have both moved beyond the older daemon roster and placeholder audit design.
*   **Test Results:**
    *   [PASS] `make -C kernel clean && make -C kernel`
    *   [PASS] `make -C kernel run`
    *   [PASS] Serial markers:
        *   `[TEST] ACPI Layer 1+2: SUCCESS.`
        *   `[TEST] Day 80 Audit Foundation Contract: SUCCESS.`

### Epoch III, Day 86: DMA Authority Contract + DMAR Truth Freeze
*   **What was changed:**
    *   Added an explicit kernel-owned `iommu_inventory_t` with:
        *   inventory state enum
        *   degraded-reason enum
        *   bounded DRHD unit inventory
        *   bounded device-scope inventory
    *   Made ACPI's DMAR parse the only firmware source of truth for IOMMU inventory via:
        *   `acpi_get_dmar_info(...)`
    *   Implemented `iommu_init()` to:
        *   classify missing DMAR as explicit unavailable state
        *   reject invalid or ambiguous topology as degraded state
        *   accept valid DMAR topology as inventoried state
    *   Added `iommu_self_test()` to validate inventory/degraded policy behavior.
    *   Added audit event types for:
        *   IOMMU inventory
        *   degraded state
        *   unit discovery
        *   topology rejection
    *   Added Day 86 design/report/checklist/version artifacts.
*   **Why it was changed:**
    *   To freeze DMA authority semantics before enabling VT-d hardware paths.
    *   To ensure hardware isolation work remains explicit, auditable, and fail-closed rather than drifting into implicit policy.
*   **Test Results:**
    *   [PASS] `make -C kernel clean && make -C kernel`
    *   [PASS] `make -C kernel run`
    *   [PASS] Serial markers:
        *   explicit `[IOMMU] State: ...`
        *   `[TEST] IOMMU Inventory Contract: SUCCESS.`
        *   `[TEST] IOMMU Degraded Policy Contract: SUCCESS.`

### Epoch III, Day 87: Paradigm Stack Baseline Closure
*   **What was changed:**
    *   Read `kernel/serial.log` and confirmed the early Paradigm bootstrap user fault at `0x7ffdd0` below the fixed entry stack top `0x800000`.
    *   Expanded the Genesis-mapped Paradigm user stack in `kernel/genesis.c` from one page to eight pages beneath the fixed entry RSP.
    *   Synchronized report/roadmap/version status documents so the post-fix baseline is explicitly recorded for later daemon work.
*   **Why it was changed:**
    *   To remove the immediate userspace bootstrap fault before starting core-daemon implementation work.
    *   To re-freeze runtime confidence on the exact baseline that will be referenced by later daemon reports.
*   **Test Results:**
    *   [PASS] `make -C kernel`
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] Headless serial log no longer shows the prior `0x7ffdd0` Paradigm bootstrap fault and reaches normal Paradigm runtime markers.

### Epoch III, Day 88: Area 1 Genesis & Process Restoration Closure
*   **What was changed:**
    *   Surgically removed structural corruption from `kernel/process.c`, `kernel/syscall.c`, and `kernel/genesis.c`.
    *   Reimplemented the missing process registry (`process_find_by_pid`, `process_register_live`, `process_unregister_live`) and `genesis_syscall_dispatch`.
    *   Dynamicized Paradigm PID detection in the "Stable Test Suite" to accommodate variable boot-time PID assignments.
*   **Why it was changed:**
    *   To restore kernel build integrity after a corruption event and satisfy the formal Law2+Fate verification matrix.
    *   To formally close Area 1 of the Epoch III backlog and enable work on hardware-backend migration.
*   **Test Results:**
    *   [PASS] `make -C kernel verify_matrix` (3/3)
    *   [PASS] Serial log confirms: `[GENESIS] sys_genesis_invoke: PASS`.
    *   [PASS] Serial log confirms: `USER-LOG] PARADIGM: Genesis bridge probe PASS`.
