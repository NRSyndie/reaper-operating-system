#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include "include/pmm.h"
#include "include/mode.h"
#include "include/limine.h"
#include "include/console.h"
#include "include/utils.h"
#include "include/klog.h"

// External requests from main.c
extern struct limine_memmap_request memmap_request;
extern struct limine_hhdm_request hhdm_request;

// Internal State
static uint8_t*  bitmap;
static struct frame_metadata* metadata;
static uint64_t  total_frames;
static uint64_t  free_frames;
static uint64_t  hhdm_offset;
static uint64_t  max_address;
static spinlock_t pmm_lock = 0;
static uint64_t law9_temporal_scour_count = 0;
static uint64_t law9_same_epoch_zero_count = 0;
static bool law9_marker_emitted = false;
static uint64_t pmm_quarantine_frames = 0;
static uint64_t pmm_candidate_frames = 0;
static uint64_t pmm_reserved_ledger_frames = 0;
static bool pmm_policy_marker_emitted = false;

#define PMM_STACK_END 0xFFFFFFFF
#define PMM_MIN_FREE_FRAMES 128ULL
#define PMM_MAX_QUARANTINE_PERCENT 35ULL
#define PMM_DMA_LIMIT_BYTES   (16ULL * 1024ULL * 1024ULL)
#define PMM_HIGH_LIMIT_BYTES  (4ULL * 1024ULL * 1024ULL * 1024ULL)
#define PMM_BUDDY_MAX_ORDER   10
#define PMM_BUDDY_FLAG_HEAD   0x80
#define PMM_BUDDY_FLAG_ORDER  0x0F

typedef enum {
    PMM_PROFILE_STRICT = 0,
    PMM_PROFILE_COMPAT = 1
} pmm_profile_t;

static const pmm_profile_t pmm_profile = PMM_PROFILE_STRICT;
static uint32_t buddy_free_heads[PMM_ZONE_HIGH + 1][PMM_BUDDY_MAX_ORDER + 1];

// Helper: Convert physical address to Higher-Half Direct Map address
static inline void* phys_to_virt(uint64_t phys) {
    return (void*)(phys + hhdm_offset);
}

static inline uint64_t align_up_u64(uint64_t value, uint64_t alignment) {
    if (alignment == 0) return value;
    uint64_t rem = value % alignment;
    if (rem == 0) return value;
    if (value > UINT64_MAX - (alignment - rem)) return UINT64_MAX;
    return value + (alignment - rem);
}

static inline uint64_t align_down_u64(uint64_t value, uint64_t alignment) {
    if (alignment == 0) return value;
    return value - (value % alignment);
}

static inline uint64_t u64_percent(uint64_t part, uint64_t total) {
    if (total == 0) return 0;
    return (part * 100ULL) / total;
}

static pmm_zone_t pmm_zone_from_phys(uint64_t phys) {
    if (phys < PMM_DMA_LIMIT_BYTES) return PMM_ZONE_DMA;
    if (phys < PMM_HIGH_LIMIT_BYTES) return PMM_ZONE_NORMAL;
    return PMM_ZONE_HIGH;
}

static void pmm_emit_profile_marker(void) {
    const char *name = (pmm_profile == PMM_PROFILE_STRICT) ? "strict" : "compat";
    kprintf("[PMM-PROFILE] active=%s min_free=%lu max_quarantine_pct=%lu\n",
            name,
            PMM_MIN_FREE_FRAMES,
            PMM_MAX_QUARANTINE_PERCENT);
}

static void pmm_fail_closed(const char *reason) {
    kprintf("[PMM-FAIL] phase=init reason=%s profile=%s\n",
            reason,
            (pmm_profile == PMM_PROFILE_STRICT) ? "strict" : "compat");
    kpanic("PMM INIT FAIL-CLOSED: %s", reason);
}

// Helper: Set/Clear/Test bits in bitmap
static inline void bitmap_set(uint64_t frame_idx) {
    bitmap[frame_idx / 8] |= (1 << (frame_idx % 8));
}

