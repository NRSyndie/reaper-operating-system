# PCID Subsystem API

The PCID Subsystem provides the hardware-accelerated context switching interface.

## 1. Allocator API
Defined in `kernel/include/pcid.h`.

### `void pcid_init(void)`
Initializes the bitmap allocator. Reserves PCID 0 (Kernel) and PCID 4095 (Overflow).

### `uint16_t pcid_alloc(void)`
Allocates the next available PCID.
- **Returns:** A unique ID (1-4094) or `PCID_ERROR` (0xFFFF).

### `void pcid_free(uint16_t pcid)`
Releases a PCID back to the pool.
- **Safety:** Panics if attempting to free PCID 0.

## 2. VMM PCID Interface
Defined in `kernel/include/vmm.h`.

### `void vmm_switch(uint64_t pml4_phys, uint16_t pcid)`
The core context switch mechanism.
- **pml4_phys:** Physical address of the new page table.
- **pcid:** The ID associated with this address space.
- **Behavior:** Automatically sets the `NOFLUSH` bit to preserve TLB if hardware support is detected.

### `void invpcid_flush_single(uint16_t pcid, uint64_t addr)`
Invalidates a single virtual address mapping for a specific PCID.

### `void invpcid_flush_context(uint16_t pcid)`
Invalidates all mappings for a specific PCID.

## 3. Hardware Feature Queries
Defined in `kernel/include/cpu.h`.

### `bool cpu_has_pcid(void)`
Returns `true` if the processor supports the PCIDE bit in CR4.

### `bool cpu_has_invpcid(void)`
Returns `true` if the processor supports the `INVPCID` instruction.

## 4. Usage Example

```c
// Creating a new reality
uint64_t new_pml4 = vmm_fork_pml4();
uint16_t my_pcid = pcid_alloc();

// High-speed context switch
vmm_switch(new_pml4, my_pcid);

// ... perform work ...

// Precision TLB flush
invpcid_flush_single(my_pcid, 0x1234000);
```
