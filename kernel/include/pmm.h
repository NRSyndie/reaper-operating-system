#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

typedef enum {
    COLOR_VOID     = 0, // Unassigned/Physical/Kernel
    COLOR_CASUAL   = 1, // Casual Mode
    COLOR_SECURE   = 2, // Secure Mode
    COLOR_LOCKDOWN = 3, // Lockdown Mode
    COLOR_GHOST    = 4  // Ghost Mode
} security_color_t;

typedef enum {
    PMM_ZONE_DMA = 0,
    PMM_ZONE_NORMAL = 1,
    PMM_ZONE_HIGH = 2,
    PMM_ZONE_ANY = 3
} pmm_zone_t;

typedef enum {
    PMM_TRUST_VERIFIED = 0,
    PMM_TRUST_UNVERIFIED = 1,
    PMM_TRUST_ANY = 2
} pmm_trust_t;

typedef struct {
    security_color_t color;
    uint64_t owner_token;
    pmm_zone_t preferred_zone;
    pmm_trust_t trust_level;
    uint8_t order;
} pmm_alloc_policy_t;

// Frame state flags
#define FRAME_STATE_FREE  0x01
#define FRAME_STATE_HOT   0x02 // Recently used
#define FRAME_STATE_COLD  0x04 // Long-term storage
#define FRAME_STATE_DIRTY 0x08 // Needs zeroing (not used if we zero on alloc)
#define FRAME_STATE_PT    0x10 // Frame is currently a Page Table (Shadow Mapping)

// Immutable Truth: Exactly 16 bytes per frame
struct frame_metadata {
    uint64_t owner_token;   // Opaque token of the Domain
    uint8_t  color;         // security_color_t
    uint8_t  state;         // Frame state flags
    uint8_t  flags;         // Additional flags (reserved)
    uint8_t  epoch;         // Allocation epoch (groundwork)
    uint32_t next_free_idx; // Index of next free frame in O(1) list
} __attribute__((packed));

// Ensure the 16-byte invariant at compile time
_Static_assert(sizeof(struct frame_metadata) == 16, "frame_metadata must be exactly 16 bytes");

/**
 * @brief Initialize the Physical Memory Manager.
 * Carves out Bitmap and Metadata regions and audits usable RAM.
 */
void pmm_init(void);

/**
 * @brief Allocate a purified (zeroed) 4KB frame.
 * @param color The security color for this frame.
 * @param owner The ownership token for this frame.
 * @return Physical address of the frame, or 0 on failure.
 */
uint64_t pmm_alloc(security_color_t color, uint64_t owner);
uint64_t pmm_alloc_ex(const pmm_alloc_policy_t* policy);

/**
 * @brief Release a frame back to the void.
 */
void pmm_free(uint64_t physical_address);
void pmm_free_ex(uint64_t physical_address, uint8_t order);

/**
 * @brief Atomic transfer of ownership and state.
 */
bool pmm_transfer(uint64_t physical_address, uint64_t new_owner, security_color_t new_color, uint8_t new_state);

/**
 * @brief Return memory statistics.
 */
void pmm_stats(uint64_t *total_frames, uint64_t *free_frames);
void pmm_law9_stats(uint64_t *temporal_scours, uint64_t *same_epoch_zeroes);

/**
 * @brief Convert a physical address to a kernel higher-half virtual address.
 *        Requires HHDM to be initialized.
 */
void* pmm_phys_to_virt(uint64_t phys);

/**
 * @brief Convert a kernel higher-half virtual address back to physical.
 */
uint64_t pmm_virt_to_phys(void* virt);

#endif
