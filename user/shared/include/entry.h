#ifndef SHARED_ENTRY_H
#define SHARED_ENTRY_H

#include <stdint.h>

typedef uint64_t lease_id_t;

/**
 * Day 11 Entry Lease
 * Anchors an occupant's execution to a specific authority epoch and reality.
 */
typedef struct {
    lease_id_t id;
    uint64_t   epoch;
    uint32_t   authority_mask;
    uint32_t   mode_mask;
    uint64_t   expiry_tsc;
} entry_lease_t;

/* 
 * Entry Markers for matrix verification.
 * These must be emitted by the kernel during the entry pipeline.
 */
#define ENTRY_MARKER_COMPILE "[ENTRY_COMPILE]"
#define ENTRY_MARKER_VERIFY  "[ENTRY_VERIFY]"
#define ENTRY_MARKER_APPLY   "[ENTRY_APPLY]"
#define ENTRY_MARKER_ATTEST  "[ENTRY_ATTEST]"

#define ENTRY_REJECT_MODE    "[ENTRY_REJECT] MODE_MISMATCH"
#define ENTRY_REJECT_EPOCH   "[ENTRY_REJECT] EPOCH_STALE"
#define ENTRY_REJECT_AUTH    "[ENTRY_REJECT] AUTH_DENIED"

#endif /* SHARED_ENTRY_H */