static inline void bitmap_clear(uint64_t frame_idx) {
    bitmap[frame_idx / 8] &= ~(1 << (frame_idx % 8));
}

static inline bool bitmap_test(uint64_t frame_idx) {
    return bitmap[frame_idx / 8] & (1 << (frame_idx % 8));
}

static inline uint64_t pmm_block_pages(uint8_t order) {
    return (1ULL << order);
}

static inline uint64_t pmm_zone_limit_frame(pmm_zone_t zone) {
    if (zone == PMM_ZONE_DMA) {
        uint64_t limit = PMM_DMA_LIMIT_BYTES / PAGE_SIZE;
        return (limit < total_frames) ? limit : total_frames;
    }
    if (zone == PMM_ZONE_NORMAL) {
        uint64_t limit = PMM_HIGH_LIMIT_BYTES / PAGE_SIZE;
        return (limit < total_frames) ? limit : total_frames;
    }
    return total_frames;
}

static inline bool pmm_block_is_head(uint64_t idx, uint8_t order) {
    if (idx >= total_frames) return false;
    if (!(metadata[idx].flags & PMM_BUDDY_FLAG_HEAD)) return false;
    return (metadata[idx].flags & PMM_BUDDY_FLAG_ORDER) == (order & PMM_BUDDY_FLAG_ORDER);
}

static inline bool pmm_block_fits_zone(uint64_t idx, uint8_t order, pmm_zone_t zone) {
    uint64_t end = idx + pmm_block_pages(order);
    uint64_t zone_limit = pmm_zone_limit_frame(zone);
    return end <= zone_limit;
}

static void pmm_set_block_bitmap(uint64_t idx, uint8_t order, bool allocated) {
    uint64_t pages = pmm_block_pages(order);
    for (uint64_t i = 0; i < pages; i++) {
        if (allocated) {
            bitmap_set(idx + i);
            metadata[idx + i].state = 0;
        } else {
            bitmap_clear(idx + i);
            metadata[idx + i].state = FRAME_STATE_FREE;
        }
    }
}

static void pmm_buddy_push(pmm_zone_t zone, uint8_t order, uint64_t idx) {
    metadata[idx].flags = PMM_BUDDY_FLAG_HEAD | (order & PMM_BUDDY_FLAG_ORDER);
    metadata[idx].next_free_idx = buddy_free_heads[zone][order];
    buddy_free_heads[zone][order] = (uint32_t)idx;
}

static uint32_t pmm_buddy_pop(pmm_zone_t zone, uint8_t order) {
    uint32_t head = buddy_free_heads[zone][order];
    if (head == PMM_STACK_END) return PMM_STACK_END;
    buddy_free_heads[zone][order] = metadata[head].next_free_idx;
    metadata[head].next_free_idx = PMM_STACK_END;
    return head;
}

static bool pmm_buddy_remove(pmm_zone_t zone, uint8_t order, uint64_t target_idx) {
    uint32_t* head = &buddy_free_heads[zone][order];
    uint32_t prev = PMM_STACK_END;
    uint32_t cur = *head;

    while (cur != PMM_STACK_END) {
        if (cur == target_idx) {
            uint32_t next = metadata[cur].next_free_idx;
            if (prev == PMM_STACK_END) {
                *head = next;
            } else {
                metadata[prev].next_free_idx = next;
            }
            metadata[cur].next_free_idx = PMM_STACK_END;
            metadata[cur].flags &= ~PMM_BUDDY_FLAG_HEAD;
            return true;
        }
        prev = cur;
        cur = metadata[cur].next_free_idx;
    }
    return false;
}

