# PCID Allocation Strategy

This document defines how Reaper-OS allocates and manages the 12-bit Process-Context Identifiers (PCIDs) to eliminate the "Microkernel Tax" (TLB flush overhead).

## 1. The PCID Address Space

The 12-bit identifier provides 4096 possible slots (0-4095). We segment this space to ensure stability and performance.

| PCID Range | Owner | Description |
| :--- | :--- | :--- |
| **0** | **The Void (Kernel)** | Reserved for the Core Kernel, Idle Loop, and early boot capability. Equivalent to "no PCID" in legacy behavior. |
| **1 - 4094** | **Realities (User Domains)** | Unique identifiers for active processes/domains. Allocated First-Come, First-Served. |
| **4095** | **The Overflow (Trash)** | Reserved for use when >4094 domains are active. Shared by all excess domains. |

## 2. Allocation Lifecycle

We manage PCID allocation using a simple **Bitmap** (similar to the PMM).

### A. Creation (Domain Spawn)
1.  Search the PCID Bitmap for the first `0` bit (starting from index 1).
2.  **If found (e.g., 5):**
    *   Set bit 5.
    *   Assign `Domain->pcid = 5`.
    *   `Domain->use_pcid_flush = false`.
3.  **If NOT found (Exhaustion):**
    *   Assign `Domain->pcid = 4095` (The Overflow).
    *   `Domain->use_pcid_flush = true` (Must flush on every switch).

### B. Context Switch (The Hot Path)
When switching from `PrevDomain` to `NextDomain`:

1.  **Standard Case (PCID 1-4094):**
    *   Construct CR3: `(NextDomain->pml4_phys) | (NextDomain->pcid)`.
    *   Set Bit 63 (`NOFLUSH`) to **1**.
    *   *Result:* TLB preserved. Switch cost: ~50 cycles.

2.  **Overflow Case (PCID 4095):**
    *   Construct CR3: `(NextDomain->pml4_phys) | (4095)`.
    *   Set Bit 63 (`NOFLUSH`) to **0** (Force Flush).
    *   *Result:* TLB flushed for PCID 4095. Switch cost: ~1000+ cycles.
    *   *Note:* This ensures that two overflow domains sharing PCID 4095 do not see each other's cached mappings.

### C. Destruction (Domain Death)
1.  If `Domain->pcid != 4095`:
    *   Clear the corresponding bit in the PCID Bitmap.
    *   Issue `INVPCID` (Type 1: Single Context) for this PCID to scrub any lingering entries from the TLB.
    *   *Why:* Ensures the next domain to reuse this ID starts with a clean slate.

## 3. PCID and The Universe Layer (Modes)

PCIDs are **Orthogonal** to the System Mode. A change in global reality (e.g., Casual -> Ghost) does not inherently require a PCID change for existing processes, but the *policies* around them change.

### Interaction Rules
1.  **Ghost Mode Isolation:**
    *   While PCIDs are assigned normally (1-4094), exiting **GHOST** mode requires an explicit cleanup.
    *   **Action:** When transitioning `GHOST -> CASUAL`, the kernel must issue `INVPCID` (Type 1) for every PCID active during the Ghost session, or a full `INVPCID` (Type 2) if precise tracking is too expensive.
    *   This ensures no "Ghost Reality" mappings persist in the TLB when the system returns to "Casual Reality."

2.  **Lockdown Mode:**
    *   In `LOCKDOWN`, the system may enforce `PCID 0` (Kernel) only, suspending user domains.
    *   If user domains run, they strictly follow standard allocation.

## 4. Rationale

*   **Performance:** 99.9% of the time, domains get a unique PCID and enjoy near-instant context switches.
*   **Scalability:** The "Overflow" strategy allows infinite domains (graceful degradation) without complex LRU recycling logic.
*   **Security:** Explicit invalidation on domain destruction and Ghost exit prevents TLB poisoning/tag-collision attacks.
