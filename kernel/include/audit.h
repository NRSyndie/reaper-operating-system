#ifndef REAPER_AUDIT_H
#define REAPER_AUDIT_H

#include <stdint.h>
#include <stdatomic.h>
#include <assert.h>
#include "include/mode.h"

#define AUDIT_LATTICE_CAPACITY 1024
#define AUDIT_RESERVED_SLOTS 2
#define AUDIT_WRITE_THRESHOLD (AUDIT_LATTICE_CAPACITY - AUDIT_RESERVED_SLOTS)

typedef union {
    struct { uint64_t cr2, rip, err; } fault;
    struct { uint32_t tid, auth_status; } sched;
    struct { uint64_t cap_addr, rights; } cap;
    uint64_t raw[4];
} audit_meta_t;

typedef struct {
    uint64_t timestamp;        
    uint8_t prev_hash[32];     
    uint64_t actor_id;         
    /*
     * IOMMU/DMA target encoding uses:
     *   bits 63:48 = PCI segment
     *   bits 47:0  = unit index or degraded/reject reason detail
     */
    uint64_t target_id;        
    uint16_t event_type;       
    uint16_t result_code;      
    uint32_t gap_seq;          
    audit_meta_t metadata;     
    uint8_t hash[32];          
} __attribute__((packed)) audit_record_t;

static_assert(sizeof(audit_record_t) == 128, "audit record size mismatch");

typedef struct {
    atomic_uint_least32_t head;
    atomic_uint_least32_t tail;
    uint32_t overflow_count;
    audit_record_t records[AUDIT_LATTICE_CAPACITY];
} audit_lattice_t;

typedef enum {
    AUDIT_EVENT_NONE = 0,
    AUDIT_EVENT_THREAD_CREATE = 1,
    AUDIT_EVENT_THREAD_DESTROY = 2,
    AUDIT_EVENT_PHASE_SHIFT = 3,
    AUDIT_EVENT_CAP_DENIED = 4,
    AUDIT_EVENT_CAP_MINT = 5,
    AUDIT_EVENT_SCHED_STALL = 6,
    AUDIT_EVENT_OVERFLOW = 7,
    AUDIT_EVENT_IOMMU_INVENTORY = 8,
    AUDIT_EVENT_IOMMU_DEGRADED = 9,
    AUDIT_EVENT_IOMMU_UNIT_DISCOVERED = 10,
    AUDIT_EVENT_IOMMU_TOPOLOGY_REJECT = 11
} audit_event_t;

typedef enum {
    AUDIT_RESULT_OK = 0,
    AUDIT_RESULT_DENIED = 1,
    AUDIT_RESULT_ERROR = 2
} audit_result_t;

void audit_init(void);
void audit_rotate_seed(mode_id_t reality_id, uint64_t epoch);
void audit_strike(uint16_t event_type, uint16_t result_code, uint64_t target_id, audit_meta_t meta);

#endif