static void pmm_buddy_init_from_free_frames(void) {
    for (uint32_t zone = PMM_ZONE_DMA; zone <= PMM_ZONE_HIGH; zone++) {
        for (uint32_t order = 0; order <= PMM_BUDDY_MAX_ORDER; order++) {
            buddy_free_heads[zone][order] = PMM_STACK_END;
        }
    }

    uint64_t idx = 0;
    while (idx < total_frames) {
        if (bitmap_test(idx)) {
            idx++;
            continue;
        }

        pmm_zone_t zone = pmm_zone_from_phys(idx * PAGE_SIZE);
        uint8_t best_order = 0;

        while (best_order < PMM_BUDDY_MAX_ORDER) {
            uint8_t candidate = best_order + 1;
            uint64_t pages = pmm_block_pages(candidate);
            if ((idx & (pages - 1)) != 0) break;
            if (idx + pages > total_frames) break;
            if (!pmm_block_fits_zone(idx, candidate, zone)) break;

            bool all_free = true;
            for (uint64_t i = 0; i < pages; i++) {
                if (bitmap_test(idx + i)) {
                    all_free = false;
                    break;
                }
            }
            if (!all_free) break;
            best_order = candidate;
        }

        pmm_buddy_push(zone, best_order, idx);
        idx += pmm_block_pages(best_order);
    }

    kprintf("[PMM-AUDIT] buddy_free_lists initialized (max_order=%u)\n", PMM_BUDDY_MAX_ORDER);
}

static uint64_t pmm_buddy_alloc_block(uint8_t order, pmm_zone_t preferred_zone) {
    pmm_zone_t zone_try[3];
    uint32_t zone_count = 0;

    if (preferred_zone == PMM_ZONE_ANY) {
        zone_try[zone_count++] = PMM_ZONE_NORMAL;
        zone_try[zone_count++] = PMM_ZONE_DMA;
        zone_try[zone_count++] = PMM_ZONE_HIGH;
    } else {
        zone_try[zone_count++] = preferred_zone;
    }

    for (uint32_t zi = 0; zi < zone_count; zi++) {
        pmm_zone_t zone = zone_try[zi];
        for (uint8_t cur = order; cur <= PMM_BUDDY_MAX_ORDER; cur++) {
            uint32_t idx = pmm_buddy_pop(zone, cur);
            if (idx == PMM_STACK_END) continue;

            while (cur > order) {
                cur--;
                uint64_t buddy_idx = idx + pmm_block_pages(cur);
                pmm_set_block_bitmap(buddy_idx, cur, false);
                pmm_buddy_push(zone, cur, buddy_idx);
            }

            pmm_set_block_bitmap(idx, order, true);
            metadata[idx].flags = (order & PMM_BUDDY_FLAG_ORDER);
            metadata[idx].next_free_idx = PMM_STACK_END;
            free_frames -= pmm_block_pages(order);
            return ((uint64_t)idx * PAGE_SIZE);
        }
    }

    return 0;
}

static void pmm_buddy_free_block(uint64_t idx, uint8_t order) {
    pmm_zone_t zone = pmm_zone_from_phys(idx * PAGE_SIZE);
    uint64_t pages = pmm_block_pages(order);
    pmm_set_block_bitmap(idx, order, false);
    free_frames += pages;

    while (order < PMM_BUDDY_MAX_ORDER) {
        uint64_t buddy_idx = idx ^ pmm_block_pages(order);
        if (buddy_idx >= total_frames) break;
        if (!pmm_block_fits_zone(buddy_idx, order, zone)) break;
        if (!pmm_block_is_head(buddy_idx, order)) break;
        if (bitmap_test(buddy_idx)) break;
        if (!pmm_buddy_remove(zone, order, buddy_idx)) break;

        if (buddy_idx < idx) idx = buddy_idx;
        order++;
    }

    pmm_buddy_push(zone, order, idx);
}

static inline bool pmm_needs_temporal_scour(uint8_t frame_epoch, uint64_t alloc_epoch) {
    return frame_epoch != (uint8_t)alloc_epoch;
}

