#include "include/pcid.h"
#include "include/console.h"
#include "include/utils.h"
#include "include/klog.h"
#include "include/cpu.h"
#include <string.h> // For memset
#include <stdbool.h>

_Static_assert(PCID_SCHEME_MAX < PCID_KERNEL_SECURE, "PCID scheme overlaps secure kernel PCID");

// Global Allocator Instance
struct pcid_allocator pcid_manager;

// Implementation of mode-aware PCID helpers (declared in pcid.h)
uint16_t pcid_get_mode_base(mode_id_t mode) {
    switch (mode) {
        case MODE_CASUAL:    return PCID_BASE_CASUAL;
        case MODE_SECURE:    return PCID_BASE_SECURE;
        case MODE_LOCKDOWN:  return PCID_BASE_LOCKDOWN;
        case MODE_GHOST:     return PCID_BASE_GHOST;
        case MODE_KERNEL:    return PCID_KERNEL; // PCID 0
        default: return PCID_ERROR; // Should not happen with valid mode_id_t
    }
}

uint16_t pcid_get_mode_count(mode_id_t mode) {
    if (mode == MODE_KERNEL) return PCIDS_FOR_KERNEL; // Only PCID 0
    if (mode >= MODE_CASUAL && mode < MODE_KERNEL) return PCIDS_PER_USER_MODE;
    return 0; // Invalid mode or modes beyond defined user modes
}

// Helper to get bitmap for a given mode
static uint64_t* get_mode_bitmap(mode_id_t mode) {
    switch (mode) {
        case MODE_CASUAL:    return pcid_manager.bitmap_casual;
        case MODE_SECURE:    return pcid_manager.bitmap_secure;
        case MODE_LOCKDOWN:  return pcid_manager.bitmap_lockdown;
        case MODE_GHOST:     return pcid_manager.bitmap_ghost;
        default: return NULL; // MODE_KERNEL has a fixed PCID (0), not managed by per-mode bitmap
    }
}

// Helper to get next_hint for a given mode
static uint16_t* get_mode_next_hint(mode_id_t mode) {
    switch (mode) {
        case MODE_CASUAL:    return &pcid_manager.next_hint_casual;
        case MODE_SECURE:    return &pcid_manager.next_hint_secure;
        case MODE_LOCKDOWN:  return &pcid_manager.next_hint_lockdown;
        case MODE_GHOST:     return &pcid_manager.next_hint_ghost;
        default: return NULL;
    }
}

// Helper to get allocated_count for a given mode
static uint32_t* get_mode_allocated_count(mode_id_t mode) {
    switch (mode) {
        case MODE_CASUAL:    return &pcid_manager.allocated_count_casual;
        case MODE_SECURE:    return &pcid_manager.allocated_count_secure;
        case MODE_LOCKDOWN:  return &pcid_manager.allocated_count_lockdown;
        case MODE_GHOST:     return &pcid_manager.allocated_count_ghost;
        default: return NULL;
    }
}

static uint32_t* get_mode_max_count(mode_id_t mode) {
    switch (mode) {
        case MODE_CASUAL:    return &pcid_manager.max_count_casual;
        case MODE_SECURE:    return &pcid_manager.max_count_secure;
        case MODE_LOCKDOWN:  return &pcid_manager.max_count_lockdown;
        case MODE_GHOST:     return &pcid_manager.max_count_ghost;
        default: return NULL;
    }
}

static uint32_t* get_mode_freed_count(mode_id_t mode) {
    switch (mode) {
        case MODE_CASUAL:    return &pcid_manager.freed_count_casual;
        case MODE_SECURE:    return &pcid_manager.freed_count_secure;
        case MODE_LOCKDOWN:  return &pcid_manager.freed_count_lockdown;
        case MODE_GHOST:     return &pcid_manager.freed_count_ghost;
        default: return NULL;
    }
}

// Internal helpers for bitmap manipulation (relative to mode's range)
static inline void bitmap_set_relative(uint64_t* bitmap, uint16_t relative_idx) {
    if (bitmap) {
        bitmap[relative_idx / 64] |= (1ULL << (relative_idx % 64));
    }
}

static inline void bitmap_clear_relative(uint64_t* bitmap, uint16_t relative_idx) {
    if (bitmap) {
        bitmap[relative_idx / 64] &= ~(1ULL << (relative_idx % 64));
    }
}

static inline bool bitmap_test_relative(uint64_t* bitmap, uint16_t relative_idx) {
    if (bitmap) {
        return (bitmap[relative_idx / 64] & (1ULL << (relative_idx % 64))) != 0;
    }
    return false; // Invalid bitmap
}

void pcid_init(void) {
    // Zero out the entire structure, including all bitmaps and stats
    memset(&pcid_manager, 0, sizeof(struct pcid_allocator));
    
    // Initialize lock
    pcid_manager.lock = 0;
    
    // PCID_KERNEL (PCID 0) is implicitly allocated and reserved.
    // It's not part of the user mode ranges, so no need to set it in any bitmap.

    // Set next hint for user modes to their respective first relative index (0)
    pcid_manager.next_hint_casual = 0;
    pcid_manager.next_hint_secure = 0;
    pcid_manager.next_hint_lockdown = 0;
    pcid_manager.next_hint_ghost = 0;

    klog_info("PCID: Initialized. Kernel PCID: %u. Casual Base: %u, Count: %u. Secure Base: %u, Count: %u. Lockdown Base: %u, Count: %u. Ghost Base: %u, Count: %u.",
              PCID_KERNEL,
              PCID_BASE_CASUAL, pcid_get_mode_count(MODE_CASUAL),
              PCID_BASE_SECURE, pcid_get_mode_count(MODE_SECURE),
              PCID_BASE_LOCKDOWN, pcid_get_mode_count(MODE_LOCKDOWN),
              PCID_BASE_GHOST, pcid_get_mode_count(MODE_GHOST));
}

