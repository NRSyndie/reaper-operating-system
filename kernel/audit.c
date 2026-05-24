#include "include/audit.h"
#include "include/scheduler.h"
#include "include/klog.h"
#include "include/cpu.h"
#include "include/kmalloc.h"
#include "include/utils.h"
#include "blake3/blake3.h"

static audit_lattice_t* g_audit_lattice;
static uint64_t g_root_system_seed;
static uint64_t g_current_fate_seed;
static uint8_t g_last_hash[32];

void audit_init(void) {
    g_audit_lattice = (audit_lattice_t*)kmalloc(sizeof(audit_lattice_t));
    if (!g_audit_lattice) kpanic("AUDIT: Failed to allocate lattice");
    
    fast_zero(g_audit_lattice, sizeof(audit_lattice_t));
    atomic_init(&g_audit_lattice->head, 0);
    atomic_init(&g_audit_lattice->tail, 0);
    g_audit_lattice->overflow_count = 0;
    
    uint64_t seed = 0;
    bool seed_ok = false;
    if (cpu_has_rdrand()) {
        for (int i = 0; i < 10; i++) {
            uint8_t ok;
            __asm__ volatile ("rdrand %0; setc %1" : "=r"(seed), "=qm"(ok));
            if (ok) {
                seed_ok = true;
                break;
            }
        }
    }
    
    if (!seed_ok) {
        seed = rdtsc(); /* WEAK SEED: TSC fallback active. MILESTONE_GHOST_HARDENING: replace with sealed storage seed before Ghost Mode. */
    }
    
    g_root_system_seed = seed;
    g_current_fate_seed = seed; // Initial seed
    
    // Initialize last hash from seed using BLAKE3
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, &seed, sizeof(seed));
    blake3_hasher_finalize(&hasher, g_last_hash, 32);
    
    klog_info("AUDIT: System initialized with root seed 0x%lx", seed);
}

void audit_rotate_seed(mode_id_t reality_id, uint64_t epoch) {
    // Current_FateSeed = BLAKE3(Root_System_Seed | Reality_ID | Epoch)
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, &g_root_system_seed, sizeof(g_root_system_seed));
    blake3_hasher_update(&hasher, &reality_id, sizeof(reality_id));
    blake3_hasher_update(&hasher, &epoch, sizeof(epoch));
    
    uint8_t full_seed[32];
    blake3_hasher_finalize(&hasher, full_seed, 32);
    // Take first 64 bits for current seed
    memcpy(&g_current_fate_seed, full_seed, sizeof(g_current_fate_seed));
    
    klog_debug("AUDIT: Seed rotated for Reality %u, Epoch %lu", (unsigned)reality_id, epoch);
}

void audit_strike(uint16_t event_type, uint16_t result_code, uint64_t target_id, audit_meta_t meta) {
    if (!g_audit_lattice) return;

    uint32_t tail = atomic_load_explicit(&g_audit_lattice->tail, memory_order_relaxed);
    uint32_t head = atomic_load_explicit(&g_audit_lattice->head, memory_order_acquire);
    
    uint32_t distance = (tail >= head) ? (tail - head) : (AUDIT_LATTICE_CAPACITY - head + tail);
    
    if (event_type != AUDIT_EVENT_OVERFLOW && distance >= AUDIT_WRITE_THRESHOLD) {
        if (g_audit_lattice->overflow_count == 0) {
            audit_meta_t overflow_meta = {0};
            audit_strike(AUDIT_EVENT_OVERFLOW, AUDIT_RESULT_ERROR, distance, overflow_meta);
        }
        g_audit_lattice->overflow_count++;
        return;
    }

    uint32_t next_tail = (tail + 1) % AUDIT_LATTICE_CAPACITY;
    if (next_tail == head) {
        g_audit_lattice->overflow_count++;
        return;
    }

    audit_record_t* rec = &g_audit_lattice->records[tail];
    rec->timestamp = scheduler_get_global_tick();
    memcpy(rec->prev_hash, g_last_hash, 32);
    
    thread_t* curr = scheduler_get_current();
    if (curr && curr->owner) {
        rec->actor_id = (uint64_t)curr->owner->mode << 48 | (uint32_t)curr->tid;
    } else {
        rec->actor_id = 0;
    }
    
    rec->target_id = target_id;
    rec->event_type = event_type;
    rec->result_code = result_code;
    rec->metadata = meta;
    rec->gap_seq = g_audit_lattice->overflow_count;
    
    // BLAKE3 Hash Record Chain: BLAKE3(prev_hash | record_content | FateSeed)
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, rec->prev_hash, 32);
    blake3_hasher_update(&hasher, &rec->timestamp, 8);
    blake3_hasher_update(&hasher, &rec->actor_id, 8);
    blake3_hasher_update(&hasher, &rec->target_id, 8);
    blake3_hasher_update(&hasher, &rec->event_type, 2);
    blake3_hasher_update(&hasher, &rec->result_code, 2);
    blake3_hasher_update(&hasher, &rec->gap_seq, 4);
    blake3_hasher_update(&hasher, &rec->metadata, sizeof(audit_meta_t));
    blake3_hasher_update(&hasher, &g_current_fate_seed, sizeof(g_current_fate_seed));
    
    blake3_hasher_finalize(&hasher, rec->hash, 32);
    memcpy(g_last_hash, rec->hash, 32);
    
    atomic_store_explicit(&g_audit_lattice->tail, next_tail, memory_order_release);
    
    if (g_audit_lattice->overflow_count > 0) {
        g_audit_lattice->overflow_count = 0;
    }
}