void pmm_init(void) {
    pmm_emit_profile_marker();

    if (!memmap_request.response || !hhdm_request.response) {
        pmm_fail_closed("missing_limine_memmap_or_hhdm");
    }

    hhdm_offset = hhdm_request.response->offset;
    
    // 1. Find the highest physical address to size the bitmap and metadata
    // We only care about addresses we might actually use.
    max_address = 0;
    uint64_t usable_entries = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (entry->length == 0) {
            continue;
        }
        if (entry->base > UINT64_MAX - entry->length) {
            pmm_fail_closed("memmap_entry_overflow");
        }
        if (entry->type == LIMINE_MEMMAP_USABLE || 
            entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
            uint64_t end = entry->base + entry->length;
            if (end > max_address) max_address = end;
            if (entry->type == LIMINE_MEMMAP_USABLE) usable_entries++;
        }
    }

    if (max_address < PAGE_SIZE) {
        pmm_fail_closed("no_usable_physical_address_space");
    }

    total_frames = max_address / PAGE_SIZE;
    if (total_frames == 0) {
        pmm_fail_closed("zero_total_frames");
    }

    uint64_t bitmap_size = (total_frames + 7ULL) / 8ULL;
    if (total_frames > (UINT64_MAX / sizeof(struct frame_metadata))) {
        pmm_fail_closed("metadata_size_overflow");
    }
    uint64_t metadata_size = total_frames * sizeof(struct frame_metadata);

    // [FIX] Ensure needed size is page-aligned
    if (bitmap_size > UINT64_MAX - metadata_size) {
        pmm_fail_closed("ledger_size_overflow");
    }
    uint64_t needed = bitmap_size + metadata_size;
    needed = align_up_u64(needed, PAGE_SIZE);
    if (needed == UINT64_MAX) {
        pmm_fail_closed("ledger_alignment_overflow");
    }
    
    uint64_t ledger_phys = 0;
    bool ledger_found = false;

    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE || entry->length == 0) {
            continue;
        }

        uint64_t region_start = align_up_u64(entry->base, PAGE_SIZE);
        uint64_t region_end = align_down_u64(entry->base + entry->length, PAGE_SIZE);
        if (region_start == UINT64_MAX || region_end <= region_start) {
            continue;
        }

        if (region_end - region_start >= needed) {
            ledger_phys = region_start;
            ledger_found = true;
            break;
        }
    }

    if (!ledger_found) {
        pmm_fail_closed("ledger_region_not_found");
    }

    bitmap = (uint8_t*)phys_to_virt(ledger_phys);
    metadata = (struct frame_metadata*)phys_to_virt(ledger_phys + bitmap_size);
    pmm_reserved_ledger_frames = needed / PAGE_SIZE;

    kprintf("[PMM-AUDIT] memmap_entries=%lu usable_entries=%lu total_frames=%lu\n",
            memmap_request.response->entry_count,
            usable_entries,
            total_frames);
#ifdef PMM_AUDIT_DEBUG
    kprintf("[PMM-AUDIT] ledger_phys=0x%lx ledger_bytes=%lu\n", ledger_phys, needed);
#else
    kprintf("[PMM-AUDIT] ledger_frame=%lu ledger_frames=%lu\n",
            (ledger_phys / PAGE_SIZE),
            pmm_reserved_ledger_frames);
