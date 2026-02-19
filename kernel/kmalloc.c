#include "include/kmalloc.h"
#include "include/slab.h"
#include "include/pmm.h"
#include "include/mode.h"
#include "include/utils.h"

#define KMALLOC_BUCKET_COUNT 9
#define KMALLOC_LARGE_MAGIC 0x4B4D4C47u /* 'KMLG' */

typedef struct {
    uint32_t magic;
    uint8_t order;
    uint8_t _reserved[3];
    uint32_t requested_size;
} kmalloc_large_header_t;

static slab_cache_t* buckets[KMALLOC_BUCKET_COUNT];
static size_t bucket_sizes[KMALLOC_BUCKET_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4032
};

static inline uint8_t pages_to_order(size_t pages) {
    uint8_t order = 0;
    size_t span = 1;
    while (span < pages && order < 31) {
        span <<= 1;
        order++;
    }
    return order;
}

static void* kmalloc_large(size_t size) {
    size_t total = sizeof(kmalloc_large_header_t) + size;
    size_t pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    uint8_t order = pages_to_order(pages);

    pmm_alloc_policy_t policy;
    policy.color = mode_to_color(mode_get_current());
    policy.owner_token = 0;
    policy.preferred_zone = PMM_ZONE_ANY;
    policy.trust_level = PMM_TRUST_ANY;
    policy.order = order;

    uint64_t phys = pmm_alloc_ex(&policy);
    if (!phys) return NULL;

    kmalloc_large_header_t* hdr = (kmalloc_large_header_t*)pmm_phys_to_virt(phys);
    fast_zero(hdr, PAGE_SIZE << order);

    hdr->magic = KMALLOC_LARGE_MAGIC;
    hdr->order = order;
    hdr->requested_size = (uint32_t)size;

    return (void*)((uintptr_t)hdr + sizeof(kmalloc_large_header_t));
}

void kmalloc_init(void) {
    slab_policy_t policy;
    slab_get_default_policy(&policy);
    policy.scrub_on_alloc = 1;
    policy.scrub_on_free = 1;
    policy.scrub_on_annihilate = 1;
    policy.audit_class = SLAB_AUDIT_STRICT;
    policy.min_partial_slabs = 1;

    for (int i = 0; i < KMALLOC_BUCKET_COUNT; i++) {
        buckets[i] = slab_create_cache_ex("kmalloc-bucket", bucket_sizes[i], 16, &policy);
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    for (int i = 0; i < KMALLOC_BUCKET_COUNT; i++) {
        if (size <= bucket_sizes[i]) {
            return slab_alloc(buckets[i]);
        }
    }

    return kmalloc_large(size);
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void kfree(void* ptr) {
    if (!ptr) return;

    uintptr_t addr = (uintptr_t)ptr;
    struct slab* s = (struct slab*)(addr & ~(PAGE_SIZE - 1));
    if (s->magic == 0x534C4142u && s->cache) {
        slab_free(s->cache, ptr);
        return;
    }

    kmalloc_large_header_t* hdr = (kmalloc_large_header_t*)(addr & ~(PAGE_SIZE - 1));
    if (hdr->magic == KMALLOC_LARGE_MAGIC) {
        pmm_free_ex(pmm_virt_to_phys(hdr), hdr->order);
    }
}
