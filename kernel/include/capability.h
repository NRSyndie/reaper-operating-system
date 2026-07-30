#ifndef REAPER_CAPABILITY_H
#define REAPER_CAPABILITY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * REAPER-OS CAPABILITY SYSTEM (THE RUNE & THE WEB)
 * 
 * DESIGNATION IS AUTHORITY.
 * Possession of a capability (Rune) provides the right to perform
 * actions on the referenced object. No ACLs are checked.
 * 
 * Revocation and derivation are lineage-aware.
 * Identities are tracked via parent/child links and revocation epoch checks.
 */

typedef enum {
    CAP_TYPE_NONE = 0,
    CAP_TYPE_GENESIS,  /* THE PRIMORDIAL AUTHORITY (Genesis Cap) */
    CAP_TYPE_CNODE,    /* Capability to a CNode (Recursive Authority) */
    CAP_TYPE_RAM,      /* Capability to a Physical RAM Range */
    CAP_TYPE_THREAD,   /* Capability to a Thread Control Block */
    CAP_TYPE_ENDPOINT, /* IPC Endpoint */
    CAP_TYPE_PCID,     /* Hardware Address Space Authority */
    CAP_TYPE_PAGETABLE,/* Page Table Authority (PML4, PDPT, PD, PT) */
    CAP_TYPE_LATTICE,  /* Shared Memory Lattice Authority */
    CAP_TYPE_AUDITOR,  /* Fate String Auditing Authority */
    CAP_TYPE_SCHED_AUTH_ROOT,   /* Process scheduling root authority */
    CAP_TYPE_SCHED_AUTH_THREAD, /* Thread scheduling derived authority */
    CAP_TYPE_REALITY_CTRL,      /* Reality control authority */
    CAP_TYPE_AUDIT_WRITE,       /* Audit record emission authority */
    CAP_TYPE_SCHED_AUTH,        /* Broad process-level scheduling authority */
    CAP_MAX_TYPES
} cap_type_t;

/* Rights Bitmask */
#define CAP_RIGHT_READ    (1 << 0)
#define CAP_RIGHT_WRITE   (1 << 1)
#define CAP_RIGHT_GRANT   (1 << 2) /* Ability to copy/mint the cap */
#define CAP_RIGHT_INVOKE  (1 << 3) /* Ability to 'call' the object */
#define CAP_RIGHT_EXECUTE (1 << 4) /* Ability to execute memory */

/* Mode Constraint Bitmask (Conditional Runes) */
#define CAP_MODE_VOID     (1 << 0)
#define CAP_MODE_CASUAL   (1 << 1)
#define CAP_MODE_SECURE   (1 << 2)
#define CAP_MODE_LOCKDOWN (1 << 3)
#define CAP_MODE_GHOST    (1 << 4)
#define CAP_MODE_VALID_MASK (CAP_MODE_VOID | CAP_MODE_CASUAL | CAP_MODE_SECURE | CAP_MODE_LOCKDOWN | CAP_MODE_GHOST)
#define CAP_MODE_ALL      CAP_MODE_VALID_MASK

_Static_assert(CAP_MODE_VALID_MASK == CAP_MODE_ALL, "cap mode mask drift");

typedef enum {
    CAP_STATE_ACTIVE = 0,
    CAP_STATE_REVOKED,
    CAP_STATE_GHOST
} cap_state_t;

/* Forward declaration */
struct cap_identity;
typedef struct cap_identity cap_identity_t;

#include "utils.h" /* For spinlock_t */

/**
 * cap_identity_t (The Spirit)
 * The actual authority and lineage metadata.
 * 
 * DESIGN: Void-Optimal Lineage
 * We use 64-bit epochs to enable O(1) revocation checks.
 * If (parent->revocation_epoch != parent_epoch_start), the child is dead.
 */
struct cap_identity {
    /* --- Immutable Header (Set at Mint) --- */
    uint64_t object_ptr;
    uint8_t  type;
    uint8_t  allowed_modes;
    uint16_t rights;
    uint32_t badge;

