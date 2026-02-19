#ifndef REAPER_BOOTINFO_H
#define REAPER_BOOTINFO_H

#include <stdint.h>

/**
 * REAPER-OS BOOT INFORMATION (THE GENESIS BRIDGE)
 * 
 * This structure is passed from the Voidborn kernel to the first 
 * user-space process (Paradigm) to provide the initial context 
 * required to construct reality.
 */

#define BOOTINFO_MAGIC 0x5245415053454E47ULL /* "REAPGENG" - Reaper Genesis */

typedef struct {
    uint64_t magic;
    uint64_t version;
    
    /* The HHDM offset used by the kernel (for informational purposes) */
    uint64_t hhdm_offset;
    
    /* The index of the Genesis Capability in Paradigm's C-Space */
    uint32_t genesis_cap_slot;
    
    /* Memory Map (Physical addresses) 
     * Paradigm uses this to understand the physical layout of the universe.
     */
    uint64_t memmap_addr;
    uint64_t memmap_entries;
    
    /* Kernel ELF information (Physical range) */
    uint64_t kernel_start;
    uint64_t kernel_end;

    /* Reserved for future expansion of the Bridge */
    uint64_t reserved[8];
} __attribute__((packed)) boot_info_t;

#endif /* REAPER_BOOTINFO_H */
