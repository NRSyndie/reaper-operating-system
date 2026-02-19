#ifndef REAPER_CAPABILITY_H
#define REAPER_CAPABILITY_H

#include <stdint.h>

/*
 * REAPER-OS CAPABILITY SYSTEM (THE RUNE & THE WEB)
 * Shared Definitions
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
    CAP_MAX_TYPES
} cap_type_t;

/* Rights Bitmask */
#define CAP_RIGHT_READ    (1 << 0)
#define CAP_RIGHT_WRITE   (1 << 1)
#define CAP_RIGHT_GRANT   (1 << 2) /* Ability to copy/mint the cap */
#define CAP_RIGHT_INVOKE  (1 << 3) /* Ability to 'call' the object */
#define CAP_RIGHT_EXECUTE (1 << 4) /* Ability to execute memory */

/* Mode Constraint Bitmask (Conditional Runes) */
#define CAP_MODE_ALL      0xFF
#define CAP_MODE_VOID     (1 << 0)
#define CAP_MODE_CASUAL   (1 << 1)
#define CAP_MODE_SECURE   (1 << 2)
#define CAP_MODE_LOCKDOWN (1 << 3)
#define CAP_MODE_GHOST    (1 << 4)

#endif /* REAPER_CAPABILITY_H */
