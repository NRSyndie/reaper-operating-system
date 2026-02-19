#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "include/slab.h"
#include "include/pmm.h"
#include "include/mode.h"
#include "include/utils.h"
#include "include/klog.h"
#include "include/kmalloc.h"

#define SLAB_MAGIC 0x534C4142u /* 'SLAB' */
#define SLAB_REDZONE_CANARY 0xD00DFEEDCAFEBEEFULL
#define SLAB_POISON_BYTE 0xA5

// Global list of all caches (for global annihilation)
static slab_cache_t* global_cache_list = NULL;
static spinlock_t global_slab_lock = 0;

static inline uint8_t slab_all_mode_mask(void) {
    uint8_t mask = 0;
    for (int i = 0; i < MODE_COUNT && i < 8; i++) {
        mask |= (1u << i);
    }
    return mask;
}

static inline bool slab_policy_allows_mode(const slab_cache_t* cache, mode_id_t mode) {
    if (!cache || mode < 0 || mode >= MODE_COUNT) return false;
    return (cache->policy.mode_mask & SLAB_MODE_MASK(mode)) != 0;
}

static inline void slab_log_policy_violation(const slab_cache_t* cache, mode_id_t mode, const char* op) {
    if (!cache || cache->policy.audit_class == SLAB_AUDIT_NONE) return;

    if (cache->policy.audit_class == SLAB_AUDIT_STRICT) {
        klog_error("SLAB[%s]: policy denied op=%s mode=%u mask=0x%x", cache->name, op,
                   (unsigned)mode, (unsigned)cache->policy.mode_mask);
    } else {
        klog_warn("SLAB[%s]: policy denied op=%s mode=%u", cache->name, op, (unsigned)mode);
    }
}

static inline void list_push(struct slab** head, struct slab* s) {
    s->prev = NULL;
    s->next = *head;
    if (*head) (*head)->prev = s;
    *head = s;
}

static inline void list_remove(struct slab** head, struct slab* s) {
    if (s->prev) s->prev->next = s->next;
    if (s->next) s->next->prev = s->prev;
    if (*head == s) *head = s->next;
    s->next = NULL;
    s->prev = NULL;
}

static inline int bitmap_find_free_slot(struct slab* s) {
    if (!s) return -1;

    uint16_t start = s->free_hint;
    if (start >= s->max_objects) start = 0;

    for (uint16_t n = 0; n < s->max_objects; n++) {
        uint16_t idx = (uint16_t)((start + n) % s->max_objects);
        uint64_t word = s->free_bitmap[idx / 64];
        uint64_t mask = (1ULL << (idx % 64));
        if (word & mask) {
            s->free_hint = (uint16_t)((idx + 1) % s->max_objects);
            return (int)idx;
        }
    }

    return -1;
}

static inline void bitmap_mark_used(struct slab* s, int idx) {
    s->free_bitmap[idx / 64] &= ~(1ULL << (idx % 64));
    s->in_use_count++;
}

static inline void bitmap_mark_free(struct slab* s, int idx) {
    s->free_bitmap[idx / 64] |= (1ULL << (idx % 64));
    s->in_use_count--;
}

static inline void slab_set_redzone(const slab_cache_t* cache, void* obj) {
    if (!cache->policy.enable_redzone) return;
    if (cache->object_size < cache->requested_size + sizeof(uint64_t)) return;

    uint64_t* canary_ptr = (uint64_t*)((uintptr_t)obj + cache->requested_size);
    *canary_ptr = SLAB_REDZONE_CANARY;
}

static inline void slab_check_redzone(const slab_cache_t* cache, const void* obj) {
    if (!cache->policy.enable_redzone) return;
    if (cache->object_size < cache->requested_size + sizeof(uint64_t)) return;

    const uint64_t* canary_ptr = (const uint64_t*)((uintptr_t)obj + cache->requested_size);
    if (*canary_ptr != SLAB_REDZONE_CANARY) {
        kpanic("SLAB: Redzone canary corruption detected");
    }
}

static struct slab* slab_allocate_page(slab_cache_t* cache, mode_id_t mode) {
    security_color_t color = mode_to_color(mode);
    uint64_t phys = pmm_alloc(color, 0);
    if (!phys) return NULL;

    struct slab* s = (struct slab*)pmm_phys_to_virt(phys);
    fast_zero(s, PAGE_SIZE);

    s->cache = cache;
    s->mode_id = (uint8_t)mode;
    s->state = SLAB_STATE_FREE;
    s->free_hint = 0;
    s->in_use_count = 0;
    s->max_objects = (uint16_t)((PAGE_SIZE - sizeof(struct slab)) / cache->object_size);
    if (s->max_objects > SLAB_MAX_OBJECTS) s->max_objects = SLAB_MAX_OBJECTS;
    s->magic = SLAB_MAGIC;

    for (int i = 0; i < SLAB_BITMAP_WORDS; i++) s->free_bitmap[i] = 0;
    for (uint16_t i = 0; i < s->max_objects; i++) {
        s->free_bitmap[i / 64] |= (1ULL << (i % 64));
    }

