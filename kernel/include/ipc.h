#ifndef REAPER_IPC_H
#define REAPER_IPC_H

#include "thread.h"

#define IPC_BUFFER_SIZE 16

/**
 * ipc_message_t
 * A buffered message in an endpoint.
 */
typedef struct {
    uint64_t data[4];
} ipc_message_t;

/**
 * ipc_endpoint_t
 * A rendezvous point or buffered queue for IPC.
 */
typedef struct {
    thread_t* send_head; /* Queue of senders waiting to send (buffer full) */
    thread_t* send_tail;
    thread_t* recv_head; /* Queue of receivers waiting for a message (buffer empty) */
    thread_t* recv_tail;

    /* Buffered IPC */
    ipc_message_t buffer[IPC_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;

    spinlock_t lock;
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
