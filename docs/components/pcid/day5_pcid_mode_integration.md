# Day 5 Plan: PCID & Mode Integration

As the system moves to Day 5 (PCID & INVPCID Integration), the Mode Subsystem will act as the primary orchestrator for TLB (Translation Lookaside Buffer) management.

## 1. The PCID Strategy

Processor Context Identifiers (PCID) allow the CPU to retain TLB entries for multiple address spaces simultaneously.

### Mode-Global vs. Process-Specific
- **System Reality:** The `mode_id_t` is a global state, but it influences how PCIDs are allocated.
- **Context Shifts:** 
    - A shift between `CASUAL` and `SECURE` is a system-wide reality change.
    - A shift into `GHOST` mode requires a "Total Eclipse" of the TLB.

## 2. PCID Allocation Rules

| Mode | PCID Policy |
| :--- | :--- |
| **CASUAL** | Processes use standard PCID range (1 - 4095). |
| **SECURE** | Same as Casual, but PCID flushing is more aggressive on context switches to prevent cross-process data leakage. |
| **LOCKDOWN** | All PCIDs are flushed. Only PCID 0 (Kernel) is active. No user-space TLB entries permitted. |
| **GHOST** | **Isolated Range.** Ghost processes use a specific, high PCID range. Moving from GHOST back to CASUAL triggers an `INVPCID` type 0 (all contexts) to ensure zero residue. |

## 3. Implementation Tasks (Day 5)

1.  **PCID Detection:** Verify `CPUID` leaf for PCID and INVPCID support.
2.  **Enable PCID:** Set `CR4.PCIDE`.
3.  **Mode-Aware `vmm_switch`**:
    - Modify `vmm_switch(uint64_t pml4_phys)` to take a `pcid` argument.
    - If `mode_get_current() == MODE_GHOST`, force a TLB flush even if the PCID matches.
4.  **INVPCID Integration**:
    - Use `INVPCID` instruction for targeted flushing of the "Casual" world when entering "Secure" world.

## 4. Performance Goals
- Reduce context switch overhead by 15-25% by avoiding full TLB flushes for common Casual-to-Casual switches.
- Ensure 100% isolation for GHOST mode via mandatory flushes.
