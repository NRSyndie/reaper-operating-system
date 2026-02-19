# PCID Hardware Analysis (Intel x86_64)

This document details the hardware mechanics of Process-Context Identifiers (PCID) and the `INVPCID` instruction, based on Intel SDM Vol 3, Section 4.10.1.

## 1. Concept & Motivation
In standard paging (non-PCID), the Translation Lookaside Buffer (TLB) caches virtual-to-physical address translations.
*   **The Problem:** On x86, writing to the `CR3` register (Context Switch) implicitly flushes the **entire** TLB.
*   **The Cost:** A TLB flush wastes ~1000+ CPU cycles as the pipeline stalls and the CPU must walk page tables again for every memory access until the TLB warms up. This is the "Microkernel Tax" (heavy context switch overhead).
*   **The Solution (PCID):** The CPU tags every TLB entry with a 12-bit ID. `MOV CR3` no longer flushes the whole TLB, but switches the "Current PCID". Entries for other PCIDs remain cached but inactive.

## 2. Hardware Structures

### CR3 Register Layout (When CR4.PCIDE = 1)
When PCID is enabled, the `CR3` register structure changes to accommodate the 12-bit ID.

```text
63                                   52 51                                   12 11          0
+-------------------------------------+---------------------------------------+-------------+
|              Reserved               |      Page Directory Base (Phys)       |     PCID    |
+-------------------------------------+---------------------------------------+-------------+
| NOFLUSH Bit (on Write)              |      (4KB Aligned Address)            |   (0-4095)  |
+-------------------------------------+---------------------------------------+-------------+
```

*   **Bits 11:0 (PCID):** The context identifier. 12 bits = 4096 possible contexts.
*   **Bits 51:12 (PDBR):** Physical address of the PML4 table.
*   **Bit 63 (NOFLUSH):** *Only significant during a `MOV CR3` instruction.*
    *   If `0`: The CPU flushes all TLB entries associated with the **new** PCID (ensures coherency if the page table changed).
    *   If `1`: The CPU preserves all TLB entries. This is the "Fast Switch."

## 3. Feature Detection (CPUID)

Before enabling, we must verify hardware support.

### PCID Support
*   **Instruction:** `CPUID` with `EAX = 0x01`
*   **Result:** Check `ECX` Register, Bit **17**.
*   **Meaning:** If 1, the hardware supports PCID and the `CR4.PCIDE` bit.

### INVPCID Support
*   **Instruction:** `CPUID` with `EAX = 0x07`, `ECX = 0x00`
*   **Result:** Check `EBX` Register, Bit **10**.
*   **Meaning:** If 1, the `INVPCID` instruction is available for fine-grained TLB flushing.

## 4. Enabling PCID
PCID is enabled by setting a bit in Control Register 4 (`CR4`).

*   **Register:** `CR4`
*   **Bit:** 17 (`CR4.PCIDE`)
*   **Constraint:** Can only be toggled when in Long Mode (IA32_EFER.LMA=1).

## 5. Invalidation Strategies (INVPCID)

When PCID is active, `INVLPG` only invalidates the specific address for the *current* PCID. To manage other contexts without switching to them, we use `INVPCID`.

**Instruction:** `INVPCID type, descriptor`
*   `descriptor`: 128-bit memory operand (PCID + Address).

### Invalidation Types:
1.  **Type 0 (Individual Address):** Invalidates a specific virtual address for a specific PCID.
2.  **Type 1 (Single Context):** Invalidates **all** mappings for a specific PCID (Global pages excluded).
3.  **Type 2 (All Contexts, including Global):** Invalidates everything (Total wipe).
4.  **Type 3 (All Contexts, excluding Global):** Invalidates everything except Global pages.

## 6. Relevance to Reaper-OS
*   **Epoch II (IPC):** Fast switching allows us to treat IPC as a "function call" rather than a heavy heavyweight switch.
*   **Ghost Mode:** We can assign a unique PCID to Ghost processes. When exiting Ghost Mode, we use `INVPCID Type 1` to scrub strictly the Ghost context, leaving the Host context warm in the TLB.
