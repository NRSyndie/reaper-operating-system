#ifndef REAPER_PROCESS_H
#define REAPER_PROCESS_H

#include <stdint.h>
#include "capability.h"
#include "mode.h" // Include mode.h for mode_id_t

/* Forward declaration */
struct lattice;
typedef struct lattice lattice_t;
struct thread;

typedef struct {
    uint64_t vaddr;
    uint32_t page_count;
    struct lattice* lattice;
    bool     is_source; /* Source (RW) vs Echo (RO) */
} lattice_attachment_t;

#define MAX_PROCESS_LATTICES 8

/**
 * process_t (The World)
 * A container for a Reality (address space) and Authority (C-Node).
 */
typedef struct {
    uint64_t pml4_phys;  /* Physical address of the PML4 */
    uint16_t pcid;       /* Hardware Context ID */
    cnode_t* cspace;     /* The Keyring (Capability List) */
    uint32_t pid;        /* Process ID */
    uint32_t thread_count;
    mode_id_t mode;      /* The Reality (Mode) this process belongs to */
    struct thread* waiter_thread; /* Optional waiter for SYS_WAIT */
    uint32_t exit_events; /* Count of peer exits observed by this process */
    uint64_t remaining_process_budget;
    uint64_t max_total_budget;
    uint64_t refill_period_ticks;
    uint64_t last_process_refill;
    uint32_t sched_auth_root_slot;
    bool     sched_auth_root_valid;

    lattice_attachment_t lattices[MAX_PROCESS_LATTICES];
} process_t;

/**
 * process_create: Initialize a new World.
 */
process_t* process_create(uint64_t pml4, uint16_t pcid, cnode_t* cspace, mode_id_t mode); // Added mode_id_t

/**
 * process_attach_lattice: Map a lattice into a process.
 */
int process_attach_lattice(process_t* proc, lattice_t* lattice, uint64_t vaddr, bool is_source);

/**
 * process_detach_lattice: Detach a mapped lattice from a process.
 */
int process_detach_lattice(process_t* proc, lattice_t* lattice, uint64_t vaddr);

/**
 * process_destroy: Annihilate a World.
 */
void process_destroy(process_t* process);

/**
 * process_find_by_pid: Lookup a live process by PID.
 */
process_t* process_find_by_pid(uint32_t pid);

#endif /* REAPER_PROCESS_H */
