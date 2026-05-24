# Reaper-OS Mode System Integration Plan

This document identifies the specific touchpoints where the Universe Layer (Mode Subsystem) integrates with the existing and future Kernel architecture.

## 1. Immediate Integration (Epoch I, Day 4)

These changes will be implemented immediately alongside the mode subsystem code.

### A. Kernel Entry Point (`kernel/main.c`)
The Mode system must be initialized after memory management (PMM/VMM) but before any user-space or sophisticated drivers are loaded.

*   **Initialization:**
    *   **Location:** Inside `kmain()`, immediately after `vmm_init()`.
    *   **Action:** Call `mode_init()`.
    *   **Rationale:** We need dynamic memory (PMM) to potentially set up log buffers, and we need paging (VMM) active.

*   **Verification:**
    *   **Location:** Inside the existing test block.
    *   **Action:** Call `test_mode_transitions()`.
    *   **Rationale:** Validates the state machine logic (Legal/Illegal transitions) early in the boot process.

### B. Physical Memory Manager (`kernel/pmm.c`)
Original Epoch-I planning marker retired. PMM integration is now implemented and tracked through ratified contracts and closure artifacts.

*   **Touchpoint:** `pmm_alloc_page()`
*   **Current status:**
    *   Allocation and scrub behavior is enforced by the PMM policy/temporal-scouring path.
    *   Runtime verification is closure-gated through matrix and day-specific closure suites.
    *   Authoritative behavior references:
        *   `docs/components/memory/law9_temporal_scouring.md`
        *   `docs/conformance_matrix.md`

### C. Virtual Memory Manager (`kernel/vmm.c`)
Original Epoch-I planning marker retired. VMM-related isolation enforcement is now implemented through strict syscall validation, capability checks, and mode/PCID invariants.

*   **Touchpoint:** `vmm_map_page()`
*   **Current status:**
    *   User-facing map/unmap operations are strict-only and fail-closed.
    *   Runtime conformance is verified by matrix/day closure gates with required/forbidden markers.
    *   Authoritative behavior references:
        *   `docs/components/syscalls/syscall_contracts.md`
        *   `docs/components/syscalls/syscall_gate_testing_strategy.md`
        *   `docs/conformance_matrix.md`

---

## 2. Future Integrations (Roadmap)

These points guide the development of upcoming subsystems to ensure they are "Mode-Aware" from birth.

### A. PCID & Context Switching (Day 5)
*   **Concept:** While `mode_id` is global, the *view* of memory changes.
*   **Guideline:**
    *   **Ghost Mode:** When switching TO or FROM a Ghost process, we must perform a `CR3` reload or full `INVPCID` (flushing all TLB entries).
    *   **Rationale:** Prevents Meltdown-style side-channel attacks where a Host process reads Ghost memory residue from the TLB.

### B. The Object Forge / Slab Allocator (Day 6)
*   **Concept:** Efficient object management.
*   **Guideline:**
    *   **Ghost Slabs:** Create specific slab caches (e.g., `ghost_task_struct`, `ghost_file_struct`).
    *   **Rationale:** When Ghost mode exits, we can simply destroy the entire slab cache to wipe all associated kernel objects instantly, rather than iterating and freeing them one by one.

### C. Capability Lists (Day 7)
*   **Concept:** The "Keys" to the system.
*   **Guideline:**
    *   **Tagging:** `struct capability` needs a `uint8_t origin_mode` field.
    *   **Inheritance Rule:** A process in `MODE_CASUAL` cannot hold or inherit a capability created in `MODE_SECURE` unless explicitly downgraded via an Occult Contract.
    *   **Lockdown Rule:** In `MODE_LOCKDOWN`, all `CAP_MODIFY` capabilities are effectively revoked/suspended globally.

### D. System Calls (Epoch II)
*   **Concept:** The bridge to user-space.
*   **Guideline:**
    *   **Restriction:** Certain syscalls (e.g., `ptrace`, `module_load`) must return `-EPERM` immediately if `mode_is_secure()` is true.
    *   **Network:** Socket syscalls must return `-ENETDOWN` in `MODE_LOCKDOWN`.