    cache->metrics.slab_new++;
    cache->total_pages++;
    return s;
}

void slab_get_default_policy(slab_policy_t* out_policy) {
    if (!out_policy) return;

    out_policy->mode_mask = slab_all_mode_mask();
    out_policy->scrub_on_alloc = 1;
    out_policy->scrub_on_free = 0;
    out_policy->scrub_on_annihilate = 1;
    out_policy->poison_on_free = 0;
    out_policy->enable_redzone = 0;
    out_policy->lifetime_class = SLAB_LIFETIME_DEFAULT;
    out_policy->audit_class = SLAB_AUDIT_BASIC;
    out_policy->min_partial_slabs = 1;
}

bool slab_get_metrics(const slab_cache_t* cache, slab_metrics_t* out_metrics) {
    if (!cache || !out_metrics) return false;

    uint64_t flags = spinlock_irqsave((spinlock_t*)&cache->lock);
    memcpy(out_metrics, &cache->metrics, sizeof(slab_metrics_t));
    spinlock_irqrestore((spinlock_t*)&cache->lock, flags);
    return true;
}

void slab_init(void) {
    global_cache_list = NULL;
    kmalloc_init();
}

slab_cache_t* slab_create_cache_ex(const char* name, size_t size, size_t align, const slab_policy_t* policy) {
    if (!name || size == 0) return NULL;

    slab_cache_t* c = (slab_cache_t*)kmalloc(sizeof(slab_cache_t));
    if (!c) {
        uint64_t phys = pmm_alloc(COLOR_VOID, 0);
        if (!phys) return NULL;
        c = (slab_cache_t*)pmm_phys_to_virt(phys);
    }

    fast_zero(c, sizeof(slab_cache_t));

    for (int i = 0; i < SLAB_NAME_MAX - 1 && name[i]; i++) {
        c->name[i] = name[i];
    }

    if (align < 8) align = 8;
    if (size % align != 0) {
        size = (size + align) - (size % align);
    }

    c->requested_size = size;

    if (policy && policy->enable_redzone) {
        size_t with_redzone = size + sizeof(uint64_t);
        if (with_redzone % align != 0) {
            with_redzone = (with_redzone + align) - (with_redzone % align);
        }
        size = with_redzone;
    }

    if (size + sizeof(struct slab) > PAGE_SIZE) {
        return NULL;
    }

    c->object_size = size;
    c->alignment = align;
    c->lock = 0;

    if (policy) {
        memcpy(&c->policy, policy, sizeof(slab_policy_t));
        if (c->policy.mode_mask == 0) c->policy.mode_mask = slab_all_mode_mask();
    } else {
        slab_get_default_policy(&c->policy);
    }

    uint64_t flags = spinlock_irqsave(&global_slab_lock);
    c->next = global_cache_list;
    global_cache_list = c;
    spinlock_irqrestore(&global_slab_lock, flags);

    return c;
}

slab_cache_t* slab_create_cache(const char* name, size_t size, size_t align) {
    return slab_create_cache_ex(name, size, align, NULL);
}

void* slab_alloc(slab_cache_t* cache) {
    if (!cache) return NULL;

    uint64_t flags = spinlock_irqsave(&cache->lock);
    mode_id_t mode = mode_get_current();

    if (!slab_policy_allows_mode(cache, mode)) {
        cache->metrics.policy_denied++;
        slab_log_policy_violation(cache, mode, "alloc");
        spinlock_irqrestore(&cache->lock, flags);
        return NULL;
    }

    struct slab* s = cache->partial_slabs[mode];
    if (!s) {
        s = cache->free_slabs[mode];
        if (s) {
            list_remove(&cache->free_slabs[mode], s);
            s->state = SLAB_STATE_PARTIAL;
            list_push(&cache->partial_slabs[mode], s);
        }
    }

    if (!s) {
        cache->metrics.cache_miss++;
        s = slab_allocate_page(cache, mode);
        if (!s) {
            cache->metrics.alloc_fail++;
            spinlock_irqrestore(&cache->lock, flags);
            return NULL;
        }

        s->state = SLAB_STATE_PARTIAL;
        list_push(&cache->partial_slabs[mode], s);
    }

    int slot = bitmap_find_free_slot(s);
    if (slot < 0) {
        kpanic("SLAB: Partial slab has no free slot");
    }

    bitmap_mark_used(s, slot);
    cache->total_objects++;
    cache->metrics.alloc_ok++;

    if (s->in_use_count == s->max_objects) {
        list_remove(&cache->partial_slabs[mode], s);
        s->state = SLAB_STATE_FULL;
        list_push(&cache->full_slabs[mode], s);
    }

    uintptr_t base = (uintptr_t)s;
    void* obj = (void*)(base + sizeof(struct slab) + ((size_t)slot * cache->object_size));

    if (cache->policy.scrub_on_alloc) {
        fast_zero(obj, cache->object_size);
    }

    slab_set_redzone(cache, obj);

    spinlock_irqrestore(&cache->lock, flags);
    return obj;
}

