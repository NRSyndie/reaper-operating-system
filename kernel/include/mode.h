#ifndef REAPER_MODE_H
#define REAPER_MODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pmm.h" // Include pmm.h for color_t

/* 
 * Reaper-OS Realities (Modes)
 * Core architectural states of the Universe Layer.
 */
typedef enum {
    MODE_VOID     = 0, // Uninitialized / Booting
    MODE_CASUAL   = 1, // Standard User Operation
    MODE_SECURE   = 2, // Active Defense / Hardened
    MODE_LOCKDOWN = 3, // System Survival / Breach Response
    MODE_GHOST    = 4, // Offensive OpSec / Ephemeral
    MODE_KERNEL   = 5  // For kernel-internal operations (new)
} mode_id_t;

#define MODE_COUNT 6 // Updated from 5 to 6

typedef enum {
    TRANSITION_SOURCE_KERNEL = 0,
    TRANSITION_SOURCE_DAEMON = 1,
    TRANSITION_SOURCE_USER   = 2
} transition_source_t;

typedef enum {
    FATE_RESULT_ACCEPTED = 0,
    FATE_RESULT_REJECTED = 1
} fate_result_t;

typedef enum {
    FATE_RECORD_TRANSITION = 0,
    FATE_RECORD_FAULT      = 1,
    FATE_RECORD_LATTICE    = 2
} fate_record_type_t;

typedef enum {
    FATE_READ_ALL         = 0,
    FATE_READ_TRANSITIONS = 1,
    FATE_READ_FAULTS      = 2,
    FATE_READ_LATTICE     = 3
} fate_read_mode_t;

typedef enum {
    FATE_LATTICE_ATTACH = 1,
    FATE_LATTICE_DETACH = 2
} fate_lattice_action_t;

#define MODE_NAME_MAX    16
#define MODE_HISTORY_SIZE 256

/*
 * Fate String Record (Mode Transition)
 * This is the building block of the unforgeable audit trail.
 * Compact 80-byte structure for transition + fault metadata.
 */
struct mode_transition {
    uint64_t timestamp_tsc;      // 8 bytes
    
    /* Integrity Chain (64-bit FNV-1a) */
    uint64_t prev_hash;          // 8 bytes
    uint64_t curr_hash;          // 8 bytes
    
    uint32_t requestor_pid;      // 4 bytes
    
    uint8_t  from_mode;          // 1 byte
    uint8_t  to_mode;            // 1 byte
    uint8_t  trigger_source;     // 1 byte
    uint8_t  result_code;        // 1 byte (fate_result_t)

    uint8_t  record_type;        // 1 byte (fate_record_type_t)
    uint8_t  fault_vector;       // 1 byte (x86 fault vector OR fate_lattice_action_t for lattice records)
    uint16_t _reserved0;         // 2 bytes
    uint32_t fault_error_code;   // 4 bytes
    uint64_t fault_rip;          // 8 bytes
    uint64_t fault_cr2;          // 8 bytes (CR2 for #PF, else 0)
    uint64_t fault_rsp;          // 8 bytes
    uint64_t fault_cs;           // 8 bytes
    uint64_t fault_rflags;       // 8 bytes
} __attribute__((packed));

/* Static assertion to ensure the record layout remains stable */
_Static_assert(sizeof(struct mode_transition) == 80, "Fate String record size mismatch");

/* Public API */
void        mode_init(void);
int         mode_request_transition(mode_id_t target, transition_source_t source);
mode_id_t   mode_get_current(void);
uint8_t     mode_get_current_mask(void);
mode_id_t   mode_get_previous(void);
int         mode_get_history(struct mode_transition *buffer, size_t count);
int         mode_get_history_filtered(struct mode_transition *buffer, size_t count, fate_read_mode_t mode);
const char* mode_get_name(mode_id_t mode);
bool        mode_is_secure(void);
uint64_t    mode_get_security_epoch(void);
void        mode_log_fault_event(uint8_t vector, uint64_t error_code, uint64_t rip,
                                 uint64_t cr2, uint64_t rsp, uint64_t cs, uint64_t rflags,
                                 bool from_user);
void        mode_log_lattice_event(uint32_t pid, uint64_t vaddr, uint32_t page_count,
                                   bool is_source, bool is_attach);

// PCID-Specialization for secure transitions/wipes
void        mode_enter_secure_context(void);
void        mode_exit_secure_context(void);

// New: Map mode to allocation color
security_color_t mode_to_color(mode_id_t mode);

#define KERNEL_MODE mode_get_current()

#endif /* REAPER_MODE_H */
