# Day 6 Plan: The Object Forge (Slab Allocator) & PCID Integration

The Object Forge (Slab Allocator) is Layer 0's mechanism for O(1) management of fixed-size kernel objects. By integrating with the PCID and Mode subsystems, the Slab allocator becomes a high-performance, security-aware foundation for the system's "Reality."

## 1. The PCID Advantage for Slabs

In a microkernel, the allocator often resides in a highly privileged or separate domain. PCID eliminates the "Microkernel Tax" during frequent object lifecycle events.

*   **Fast Domain Switching:** PCID enables switching between the allocator's address space and the caller's space without TLB flushes. This makes kernel object allocation (e.g., creating an IPC message or a new Capability) almost as fast as a standard function call.
*   **TLB Warmth:** By preserving mappings, the TLB remains "warm" with slab metadata, reducing the latency of subsequent allocations.

## 2. Mode-Aware Slab Pools

The "Universe Layer" (Modes) dictates how memory is physically handled. The Slab allocator enforces these laws through segregated pools.

| Mode | Slab Behavior | Security/Performance Rationale |
| :--- | :--- | :--- |
| **CASUAL** | Dynamic growth enabled. | Prioritize system flexibility and uptime. |
| **SECURE** | Quota-enforced pools. | Prevents kernel heap exhaustion attacks. |
| **LOCKDOWN** | **Fixed-size / Frozen.** | No new memory pages are added to slabs. Objects are recycled from a pre-allocated emergency buffer. |
| **GHOST** | **Ephemeral Segregated Pool.** | Uses a dedicated physical range. PCID ensures isolation. On Mode exit, the entire slab pool is wiped instantly. |

## 3. Future Optimizations

### Per-PCID Slab Caches
To eliminate lock contention in multi-core and multi-domain environments, the system will move toward **Per-PCID Caches**.
*   Each domain (assigned a unique PCID) gets a private "L1 Cache" of objects within its own context.
*   **Benefit:** Lock-free allocation for the common path. A domain only hits the global allocator lock when its local cache is empty.

### Hardware-Accelerated Zeroing
Leveraging the "Voidborn" philosophy of zero-residue execution, the Slab allocator will use AVX or PCID-aware DMA engines to zero objects in the background, ensuring that a recycled slab given to a new PCID never contains data from its previous owner.

## 4. Implementation Goals (Day 6)
1.  Define `struct slab` and `struct slab_cache`.
2.  Implement `slab_alloc()` and `slab_free()`.
3.  Implement Mode-aware logic: Refuse pool expansion in `MODE_LOCKDOWN`.
4.  Implement Ghost-isolation: Mark slab pages with `COLOR_GHOST`.
