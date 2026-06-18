#include "capability.h"
#include "utils.h"
#include "klog.h"
#include "slab.h"
#include "mode.h"
#include "ipc.h"
#include "pmm.h"
#include "scheduler.h"
#include "kmalloc.h"
#include "audit.h"

extern void lattice_destroy(lattice_t* lattice);

#define CAP_IDENT_MAGIC 0x43415049u /* CAPI */

static slab_cache_t* cnode_cache = NULL;
static slab_cache_t* identity_cache = NULL;

/* Global Authority Epoch (The Universe Clock) */
static volatile uint64_t global_revocation_epoch = 1;

/* Genesis Authority exhaustion flag */
static volatile bool g_genesis_exhausted = false;

/* Global lock for lineage tree structural changes */
static spinlock_t lineage_lock = 0;
static spinlock_t cap_metrics_lock = 0;
static cap_metrics_t cap_metrics = {0};

static inline void cap_metric_inc(uint64_t* counter) {
    __atomic_fetch_add(counter, 1, __ATOMIC_RELAXED);
}

static inline bool cap_is_valid_type(uint16_t type) {
    return type > CAP_TYPE_NONE && type < CAP_MAX_TYPES;
}

static inline bool cap_allowed_modes_nonzero(uint8_t modes) {
    return modes != 0;
}

static inline bool cap_allowed_modes_valid(uint8_t modes) {
    if (!cap_allowed_modes_nonzero(modes)) return false;
    return (modes & (uint8_t)~CAP_MODE_VALID_MASK) == 0;
}

static inline bool cap_identity_is_valid(const cap_identity_t* ident) {
    return ident && ident->magic == CAP_IDENT_MAGIC;
}

static bool cap_slot_is_empty(cnode_t* root, uint32_t cptr) {
    bool empty;
    uint64_t flags = spinlock_irqsave(&root->lock);
    empty = (root->slots[cptr].ptr == NULL);
    spinlock_irqrestore(&root->lock, flags);
    return empty;
}

bool cap_get_metrics(cap_metrics_t* out_metrics) {
    if (!out_metrics) return false;

    uint64_t flags = spinlock_irqsave(&cap_metrics_lock);
    memcpy(out_metrics, &cap_metrics, sizeof(cap_metrics_t));
    spinlock_irqrestore(&cap_metrics_lock, flags);
    return true;
}

void cap_reset_metrics(void) {
    uint64_t flags = spinlock_irqsave(&cap_metrics_lock);
    fast_zero(&cap_metrics, sizeof(cap_metrics_t));
    spinlock_irqrestore(&cap_metrics_lock, flags);
}

bool cap_genesis_is_exhausted(void) {
    return __atomic_load_n(&g_genesis_exhausted, __ATOMIC_ACQUIRE);
}