#endif

    // 3. Initialize: All memory is USED by default
    for (uint64_t i = 0; i < bitmap_size; i++) bitmap[i] = 0xFF;
    for (uint64_t i = 0; i < total_frames; i++) {
        metadata[i].owner_token = 0;
        metadata[i].color = COLOR_VOID;
        metadata[i].next_free_idx = PMM_STACK_END;
        metadata[i].state = 0;
        metadata[i].flags = 0;
        metadata[i].epoch = 0;
    }

    // 4. Clear bits for actually usable regions and populate the free list
    free_frames = 0;
    pmm_quarantine_frames = 0;
    pmm_candidate_frames = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap_request.response->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t entry_end = entry->base + entry->length;
            uint64_t start = align_up_u64(entry->base, PAGE_SIZE);
            uint64_t end = align_down_u64(entry_end, PAGE_SIZE);

            if (start == UINT64_MAX || end <= start) {
                continue;
            }

            pmm_candidate_frames += (end - start) / PAGE_SIZE;
            pmm_quarantine_frames += (start - entry->base) / PAGE_SIZE;
            pmm_quarantine_frames += (entry_end - end) / PAGE_SIZE;

            for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
                // [FIX] Explicitly skip the ledger region
                if (addr >= ledger_phys && addr < (ledger_phys + needed)) {
                    continue;
                }

                uint64_t idx = addr / PAGE_SIZE;
                if (idx < total_frames) {
                    bitmap_clear(idx);
                    metadata[idx].state = FRAME_STATE_FREE;
                    free_frames++;
                } else {
                    pmm_quarantine_frames++;
                }
            }
        }
    }

    pmm_buddy_init_from_free_frames();

    uint64_t quarantine_pct = u64_percent(pmm_quarantine_frames, pmm_candidate_frames);
    kprintf("[PMM-QUAR] candidates=%lu quarantined=%lu pct=%lu\n",
            pmm_candidate_frames,
            pmm_quarantine_frames,
            quarantine_pct);
    kprintf("[PMM-AUDIT] free_frames=%lu reserved_ledger_frames=%lu\n",
            free_frames,
            pmm_reserved_ledger_frames);

    if (free_frames < PMM_MIN_FREE_FRAMES) {
        pmm_fail_closed("free_frame_floor_breach");
    }

    if (pmm_profile == PMM_PROFILE_STRICT && quarantine_pct > PMM_MAX_QUARANTINE_PERCENT) {
        pmm_fail_closed("quarantine_ratio_exceeded");
    }
}

uint64_t pmm_alloc_ex(const pmm_alloc_policy_t* policy) {
    if (!policy) return 0;

    if (!pmm_policy_marker_emitted) {
        kprintf("[PMM-AUDIT] policy-path enabled (zone/trust/order compatibility mode).\n");
        pmm_policy_marker_emitted = true;
    }

    if (policy->order > PMM_BUDDY_MAX_ORDER) {
        kprintf("[PMM-FAIL] phase=alloc_ex reason=order_out_of_range order=%u max=%u\n",
                policy->order,
                PMM_BUDDY_MAX_ORDER);
        return 0;
    }

    if (policy->trust_level == PMM_TRUST_UNVERIFIED && pmm_profile == PMM_PROFILE_STRICT) {
        kprintf("[PMM-FAIL] phase=alloc_ex reason=trust_level_rejected profile=strict\n");
        return 0;
    }

    uint64_t flags = spinlock_irqsave(&pmm_lock);
    uint64_t phys = pmm_buddy_alloc_block(policy->order, policy->preferred_zone);
    if (!phys) {
        spinlock_irqrestore(&pmm_lock, flags);
        return 0;
    }

    uint64_t alloc_epoch = mode_get_security_epoch();
    uint64_t pages = pmm_block_pages(policy->order);
    uint64_t base_idx = phys / PAGE_SIZE;
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t frame_idx = base_idx + i;
        uint64_t frame_phys = phys + (i * PAGE_SIZE);
        void* virt = phys_to_virt(frame_phys);
        if (pmm_needs_temporal_scour(metadata[frame_idx].epoch, alloc_epoch)) {
            hyper_scrub(virt, PAGE_SIZE / 8);
            law9_temporal_scour_count++;
            if (!law9_marker_emitted) {
                kprintf("[LAW9] Temporal scouring active (epoch mismatch path engaged).\n");
                law9_marker_emitted = true;
            }
        } else {
            fast_zero(virt, PAGE_SIZE);
            law9_same_epoch_zero_count++;
        }

        metadata[frame_idx].owner_token = policy->owner_token;
        metadata[frame_idx].color = policy->color;
        metadata[frame_idx].state = 0;
        metadata[frame_idx].epoch = (uint8_t)alloc_epoch;
        if (i != 0) metadata[frame_idx].flags = 0;
    }

    spinlock_irqrestore(&pmm_lock, flags);
    return phys;
}