static inline bool slab_validate_object_ptr(const slab_cache_t* cache, const struct slab* s, const void* obj, int* out_slot) {
    uintptr_t addr = (uintptr_t)obj;
    uintptr_t payload = (uintptr_t)s + sizeof(struct slab);

    if (addr < payload) return false;

    uintptr_t offset = addr - payload;
    if ((offset % cache->object_size) != 0) return false;

    uintptr_t slot = offset / cache->object_size;
    if (slot >= s->max_objects) return false;

    if (out_slot) *out_slot = (int)slot;
    return true;
}

void slab_free(slab_cache_t* cache, void* obj) {
    if (!obj || !cache) return;

    uint64_t flags = spinlock_irqsave(&cache->lock);

    uintptr_t addr = (uintptr_t)obj;
    struct slab* s = (struct slab*)(addr & ~(PAGE_SIZE - 1));

    if (s->magic != SLAB_MAGIC || s->cache != cache) {
        cache->metrics.invalid_free++;
        spinlock_irqrestore(&cache->lock, flags);
        kpanic("SLAB: Cross-cache or invalid free attempt");
    }

    int slot = -1;
    if (!slab_validate_object_ptr(cache, s, obj, &slot)) {
        cache->metrics.invalid_free++;
        spinlock_irqrestore(&cache->lock, flags);
        kpanic("SLAB: Invalid free pointer shape");
    }

    if ((s->free_bitmap[slot / 64] & (1ULL << (slot % 64))) != 0) {
        cache->metrics.double_free++;
        spinlock_irqrestore(&cache->lock, flags);
        kpanic("SLAB: Double free");
    }

    if (cache->policy.enable_redzone) {
        slab_check_redzone(cache, obj);
    }

    bool was_full = (s->state == SLAB_STATE_FULL);

    if (cache->policy.scrub_on_free) {
        fast_zero(obj, cache->object_size);
    } else if (cache->policy.poison_on_free) {
        memset(obj, SLAB_POISON_BYTE, cache->requested_size);
    }

    bitmap_mark_free(s, slot);
    if (cache->total_objects > 0) cache->total_objects--;
    cache->metrics.free_ok++;

    if (was_full) {
        list_remove(&cache->full_slabs[s->mode_id], s);
        s->state = SLAB_STATE_PARTIAL;
        list_push(&cache->partial_slabs[s->mode_id], s);
    }

    if (s->in_use_count == 0) {
        list_remove(&cache->partial_slabs[s->mode_id], s);

        if (cache->policy.min_partial_slabs > 0) {
            uint16_t free_count = 0;
            struct slab* it = cache->free_slabs[s->mode_id];
            while (it && free_count < cache->policy.min_partial_slabs) {
                free_count++;
                it = it->next;
            }

            if (free_count < cache->policy.min_partial_slabs) {
                s->state = SLAB_STATE_FREE;
                list_push(&cache->free_slabs[s->mode_id], s);
                spinlock_irqrestore(&cache->lock, flags);
                return;
            }
        }

        if (cache->policy.scrub_on_annihilate) {
            fast_zero(s, PAGE_SIZE);
        }

        if (cache->total_pages > 0) cache->total_pages--;
        cache->metrics.slab_reclaimed++;
        pmm_free(pmm_virt_to_phys(s));
    }

    spinlock_irqrestore(&cache->lock, flags);
}

static void slab_annihilate_list(slab_cache_t* cache, struct slab* head) {
    struct slab* curr = head;
    while (curr) {
        struct slab* next = curr->next;

        if (cache->policy.scrub_on_annihilate) {
            fast_zero(curr, PAGE_SIZE);
        }

        if (cache->total_objects >= curr->in_use_count) {
            cache->total_objects -= curr->in_use_count;
        } else {
            cache->total_objects = 0;
        }

        if (cache->total_pages > 0) cache->total_pages--;
        cache->metrics.slabs_annihilated++;
        pmm_free(pmm_virt_to_phys(curr));
        curr = next;
    }
}

void slab_annihilate(slab_cache_t* cache, int mode_id) {
    if (!cache || mode_id < 0 || mode_id >= MODE_COUNT) return;

    uint64_t flags = spinlock_irqsave(&cache->lock);

    slab_annihilate_list(cache, cache->free_slabs[mode_id]);
    slab_annihilate_list(cache, cache->partial_slabs[mode_id]);
    slab_annihilate_list(cache, cache->full_slabs[mode_id]);

    cache->free_slabs[mode_id] = NULL;
    cache->partial_slabs[mode_id] = NULL;
    cache->full_slabs[mode_id] = NULL;

    spinlock_irqrestore(&cache->lock, flags);
}

void slab_annihilate_all(int mode_id) {
    if (mode_id < 0 || mode_id >= MODE_COUNT) return;

    uint64_t flags = spinlock_irqsave(&global_slab_lock);
    slab_cache_t* curr = global_cache_list;
    while (curr) {
        slab_annihilate(curr, mode_id);
        curr = curr->next;
    }
    spinlock_irqrestore(&global_slab_lock, flags);
}
