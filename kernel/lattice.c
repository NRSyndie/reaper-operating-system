#include "include/ipc.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/slab.h"
#include "include/capability.h"
#include "include/klog.h"
#include "include/utils.h"
#include "include/scheduler.h"
#include "include/process.h"
#include "include/kmalloc.h"

static slab_cache_t* lattice_cache = NULL;

/**
 * lattice_create: Allocate physical frames and create a lattice object.
 */
lattice_t* lattice_create(uint32_t page_count, uint64_t owner_token) {
    if (page_count == 0 || page_count > 1024) return NULL; // Sanity limit: 4MB

    if (!lattice_cache) {
        lattice_cache = slab_create_cache("LatticeCache", sizeof(lattice_t), 8);
    }

    lattice_t* lattice = (lattice_t*)slab_alloc(lattice_cache);
    if (!lattice) return NULL;

    lattice->page_count = page_count;
    lattice->ref_count = 1;
    lattice->lock = 0;
    lattice->crystal_index = 0;
    lattice->is_ocular = false;
    lattice->ocular_target = 0;
    lattice->wait_head = NULL;
    lattice->wait_tail = NULL;
    
    // Allocate space for the frames array (up to 1024 uint64_t entries = 8KB)
    lattice->frames = (uint64_t*)kzalloc(page_count * sizeof(uint64_t));
    if (!lattice->frames) {
        slab_free(lattice_cache, lattice);
        return NULL;
    }

    for (uint32_t i = 0; i < page_count; i++) {
        lattice->frames[i] = pmm_alloc(COLOR_CASUAL, owner_token);
        if (lattice->frames[i] == 0) {
            // Rollback
            for (uint32_t j = 0; j < i; j++) {
                pmm_free(lattice->frames[j]);
            }
            kfree(lattice->frames);
            slab_free(lattice_cache, lattice);
            return NULL;
        }
    }

    return lattice;
}

/**
 * lattice_destroy: Free frames and the lattice object.
 */
void lattice_destroy(lattice_t* lattice) {
    if (!lattice) return;

    uint32_t new_ref = __atomic_fetch_sub(&lattice->ref_count, 1, __ATOMIC_ACQ_REL);
    if (new_ref == 1) {
        for (uint32_t i = 0; i < lattice->page_count; i++) {
            pmm_free(lattice->frames[i]);
        }
        kfree(lattice->frames);
        slab_free(lattice_cache, lattice);
    }
}

/**
 * lattice_ref: Increment reference count.
 */
void lattice_ref(lattice_t* lattice) {
    if (!lattice) return;
    __atomic_fetch_add(&lattice->ref_count, 1, __ATOMIC_RELAXED);
}

int lattice_attune(lattice_t* lattice, uint32_t new_crystal_index) {
    if (!lattice) return -1;
    if (new_crystal_index >= lattice->page_count) return -1;

    uint64_t flags = spinlock_irqsave(&lattice->lock);

    // 1. Advance the Crystal Index
    lattice->crystal_index = new_crystal_index;

    // 2. Resolve the Void Wall (Wake blocked readers)
    thread_t* curr = lattice->wait_head;
    while (curr) {
        thread_t* next = curr->wait_next;
        
        // We wake everyone and let them re-fault if needed.
        scheduler_wake(curr);
        
        curr = next;
    }
    lattice->wait_head = NULL;
    lattice->wait_tail = NULL;

    spinlock_irqrestore(&lattice->lock, flags);
    return 0;
}

bool lattice_handle_fault(uint64_t vaddr, uint64_t error_code) {
    thread_t* curr_thread = scheduler_get_current();
    if (!curr_thread || !curr_thread->owner) return false;

    process_t* proc = curr_thread->owner;
    lattice_attachment_t* attach = NULL;

    // 1. Find the lattice attachment for this vaddr
    for (int i = 0; i < MAX_PROCESS_LATTICES; i++) {
        if (proc->lattices[i].lattice && 
            vaddr >= proc->lattices[i].vaddr && 
            vaddr < proc->lattices[i].vaddr + (proc->lattices[i].page_count * PAGE_SIZE)) {
            attach = &proc->lattices[i];
            break;
        }
    }

    if (!attach) return false;

    lattice_t* lattice = attach->lattice;
    uint32_t page_idx = (vaddr - attach->vaddr) / PAGE_SIZE;

    // 2. Physics Check: Why did we fault?
    if (attach->is_source) {
        // Source should only fault if mapping is missing entirely.
        // Sources are always RW.
        vmm_map(proc, vaddr & ~0xFFFULL, lattice->frames[page_idx], PAGE_USER_DATA);
        return true;
    } else {
        // Echo (Consumer)
        if (error_code & 2) { // Write attempt
            klog_critical("[PRISM] Security Violation: Echo attempted to write to Source at 0x%lx.", vaddr);
            return false; // Triggers thread exit
        }

        // The Void Wall Check: Is this page crystallized?
        if (page_idx > lattice->crystal_index) {
            // Future Page Access! Block the soul.
            uint64_t flags = spinlock_irqsave(&lattice->lock);
            
            scheduler_block(curr_thread);
            curr_thread->wait_next = NULL;
            if (!lattice->wait_head) {
                lattice->wait_head = curr_thread;
                lattice->wait_tail = curr_thread;
            } else {
                lattice->wait_tail->wait_next = curr_thread;
                lattice->wait_tail = curr_thread;
            }
            
            spinlock_irqrestore(&lattice->lock, flags);
            
            klog_debug("[VOID-WALL] Soul TID:%d blocked on future frame %d.", curr_thread->tid, page_idx);
            schedule(); // Switch away
            return true;
        }

        // Frame is ready! Materialize it RO.
        vmm_map(proc, vaddr & ~0xFFFULL, lattice->frames[page_idx], PAGE_USER_CODE); // USER_CODE is RO
        return true;
    }
}
