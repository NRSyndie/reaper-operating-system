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
    MODE_REJECT_NONE                     = 0,
    MODE_REJECT_TARGET_INVALID           = 1,
    MODE_REJECT_SOURCE_INVALID           = 2,
    MODE_REJECT_EDGE_ILLEGAL             = 3,
    MODE_REJECT_AUTH_REQUIRED            = 4,
    MODE_REJECT_SPECIAL_KEY_REQUIRED     = 5,
    MODE_REJECT_SYSTEM_PROMPT_REQUIRED   = 6,
    MODE_REJECT_COOLDOWN_ACTIVE          = 7,
    MODE_REJECT_DEESC_WINDOW_ACTIVE      = 8,
    MODE_REJECT_BOOT_POLICY              = 9,
    MODE_REJECT_AUTH_RETRY_LIMIT         = 10
} mode_reject_reason_t;

#define MODE_AUTH_PASSWORD            (1u << 0)
#define MODE_AUTH_SPECIAL_KEY         (1u << 1)
#define MODE_AUTH_SYSTEM_PROMPT       (1u << 2)
#define MODE_AUTH_COOLDOWN_ELAPSED    (1u << 3)
#define MODE_AUTH_DEESC_ELAPSED       (1u << 4)
#define MODE_AUTH_AUTOMATIC           (1u << 5)
#define MODE_AUTH_BOOT_INTENT         (1u << 6)
#define MODE_AUTH_MANUAL              (1u << 7)

typedef enum {
    ENV_CLASS_NONE = 0,
    ENV_CLASS_BASELINE = 1,
    ENV_CLASS_DEFENSIVE = 2,
    ENV_CLASS_FAIL_CLOSED = 3,
    ENV_CLASS_EPHEMERAL = 4
} envelope_class_t;

typedef enum {
    ENV_DENY_NONE = 0,
    ENV_DENY_TARGET_INVALID = 1,
    ENV_DENY_SOURCE_INVALID = 2,
    ENV_DENY_EDGE_ILLEGAL = 3
} envelope_deny_t;

typedef enum {
    FATE_RESULT_ACCEPTED = 0,
    FATE_RESULT_REJECTED = 1
} fate_result_t;

typedef enum {
    FATE_RECORD_TRANSITION = 0,
    FATE_RECORD_FAULT      = 1,
    FATE_RECORD_LATTICE    = 2,
    FATE_RECORD_ATTEST     = 3
} fate_record_type_t;

typedef enum {
    FATE_READ_ALL         = 0,
    FATE_READ_TRANSITIONS = 1,
    FATE_READ_FAULTS      = 2,
    FATE_READ_LATTICE     = 3,
    FATE_READ_ATTEST      = 4
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
int         mode_request_transition_ex(mode_id_t target, transition_source_t source, uint32_t auth_flags);
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
void        mode_log_sched_event(uint32_t pid, uint8_t event_code, uint32_t detail, fate_result_t result);
void        mode_log_law2_attestation(uint8_t day_id, fate_result_t result, uint16_t reason_code, uint32_t detail);

// PCID-Specialization for secure transitions/wipes
void        mode_enter_secure_context(void);
void        mode_exit_secure_context(void);

// New: Map mode to allocation color
security_color_t mode_to_color(mode_id_t mode);

#define KERNEL_MODE mode_get_current()

#endif /* REAPER_MODE_H */