bool cap_genesis_exhaust(void) {
    bool expected = false;
    return __atomic_compare_exchange_n(&g_genesis_exhausted,
                                       &expected,
                                       true,
                                       false,
                                       __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
}

cap_identity_t* cap_identity_create(uint64_t obj, uint16_t type, uint16_t rights, uint32_t badge, uint8_t allowed_modes) {
    if (!cap_is_valid_type(type)) return NULL;
    if (!cap_allowed_modes_valid(allowed_modes)) return NULL;

    if (!identity_cache) {
        slab_policy_t policy;
        slab_get_default_policy(&policy);
        policy.scrub_on_alloc = 1;
        policy.scrub_on_free = 1;
        policy.scrub_on_annihilate = 1;
        policy.audit_class = SLAB_AUDIT_STRICT;
        identity_cache = slab_create_cache_ex("CapIdentCache", sizeof(cap_identity_t), 32, &policy);
    }

    cap_identity_t* ident = (cap_identity_t*)slab_alloc(identity_cache);
    if (!ident) return NULL;

    fast_zero(ident, sizeof(cap_identity_t));
    ident->object_ptr = obj;
    ident->type = (uint8_t)type;
    ident->rights = rights;
    ident->badge = badge;
    ident->allowed_modes = allowed_modes;
    ident->birth_epoch = global_revocation_epoch;
    ident->parent_epoch_start = 0;
    ident->revocation_epoch = 0;
    ident->ref_count = 0;
    ident->lock = 0;
    ident->magic = CAP_IDENT_MAGIC;

    return ident;
}

cnode_t* cnode_create(void) {
    if (!cnode_cache) {
        slab_policy_t policy;
        slab_get_default_policy(&policy);
        policy.scrub_on_alloc = 1;
        policy.scrub_on_free = 1;
        policy.scrub_on_annihilate = 1;
        policy.audit_class = SLAB_AUDIT_STRICT;
        cnode_cache = slab_create_cache_ex("CNodeCache", sizeof(cnode_t), 16, &policy);
    }

    cnode_t* cn = (cnode_t*)slab_alloc(cnode_cache);
    if (!cn) return NULL;

    fast_zero(cn, sizeof(cnode_t));
    cn->lock = 0;
    return cn;
}

void cnode_destroy(cnode_t* cn) {
    if (!cn) return;

    for (int i = 0; i < CNODE_SLOTS; i++) {
        cap_delete(cn, (uint32_t)i);
    }

    fast_zero(cn, sizeof(cnode_t));
    slab_free(cnode_cache, cn);
}

static bool cap_identity_is_alive(cap_identity_t* ident) {
    if (!cap_identity_is_valid(ident)) return false;
    if (__atomic_load_n(&ident->revocation_epoch, __ATOMIC_ACQUIRE) != 0) return false;

    cap_identity_t* child = ident;
    cap_identity_t* parent = ident->parent;

    while (parent) {
        if (!cap_identity_is_valid(parent)) {
            __atomic_store_n(&ident->revocation_epoch, global_revocation_epoch, __ATOMIC_RELEASE);
            return false;
        }

        uint64_t parent_revocation = __atomic_load_n(&parent->revocation_epoch, __ATOMIC_ACQUIRE);
        if (parent_revocation != child->parent_epoch_start) {
            if (parent_revocation == 0) {
                parent_revocation = __atomic_fetch_add(&global_revocation_epoch, 1, __ATOMIC_RELAXED) + 1;
            }
            __atomic_store_n(&ident->revocation_epoch, parent_revocation, __ATOMIC_RELEASE);
            return false;
        }

        child = parent;
        parent = parent->parent;
    }

    return true;
}

cap_identity_t* cap_lookup(cnode_t* root, uint32_t cptr) {
    if (!root || cptr >= CNODE_SLOTS) {
        cap_metric_inc(&cap_metrics.lookup_miss);
        return NULL;
    }

    cap_identity_t* ident;
    uint64_t flags = spinlock_irqsave(&root->lock);
    ident = root->slots[cptr].ptr;
    spinlock_irqrestore(&root->lock, flags);

    if (!ident || !cap_identity_is_valid(ident)) {
        cap_metric_inc(&cap_metrics.lookup_miss);
        return NULL;
    }

    if (!cap_identity_is_alive(ident)) {
        cap_metric_inc(&cap_metrics.lookup_miss);
        return NULL;
    }

    if (!cap_allowed_modes_valid(ident->allowed_modes)) {
        cap_metric_inc(&cap_metrics.lookup_miss);
        return NULL;
    }

    if ((mode_get_current_mask() & ident->allowed_modes) == 0) {
        audit_meta_t meta = {0};
        meta.cap.cap_addr = (uint64_t)ident->object_ptr;
        meta.cap.rights = ident->rights;
        audit_strike(AUDIT_EVENT_CAP_DENIED, AUDIT_RESULT_DENIED, (uint64_t)ident->type << 32 | ident->badge, meta);
        cap_metric_inc(&cap_metrics.lookup_miss);
        return NULL;
    }

    cap_metric_inc(&cap_metrics.lookup_ok);
    return ident;
}

int cap_insert(cnode_t* root, uint32_t cptr, cap_identity_t* ident) {
    if (!root || cptr >= CNODE_SLOTS || !cap_identity_is_valid(ident)) {
        cap_metric_inc(&cap_metrics.insert_fail);
        return -1;
    }

    uint64_t flags = spinlock_irqsave(&root->lock);
    if (root->slots[cptr].ptr != NULL) {
        spinlock_irqrestore(&root->lock, flags);
        cap_metric_inc(&cap_metrics.insert_fail);
        return -1;
    }

    root->slots[cptr].ptr = ident;
    __atomic_fetch_add(&ident->ref_count, 1, __ATOMIC_RELAXED);
    spinlock_irqrestore(&root->lock, flags);

    cap_metric_inc(&cap_metrics.insert_ok);
    return 0;
}

static void cap_unlink_from_parent(cap_identity_t* ident) {
    cap_identity_t* parent = ident->parent;
    if (!parent) return;

    if (ident->prev_sibling) {
        ident->prev_sibling->next_sibling = ident->next_sibling;
    } else {
        parent->first_child = ident->next_sibling;
    }

    if (ident->next_sibling) {
        ident->next_sibling->prev_sibling = ident->prev_sibling;
    }

    ident->parent = NULL;
    ident->next_sibling = NULL;
    ident->prev_sibling = NULL;
}

void cap_identity_free(cap_identity_t* ident) {
    if (!cap_identity_is_valid(ident)) return;

    switch (ident->type) {
        case CAP_TYPE_LATTICE:
            lattice_destroy((lattice_t*)ident->object_ptr);
            break;
        case CAP_TYPE_RAM:
        case CAP_TYPE_CNODE:
        case CAP_TYPE_THREAD:
        case CAP_TYPE_ENDPOINT:
        case CAP_TYPE_PCID:
        case CAP_TYPE_PAGETABLE:
        case CAP_TYPE_AUDITOR:
        case CAP_TYPE_SCHED_AUTH_ROOT:
        case CAP_TYPE_SCHED_AUTH_THREAD:
            kfree((void*)ident->object_ptr);
            break;
        case CAP_TYPE_GENESIS:
        default:
            break;
    }

    cap_identity_t* parent = NULL;

    uint64_t flags = spinlock_irqsave(&lineage_lock);
    parent = ident->parent;
    cap_unlink_from_parent(ident);
    spinlock_irqrestore(&lineage_lock, flags);

    ident->magic = 0;
    fast_zero(ident, sizeof(cap_identity_t));
    slab_free(identity_cache, ident);
    cap_metric_inc(&cap_metrics.identities_freed);

    if (parent && cap_identity_is_valid(parent)) {
        uint32_t refs = __atomic_load_n(&parent->ref_count, __ATOMIC_ACQUIRE);
        flags = spinlock_irqsave(&lineage_lock);
        bool has_children = (parent->first_child != NULL);
        spinlock_irqrestore(&lineage_lock, flags);

        if (refs == 0 && !has_children) {
            cap_identity_free(parent);
        }
    }
}

void cap_reaper(void) {
    /* Synchronous cleanup via cap_delete/cap_identity_free remains the active model. */
}

void cap_delete(cnode_t* root, uint32_t cptr) {
    if (!root || cptr >= CNODE_SLOTS) return;

    uint64_t flags = spinlock_irqsave(&root->lock);
    cap_identity_t* ident = root->slots[cptr].ptr;
    if (!ident) {
        spinlock_irqrestore(&root->lock, flags);
        return;
    }

    root->slots[cptr].ptr = NULL;
    spinlock_irqrestore(&root->lock, flags);

    uint32_t old_ref = __atomic_fetch_sub(&ident->ref_count, 1, __ATOMIC_ACQ_REL);
    cap_metric_inc(&cap_metrics.delete_ok);

    if (old_ref == 1) {
        flags = spinlock_irqsave(&lineage_lock);
        bool has_children = (ident->first_child != NULL);
        spinlock_irqrestore(&lineage_lock, flags);

        if (!has_children) {
            cap_identity_free(ident);
        }
    }
}

int cap_copy(cnode_t* root, uint32_t src_cptr, uint32_t dst_cptr) {
    cap_identity_t* ident = cap_lookup(root, src_cptr);
    if (!ident || !(ident->rights & CAP_RIGHT_GRANT)) {
        cap_metric_inc(&cap_metrics.copy_fail);
        return -1;
    }

    if (cap_insert(root, dst_cptr, ident) != 0) {
        cap_metric_inc(&cap_metrics.copy_fail);
        return -1;
    }

    cap_metric_inc(&cap_metrics.copy_ok);
    return 0;
}

int cap_retype(cnode_t* root, uint32_t src_cptr, uint32_t dst_cptr, uint16_t new_type, uint32_t badge) {
    cap_identity_t* parent = cap_lookup(root, src_cptr);
    if (!parent || !(parent->rights & CAP_RIGHT_GRANT)) {
        cap_metric_inc(&cap_metrics.retype_fail);
        return -1;
    }

    if (parent->type != CAP_TYPE_RAM || (new_type != CAP_TYPE_PAGETABLE && new_type != CAP_TYPE_ENDPOINT)) {
        cap_metric_inc(&cap_metrics.retype_fail);
        return -1;
    }

    if (!cap_slot_is_empty(root, dst_cptr)) {
        cap_metric_inc(&cap_metrics.retype_fail);
        return -1;
    }

    /* Initialize the object if it's an endpoint */
    if (new_type == CAP_TYPE_ENDPOINT) {
        ipc_endpoint_t* ep = (ipc_endpoint_t*)pmm_phys_to_virt(parent->object_ptr);
        fast_zero(ep, sizeof(ipc_endpoint_t));
        ep->lock = 0;
    }

    cap_identity_t* child = cap_identity_create(parent->object_ptr, new_type, parent->rights, badge, parent->allowed_modes);
    if (!child) {
        cap_metric_inc(&cap_metrics.retype_fail);
        return -1;
    }

    uint64_t flags = spinlock_irqsave(&lineage_lock);
    child->parent = parent;
    child->parent_epoch_start = __atomic_load_n(&parent->revocation_epoch, __ATOMIC_ACQUIRE);
    child->next_sibling = parent->first_child;
    if (parent->first_child) {
        parent->first_child->prev_sibling = child;
    }
    parent->first_child = child;
    spinlock_irqrestore(&lineage_lock, flags);

    if (cap_insert(root, dst_cptr, child) != 0) {
        cap_identity_free(child);
        cap_metric_inc(&cap_metrics.retype_fail);
        return -1;
    }

    cap_metric_inc(&cap_metrics.retype_ok);
    return 0;
}

int cap_mint(cnode_t* root, uint32_t src_cptr, uint32_t dst_cptr, uint16_t new_rights, uint32_t badge, uint8_t allowed_modes) {
    cap_identity_t* parent = cap_lookup(root, src_cptr);
    if (!parent || !(parent->rights & CAP_RIGHT_GRANT)) {
        cap_metric_inc(&cap_metrics.mint_fail);
        return -1;
    }

    if ((new_rights & parent->rights) != new_rights) {
        audit_meta_t meta = {0};
        meta.cap.cap_addr = (uint64_t)parent->object_ptr;
        meta.cap.rights = new_rights;
        audit_strike(AUDIT_EVENT_CAP_DENIED, AUDIT_RESULT_DENIED, (uint64_t)parent->type << 32 | badge, meta);
        cap_metric_inc(&cap_metrics.policy_deny);
        cap_metric_inc(&cap_metrics.mint_fail);
        return -1;
    }

    if (!cap_allowed_modes_valid(allowed_modes)) {
        cap_metric_inc(&cap_metrics.policy_deny);
        cap_metric_inc(&cap_metrics.mint_fail);
        return -1;
    }

    uint8_t effective_modes = parent->allowed_modes & allowed_modes;
    if (effective_modes == 0 || !cap_slot_is_empty(root, dst_cptr)) {
        cap_metric_inc(&cap_metrics.policy_deny);
        cap_metric_inc(&cap_metrics.mint_fail);
        return -1;
    }

    cap_identity_t* child = cap_identity_create(parent->object_ptr, parent->type, new_rights, badge, effective_modes);
    if (!child) {
        cap_metric_inc(&cap_metrics.mint_fail);
        return -1;
    }

    uint64_t flags = spinlock_irqsave(&lineage_lock);
    child->parent = parent;
    child->parent_epoch_start = __atomic_load_n(&parent->revocation_epoch, __ATOMIC_ACQUIRE);
    child->next_sibling = parent->first_child;
    if (parent->first_child) {
        parent->first_child->prev_sibling = child;
    }
    parent->first_child = child;
    spinlock_irqrestore(&lineage_lock, flags);

    if (cap_insert(root, dst_cptr, child) != 0) {
        cap_identity_free(child);
        cap_metric_inc(&cap_metrics.mint_fail);
        return -1;
    }

    audit_meta_t success_meta = {0};
    success_meta.cap.cap_addr = (uint64_t)child->object_ptr;
    success_meta.cap.rights = child->rights;
    audit_strike(AUDIT_EVENT_CAP_MINT, AUDIT_RESULT_OK, (uint64_t)child->type << 32 | badge, success_meta);

    cap_metric_inc(&cap_metrics.mint_ok);
    return 0;
}

static void cap_revoke_tree(cap_identity_t* root, uint64_t epoch) {
    sched_auth_obj_t* auth;
    if (!root) return;

    cap_identity_t* child = root->first_child;
    while (child) {
        cap_revoke_tree(child, epoch);
        child = child->next_sibling;
    }

    __atomic_store_n(&root->revocation_epoch, epoch, __ATOMIC_RELEASE);
    if (root->type == CAP_TYPE_SCHED_AUTH_ROOT || root->type == CAP_TYPE_SCHED_AUTH_THREAD) {
        auth = (sched_auth_obj_t*)root->object_ptr;
        if (auth && auth->magic == 0x53415554u) {
            if (root->type == CAP_TYPE_SCHED_AUTH_ROOT && auth->owner_process) {
                scheduler_revoke_process_immediate(auth->owner_process);
            }
            if (root->type == CAP_TYPE_SCHED_AUTH_THREAD && auth->bound_thread) {
                scheduler_revoke_thread_immediate(auth->bound_thread);
            }
        }
    }
}

void cap_revoke(cnode_t* root, uint32_t cptr) {
    cap_identity_t* ident = cap_lookup(root, cptr);
    if (!ident) return;

    uint64_t my_epoch = __atomic_fetch_add(&global_revocation_epoch, 1, __ATOMIC_RELAXED) + 1;

    uint64_t flags = spinlock_irqsave(&lineage_lock);
    cap_revoke_tree(ident, my_epoch);
    spinlock_irqrestore(&lineage_lock, flags);

    cap_metric_inc(&cap_metrics.revoke_ops);
}
