#ifndef REAPER_IPC_H
#define REAPER_IPC_H

#include "thread.h"

/**
 * ipc_endpoint_t
 * A rendezvous point for Synchronous IPC.
 */
typedef struct {
    thread_t* wait_head; /* Queue of souls waiting for a rendezvous */
    thread_t* wait_tail;
} ipc_endpoint_t;

/**
 * lattice_t (The Resonance Crystal)
 * A prismatic, high-volume shared memory bridge between Worlds.
 * Enforces Unidirectional Flow (Source RW -> Echo RO).
 */
typedef struct lattice {
    uint64_t* frames;    /* Array of physical frame addresses */
    uint32_t  page_count;
    uint32_t  ref_count;

    /* The Prismatic State (Crystal Physics) */
    uint32_t  crystal_index;  /* Index of the last 'Crystallized' (Ready) frame */
    bool      is_ocular;      /* If true, bypasses scheduler and updates Ocular Engine directly */
    uint64_t  ocular_target;  /* Physical address of the framebuffer scan-out atom */

    /* The Void Wall (MMU Synchronization) */
    spinlock_t lock;
    thread_t*  wait_head;     /* Echo threads blocked on non-present pages */
    thread_t*  wait_tail;
} lattice_t;

/**
 * @brief Create a Resonance Lattice.
 */
lattice_t* lattice_create(uint32_t page_count, uint64_t owner_token);

/**
 * @brief Destroy a Resonance Lattice and free its frames.
 */
void lattice_destroy(lattice_t* lattice);

/**
 * @brief Increment reference count of a Lattice.
 */
void lattice_ref(lattice_t* lattice);

/**
 * @brief Prismatic Attunement (The Pulse).
 * Advances the crystal index and wakes up readers waiting on the newly present frames.
 */
int lattice_attune(lattice_t* lattice, uint32_t new_crystal_index);

/**
 * @brief Handle a page fault on a Lattice page (The Void Wall).
 * Returns true if the fault was handled (thread blocked or page made present).
 */
bool lattice_handle_fault(uint64_t vaddr, uint64_t error_code);

#endif /* REAPER_IPC_H */