uint16_t pcid_alloc(mode_id_t mode) {
    if (mode == MODE_KERNEL) {
        // Kernel PCID is fixed (0) and implicitly always allocated.
        // For now, we simply return 0. Future might add ref-counting for kernel PCID.
        return PCID_KERNEL;
    }

    uint64_t flags = spinlock_irqsave(&pcid_manager.lock);
    
    uint64_t* bitmap = get_mode_bitmap(mode);
    uint16_t* next_hint = get_mode_next_hint(mode);
    uint32_t* allocated_count = get_mode_allocated_count(mode);
    uint16_t mode_base = pcid_get_mode_base(mode);
    uint16_t mode_count = pcid_get_mode_count(mode);

    if (!bitmap || mode_count == 0 || mode_base == PCID_ERROR) {
        klog_error("PCID: Invalid mode (%d) or configuration for allocation.", mode);
        spinlock_irqrestore(&pcid_manager.lock, flags);
        return PCID_ERROR;
    }

    // Iterate through the mode's PCID range starting from the hint
    for (uint16_t i = 0; i < mode_count; i++) {
        uint16_t current_relative_idx = (*next_hint + i) % mode_count;

        if (!bitmap_test_relative(bitmap, current_relative_idx)) {
            // Check if this PCID was previously freed (reused)
            uint32_t* freed_ptr = get_mode_freed_count(mode);
            if (freed_ptr && *freed_ptr > 0) {
                pcid_manager.total_reuses++;
            }

            bitmap_set_relative(bitmap, current_relative_idx);
            *next_hint = (current_relative_idx + 1) % mode_count; // Update hint for next allocation
            
            (*allocated_count)++;
            
            uint32_t* max_ptr = get_mode_max_count(mode);
            if (max_ptr && *allocated_count > *max_ptr) {
                *max_ptr = *allocated_count;
            }

            pcid_manager.total_allocations++;
            
            spinlock_irqrestore(&pcid_manager.lock, flags);
            return mode_base + current_relative_idx;
        }
    }
    
    klog_warn("PCID: No free PCIDs in mode %d range (base %u, count %u).", mode, mode_base, mode_count);
    spinlock_irqrestore(&pcid_manager.lock, flags);
    return PCID_ERROR;
}

void pcid_free(uint16_t pcid, mode_id_t mode) {
    if (mode == MODE_KERNEL) {
        // Kernel PCID (0) is fixed and not dynamically allocated/freed via bitmap.
        klog_warn("PCID: Attempted to free kernel PCID (0) for MODE_KERNEL. Ignoring.");
        return;
    }
    if (pcid == PCID_ERROR) {
        klog_warn("PCID: Attempted to free PCID_ERROR value. Ignoring.");
        return;
    }

    uint64_t flags = spinlock_irqsave(&pcid_manager.lock);

    uint16_t mode_base = pcid_get_mode_base(mode);
    uint16_t mode_count = pcid_get_mode_count(mode);

    // Validate if the PCID falls within the expected range for the given mode
    if (pcid < mode_base || pcid >= (mode_base + mode_count)) {
        klog_error("PCID: Attempted to free PCID %u for mode %d, but it's outside its designated range (%u-%u).",
                   pcid, mode, mode_base, mode_base + mode_count - 1);
        spinlock_irqrestore(&pcid_manager.lock, flags);
        return;
    }

    uint64_t* bitmap = get_mode_bitmap(mode);
    uint32_t* allocated_count = get_mode_allocated_count(mode);
    
    if (!bitmap) {
        klog_error("PCID: Invalid mode (%d) for deallocation.", mode);
        spinlock_irqrestore(&pcid_manager.lock, flags);
        return;
    }

    uint16_t relative_idx = pcid - mode_base;

    if (bitmap_test_relative(bitmap, relative_idx)) {
        /*
         * Scrub stale TLB state before returning the ID to the allocator.
         * Reuse without this flush can leak old translations into the next owner.
         */
        invpcid(INVPCID_TYPE_SINGLE_CONTEXT, pcid, 0);
        pcid_manager.total_tlb_scrubs++;

        bitmap_clear_relative(bitmap, relative_idx);
        if (*allocated_count > 0) { // Safety check to prevent underflow
            (*allocated_count)--;
        }

        uint32_t* freed_ptr = get_mode_freed_count(mode);
        if (freed_ptr) {
            (*freed_ptr)++;
        }
    } else {
        klog_warn("PCID: Attempted to free an unallocated or already freed PCID %u (relative %u) for mode %d.",
                  pcid, relative_idx, mode);
    }

    spinlock_irqrestore(&pcid_manager.lock, flags);
}

// pcid_is_allocated is removed as it's not compatible with per-mode bitmaps
// and its functionality is absorbed into the alloc/free logic.
