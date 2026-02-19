# VMM & PCID Integration Plan

This document details how the existing Virtual Memory Manager (VMM) will be upgraded to support Process-Context Identifiers (PCID), transforming the context switch logic from a "Stop-the-World" flush to a precision instrument.

## 1. The Architectural Shift

### Legacy Behavior (Current)
Currently, `vmm_switch` treats the CR3 register purely as a pointer to the PML4 table.

```c
// Current Implementation
void vmm_switch(uint64_t pml4_phys) {
    // WRITE CR3: Implicitly flushes ALL non-global TLB entries.
    // Cost: ~1000+ cycles (TLB misses follow).
    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}
```

### PCID-Aware Behavior (Target)
With PCID enabled (`CR4.PCIDE = 1`), the CR3 register becomes a composite structure. We must construct it by combining the physical address and the PCID.

```c
// New Implementation
void vmm_switch(uint64_t pml4_phys, uint16_t pcid) {
    // Mask off lower 12 bits of address (should be 0 anyway due to alignment)
    uint64_t cr3_val = pml4_phys & ~0xFFF;
    
    // Add PCID (Bits 0-11)
    cr3_val |= (pcid & 0xFFF);
    
    // NOFLUSH Optimization (Bit 63)
    // If set to 1, CPU preserves TLB entries for this PCID.
    // If 0, CPU flushes entries for this PCID.
    // We typically want 1 (Preserve) for performance.
    cr3_val |= (1ULL << 63); 
    
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3_val) : "memory");
}
```

## 2. CR3 Construction Logic

The `CR3` register is 64-bits.

*   **Bit 63 (NOFLUSH):** 
    *   `1`: Do **not** flush TLB for this PCID (Fast Switch).
    *   `0`: Flush TLB for this PCID (Clean Switch).
*   **Bits 62:52:** Reserved (Must be 0).
*   **Bits 51:12 (PDBR):** The Physical Address of the PML4.
    *   *Constraint:* Must be 4KB aligned (lower 12 bits = 0).
    *   *Verification:* The PMM guarantees page alignment.
*   **Bits 11:0 (PCID):** The Context ID (0-4095).

## 3. Required VMM Modifications

### A. Update `kernel/include/vmm.h`
Modify the function prototype to accept the PCID.

```c
// Old
void vmm_switch(uint64_t pml4_phys);

// New
void vmm_switch(uint64_t pml4_phys, uint16_t pcid);
```

### B. Update `kernel/vmm.c`
Implement the bitwise logic and safety checks.

1.  **Validation:** `kpanic` if `pcid > 4095`.
2.  **Alignment Check:** `kpanic` if `pml4_phys & 0xFFF != 0`.
3.  **Assembly:** Execute the load.

### C. Updates to Callsites
All existing calls to `vmm_switch` must be updated.
*   `vmm_init`: Uses `PCID 0` (Kernel Context).
*   Future Scheduler: Will pass the domain's assigned PCID.

## 4. Handling "First Run" vs. "Switch"

When a domain is first created or reused (recycled PCID), its TLB entries might contain garbage or old data.
*   **Strategy:** The *Allocation* logic (defined in `docs/pcid_allocation_strategy.md`) handles the flushing via `INVPCID`.
*   **VMM Role:** `vmm_switch` acts as the *mechanism*, optimistically assuming the TLB is clean or valid. It defaults to `NOFLUSH=1`. Explicit flushing is done via `invpcid` instructions in the management layer, not the switch layer, to keep the switch path hot and fast.

## 5. Security & Isolation
*   **PCID 0:** Reserved for the Void (Kernel initialization).
*   **Address Masking:** `pml4_phys & ~0xFFF` ensures that even if a non-aligned address was passed (bug), it wouldn't corrupt the PCID field (though it would likely fault the CPU later).
