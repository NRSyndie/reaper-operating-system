#ifndef SLAB_H
#define SLAB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "mode.h"
#include "utils.h"

// "Soul Forge" Slab Allocator
// Designed for Reaper-OS: Mode-Partitioned, Bulk-Annihilable.

#define SLAB_NAME_MAX 32
#define SLAB_BITMAP_WORDS 4
#define SLAB_MAX_OBJECTS (SLAB_BITMAP_WORDS * 64)
#define SLAB_MODE_MASK(mode) (1U << (uint8_t)(mode))

// Forward declaration
struct slab_cache;

typedef enum {
    SLAB_STATE_FREE = 0,
    SLAB_STATE_PARTIAL = 1,
    SLAB_STATE_FULL = 2
} slab_state_t;

typedef enum {
    SLAB_LIFETIME_DEFAULT = 0,
    SLAB_LIFETIME_TRANSIENT = 1,
    SLAB_LIFETIME_PERSISTENT = 2
} slab_lifetime_t;

typedef enum {
    SLAB_AUDIT_NONE = 0,
    SLAB_AUDIT_BASIC = 1,
    SLAB_AUDIT_STRICT = 2
} slab_audit_t;

typedef struct {
    uint8_t mode_mask;
    uint8_t scrub_on_alloc;
    uint8_t scrub_on_free;
    uint8_t scrub_on_annihilate;
    uint8_t poison_on_free;
    uint8_t enable_redzone;
    uint8_t lifetime_class;
    uint8_t audit_class;
    uint16_t min_partial_slabs;
} slab_policy_t;

typedef struct {
    uint64_t alloc_ok;
    uint64_t alloc_fail;
    uint64_t free_ok;
    uint64_t policy_denied;
    uint64_t invalid_free;
    uint64_t double_free;
    uint64_t cache_miss;
    uint64_t slab_new;
    uint64_t slab_reclaimed;
    uint64_t slabs_annihilated;
} slab_metrics_t;

struct slab {
    struct slab* next;
    struct slab* prev;
    struct slab_cache* cache;    // Back-pointer to the forge
    uint8_t  mode_id;       // The reality this slab belongs to
    uint8_t  state;         // free / partial / full
    uint16_t free_hint;     // Next candidate slot index
    uint16_t in_use_count;  // Number of active objects
    uint16_t max_objects;   // Total capacity
    uint32_t magic;
    uint64_t free_bitmap[SLAB_BITMAP_WORDS]; // Bitmask for up to 256 objects (covers 16B objs in 4KB)
};

/**
 * @brief The Soul Forge Cache.
 */
typedef struct slab_cache {
    char name[SLAB_NAME_MAX];
    size_t requested_size;
    size_t object_size;
    size_t alignment;
    spinlock_t lock;
    
    // The "Forge" State
    // We maintain separate lists for each mode to prevent cross-contamination
    // and enable per-mode bulk freeing.
    struct slab* free_slabs[MODE_COUNT];
    struct slab* partial_slabs[MODE_COUNT];
    struct slab* full_slabs[MODE_COUNT];

    struct slab_cache* next; // Global list linkage

    slab_policy_t policy;
    slab_metrics_t metrics;

    // Statistics (Per cache, not per mode, for now)
    uint64_t total_objects;
    uint64_t total_pages;
} slab_cache_t;

/**
 * @brief Initialize the Soul Forge subsystem.
 */
void slab_init(void);

/**
 * @brief Create a new object cache.
 * @param name Debugging name.
 * @param size Size of objects.
 * @param align Alignment requirements.
 */
slab_cache_t* slab_create_cache(const char* name, size_t size, size_t align);
slab_cache_t* slab_create_cache_ex(const char* name, size_t size, size_t align, const slab_policy_t* policy);

/**
 * @brief Allocate an object from the cache.
 * @note This function AUTOMATICALLY detects the current process Mode.
 *       It will allocate from the specific pool for that mode.
 *       If current mode is LOCKDOWN, allocation may fail if policy dictates.
 */
void* slab_alloc(slab_cache_t* cache);

/**
 * @brief Return an object to the forge.
 * @param obj Pointer to the object.
 * @note The allocator infers the slab and mode from the object address.
 */
void slab_free(slab_cache_t* cache, void* obj);

/**
 * @brief The "Great Filter". Instantly destroys all pages associated
 *        with a specific mode in this cache.
 * @param mode The mode to annihilate (e.g., COLOR_GHOST).
 */
void slab_annihilate(slab_cache_t* cache, int mode_id);

/**
 * @brief Global annihilation. Calls slab_annihilate on ALL caches for a mode.
 *        To be called by mode_switch() when exiting GHOST or SECURE modes.
 */
void slab_annihilate_all(int mode_id);
bool slab_get_metrics(const slab_cache_t* cache, slab_metrics_t* out_metrics);
void slab_get_default_policy(slab_policy_t* out_policy);

#endif