uint64_t pmm_alloc(security_color_t color, uint64_t owner) {
    pulse_start();
    pmm_alloc_policy_t policy = {
        .color = color,
        .owner_token = owner,
        .preferred_zone = PMM_ZONE_ANY,
        .trust_level = PMM_TRUST_VERIFIED,
        .order = 0
    };
    uint64_t phys = pmm_alloc_ex(&policy);
    if (phys != 0) {
        pulse_end(KLOG_GATE | KLOG_ASSERTED, "PMM_ALLOC");
    }
    return phys;
}

void pmm_free(uint64_t phys) {
    pmm_free_ex(phys, 0);
}

void pmm_free_ex(uint64_t phys, uint8_t order) {
    uint64_t idx = phys / PAGE_SIZE;
    if (idx >= total_frames) return;
    if (order > PMM_BUDDY_MAX_ORDER) {
        kprintf("[PMM-FAIL] phase=free_ex reason=order_out_of_range order=%u max=%u\n",
                order,
                PMM_BUDDY_MAX_ORDER);
        return;
    }

    uint64_t flags = spinlock_irqsave(&pmm_lock);
    uint64_t free_epoch = mode_get_security_epoch();
    uint64_t pages = pmm_block_pages(order);

    for (uint64_t i = 0; i < pages; i++) {
        uint64_t frame_idx = idx + i;
        uint64_t frame_phys = phys + (i * PAGE_SIZE);
        if (frame_idx >= total_frames) break;

        void* virt = phys_to_virt(frame_phys);
        if (metadata[frame_idx].color == COLOR_SECURE || metadata[frame_idx].color == COLOR_GHOST) {
            hyper_scrub(virt, PAGE_SIZE / 8);
            selective_cache_flush(virt, PAGE_SIZE);
        } else {
            fast_zero(virt, PAGE_SIZE);
        }

        metadata[frame_idx].owner_token = 0;
        metadata[frame_idx].color = COLOR_VOID;
        metadata[frame_idx].state = FRAME_STATE_FREE;
        metadata[frame_idx].epoch = (uint8_t)free_epoch;
        metadata[frame_idx].flags = 0;
    }

    pmm_buddy_free_block(idx, order);
    spinlock_irqrestore(&pmm_lock, flags);
}

bool pmm_transfer(uint64_t phys, uint64_t new_owner, security_color_t new_color, uint8_t new_state) {
    uint64_t idx = phys / PAGE_SIZE;
    uint64_t flags = spinlock_irqsave(&pmm_lock);
    if (idx >= total_frames || bitmap_test(idx) == false) {
        spinlock_irqrestore(&pmm_lock, flags);
        return false;
    }

    metadata[idx].owner_token = new_owner;
    metadata[idx].color = new_color;
    metadata[idx].state = new_state;
    metadata[idx].epoch = (uint8_t)mode_get_security_epoch();
    spinlock_irqrestore(&pmm_lock, flags);
    return true;
}

void pmm_stats(uint64_t *total, uint64_t *free) {
    uint64_t flags = spinlock_irqsave(&pmm_lock);
    if (total) *total = total_frames;
    if (free) *free = free_frames;
    spinlock_irqrestore(&pmm_lock, flags);
}

void pmm_law9_stats(uint64_t *temporal_scours, uint64_t *same_epoch_zeroes) {
    uint64_t flags = spinlock_irqsave(&pmm_lock);
    if (temporal_scours) *temporal_scours = law9_temporal_scour_count;
    if (same_epoch_zeroes) *same_epoch_zeroes = law9_same_epoch_zero_count;
    spinlock_irqrestore(&pmm_lock, flags);
}

void* pmm_phys_to_virt(uint64_t phys) {
    return phys_to_virt(phys);
}

uint64_t pmm_virt_to_phys(void* virt) {
    return (uint64_t)virt - hhdm_offset;
}