    uint64_t birth_epoch;        /* Global epoch when this was born */
    uint64_t parent_epoch_start; /* parent->revocation_epoch at birth */
    struct cap_identity* parent; /* Immutable parent pointer */

    /* --- Mutable State --- */
    volatile uint64_t revocation_epoch; /* 0 if alive, else epoch of death */
    volatile uint32_t ref_count;        /* Number of slots pointing here */
    
    /* --- Lineage (Used by the Resource Reaper for cleanup) --- */
    struct cap_identity* first_child;
    struct cap_identity* next_sibling;
    struct cap_identity* prev_sibling;

    spinlock_t lock; /* Protects lineage links (first_child/siblings) */
    uint32_t magic;
};

typedef struct {
    uint64_t lookup_ok;
    uint64_t lookup_miss;
    uint64_t insert_ok;
    uint64_t insert_fail;
    uint64_t delete_ok;
    uint64_t mint_ok;
    uint64_t mint_fail;
    uint64_t copy_ok;
    uint64_t copy_fail;
    uint64_t retype_ok;
    uint64_t retype_fail;
    uint64_t revoke_ops;
    uint64_t identities_freed;
    uint64_t policy_deny;
} cap_metrics_t;

/**
 * capability_t (The Rune/Slot)
 * A simple reference to an identity.
 */
typedef struct {
    cap_identity_t* ptr;
} capability_t;

/**
 * cnode_t (The Keyring)
 * A fixed-size array of capability slots.
 */
#define CNODE_SLOTS 128
typedef struct {
    spinlock_t lock;
    capability_t slots[CNODE_SLOTS];
} cnode_t;

/* --- The Verb API --- */

/**
 * cap_lookup: O(1) translation from Handle to Identity.
 * Returns NULL if slot is empty, identity is revoked, or NOT allowed in current Mode.
 */
cap_identity_t* cap_lookup(cnode_t* root, uint32_t cptr);

/**
 * cap_insert: Place an identity into a slot.
 * Returns -1 if slot is not empty.
 */
int cap_insert(cnode_t* root, uint32_t cptr, cap_identity_t* ident);

/**
 * cap_delete: Removes a reference to an identity from a slot.
 * May trigger ghosting or deletion of the identity.
 */
void cap_delete(cnode_t* root, uint32_t cptr);

/**
 * cap_mint: Derive a new identity with restricted rights/modes.
 */
int cap_mint(cnode_t* root, uint32_t src_cptr, uint32_t dst_cptr, uint16_t new_rights, uint32_t badge, uint8_t allowed_modes);

/**
 * cap_copy: Create a new slot pointing to the same identity.
 */
int cap_copy(cnode_t* root, uint32_t src_cptr, uint32_t dst_cptr);

/**
 * cap_retype: Derive a new identity with a different type while preserving lineage.
 * Current policy allows RAM -> PAGETABLE.
 */
int cap_retype(cnode_t* root, uint32_t src_cptr, uint32_t dst_cptr, uint16_t new_type, uint32_t badge);

/**
 * cap_revoke: Recursively revoke a capability and all its descendants.
 */
void cap_revoke(cnode_t* root, uint32_t cptr);

/**
 * cap_identity_create: Internal helper to create a raw identity.
 */
cap_identity_t* cap_identity_create(uint64_t obj, uint16_t type, uint16_t rights, uint32_t badge, uint8_t allowed_modes);

/**
 * cnode_create: Allocate and initialize a new CNode.
 */
cnode_t* cnode_create(void);

/**
 * cnode_destroy: Zero and free a CNode.
 */
void cnode_destroy(cnode_t* cnode);

/**
 * @brief Background reaper for ghost identities.
 */
void cap_reaper(void);

/**
 * @brief Free a detached identity (cleanup helper).
 */
void cap_identity_free(cap_identity_t* ident);
bool cap_get_metrics(cap_metrics_t* out_metrics);
void cap_reset_metrics(void);
bool cap_genesis_is_exhausted(void);
bool process_has_capability_at(const void* proc, uint32_t slot, cap_type_t expected_type, uint16_t required_rights);

#endif /* REAPER_CAPABILITY_H */


