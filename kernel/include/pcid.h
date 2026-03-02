#ifndef REAPER_PCID_H
#define REAPER_PCID_H

#include <stdint.h>
#include <utils.h>
#include <mode.h> // Include mode.h

// --- Original PCID Definitions (Modified) ---
// PCID_KERNEL (0) is reserved for the kernel.
#define PCID_KERNEL 0
#define PCID_MAX 4095       // Maximum PCID value (12-bit)
#define PCID_COUNT 4096     // Total PCIDs
#define PCID_ERROR 0xFFFF   // Error code for allocation failure

/**
 * PCID_KERNEL_SECURE: Reserved for Mode Shifts and Secure Wipe operations.
 * Allows clean TLB flush patterns during Fate String logging, Ocular Bleaching, 
 * and Mode transitions.
 */
#define PCID_KERNEL_SECURE 4095

// --- New PCID Colorization Definitions ---
// We have 4094 remaining PCIDs (1 to 4094) for user space modes.
// Let's dedicate a fixed, conservative number of PCIDs per user mode for now.
// Current user modes: MODE_CASUAL, MODE_SECURE, MODE_LOCKDOWN, MODE_GHOST (4 modes)
/**
 * SCALABILITY NOTE:
 * If the system grows beyond 256 processes per Reality (Mode), 
 * bump PCIDS_PER_USER_MODE to 512 or 1024.
 */
#define PCIDS_PER_USER_MODE 256 // Each user mode gets 256 PCIDs.
#define PCIDS_FOR_KERNEL    1   // PCID 0 is reserved exclusively for the kernel.

// Base PCID for each mode's dedicated range
#define PCID_BASE_CASUAL    (PCIDS_FOR_KERNEL) // Starts at 1
#define PCID_BASE_SECURE    (PCID_BASE_CASUAL + PCIDS_PER_USER_MODE)
#define PCID_BASE_LOCKDOWN  (PCID_BASE_SECURE + PCIDS_PER_USER_MODE)
#define PCID_BASE_GHOST     (PCID_BASE_LOCKDOWN + PCIDS_PER_USER_MODE)

// Calculate the maximum PCID used by the current scheme.
// This is mainly for sanity checking that we don't exceed PCID_MAX
#define PCID_SCHEME_MAX     (PCID_BASE_GHOST + PCIDS_PER_USER_MODE - 1)

// --- Modified pcid_allocator structure ---
struct pcid_allocator {
    // Separate bitmap for each user mode for its dedicated PCID range
    // Each mode gets PCIDS_PER_USER_MODE (256) PCIDs, so 256 bits = 4 uint64_t for each bitmap
    uint64_t bitmap_casual[PCIDS_PER_USER_MODE / 64];
    uint64_t bitmap_secure[PCIDS_PER_USER_MODE / 64];
    uint64_t bitmap_lockdown[PCIDS_PER_USER_MODE / 64];
    uint64_t bitmap_ghost[PCIDS_PER_USER_MODE / 64];
    // PCID 0 (kernel) is fixed, so no bitmap needed for it.

    // Next PCID hints for each user mode
    uint16_t next_hint_casual;
    uint16_t next_hint_secure;
    uint16_t next_hint_lockdown;
    uint16_t next_hint_ghost;
    
    // Statistics for each user mode
    uint32_t allocated_count_casual;
    uint32_t allocated_count_secure;
    uint32_t allocated_count_lockdown;
    uint32_t allocated_count_ghost;

    uint32_t max_count_casual;
    uint32_t max_count_secure;
    uint32_t max_count_lockdown;
    uint32_t max_count_ghost;

    uint32_t freed_count_casual;
    uint32_t freed_count_secure;
    uint32_t freed_count_lockdown;
    uint32_t freed_count_ghost;

    // Global statistics (optional, for debugging/overall tracking)
    uint32_t total_allocations;
    uint32_t total_reuses;
    uint32_t total_tlb_scrubs;
    spinlock_t lock; // Global lock for the entire allocator for now. Fine-grained locks per-mode can be added later.
};

// Global Allocator Instance (to be defined in pcid.c)
extern struct pcid_allocator pcid_manager;

// Public API
void pcid_init(void);
// Modified API to support mode-aware allocation/deallocation
uint16_t pcid_alloc(mode_id_t mode);
void pcid_free(uint16_t pcid, mode_id_t mode);

// Forward declarations for mode-aware PCID helpers (implemented in pcid.c)
uint16_t pcid_get_mode_base(mode_id_t mode);
uint16_t pcid_get_mode_count(mode_id_t mode);


#endif /* REAPER_PCID_H */
