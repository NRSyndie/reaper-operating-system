# PCID Subsystem Design (The Fast Switch)

Process-Context Identifiers (PCID) are the primary mechanism used by Reaper-OS to eliminate the "Microkernel Tax." They allow the CPU to tag TLB entries with a unique ID, preventing the mandatory TLB flush that usually occurs during a context switch (CR3 load).

## 1. Allocation Strategy (The 12-bit Space)

Reaper-OS manages the 4096 available PCIDs (0-4095) using a deterministic bitmap allocator.

### PCID Segmentation
| ID | Role | Logic |
| :--- | :--- | :--- |
| **0** | **Kernel (Void)** | Reserved for the core kernel and early boot. Equivalent to legacy behavior. |
| **1 - 4094** | **User Domains** | Handed out to processes/realities. Each gets a unique TLB "slice." |
| **4095** | **Overflow** | Reserved for use when >4094 domains are active. Forces a TLB flush on every switch. |

### The "Overflow" Fallback
If the system exceeds 4094 concurrent domains, it assigns ID 4095 to all subsequent domains. In `vmm_switch`, the `NOFLUSH` bit is disabled for this ID, ensuring safety at the cost of performance (graceful degradation).

## 2. VMM Integration

The Virtual Memory Manager (VMM) is now "PCID-Aware." The `vmm_switch` function no longer treats CR3 as a simple pointer.

### Composite CR3 Construction
When a context switch occurs, the kernel constructs the new CR3 value:
1.  **Bits 51:12:** Physical address of the PML4 (4KB aligned).
2.  **Bits 11:0:** The 12-bit PCID.
3.  **Bit 63 (NOFLUSH):** Set to `1` if PCID is active and not the Overflow ID.

This allows the CPU to switch address spaces in ~50-100 cycles instead of ~1000+.

## 3. Invalidation & Consistency

Reaper-OS uses the `INVPCID` instruction for precise TLB management.

- **Selective Invalidation:** When a page is unmapped in one domain, we use `invpcid` (Type 0) to flush *only* that entry for that specific PCID.
- **Context Liquidaton:** When a domain is destroyed, we use `invpcid` (Type 1) to scrub its entire TLB footprint, ensuring the next owner of that ID starts with a clean slate.
- **Fallback:** On hardware lacking `INVPCID` support, the kernel falls back to standard `CR3` reloads (Type 2/3 equivalent).

## 4. Performance Characteristics

| Metric | Legacy (Flush) | PCID (Preserve) |
| :--- | :--- | :--- |
| **Switch Latency** | ~11,000 cycles (emulated) | ~100 cycles (hardware) |
| **TLB Warmth** | Lost on every switch | Preserved across realities |
| **IPC Potential** | Bottlenecked by I/O | Fast as a function call |

## 5. Limitations
- **Max Unique Realities:** 4094 (Hardware architecture limit).
- **Global Pages:** Pages marked `GLOBAL` in the page tables ignore PCID and are shared across all contexts (used for Kernel Text).
