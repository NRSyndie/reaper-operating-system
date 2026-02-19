#ifndef SHARED_MODE_H
#define SHARED_MODE_H

#include <stdint.h>

/* 
 * Reaper-OS Realities (Modes)
 */
typedef enum {
    MODE_VOID     = 0,
    MODE_CASUAL   = 1,
    MODE_SECURE   = 2,
    MODE_LOCKDOWN = 3,
    MODE_GHOST    = 4,
    MODE_KERNEL   = 5
} mode_id_t;

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

/*
 * Fate String Record (Mode Transition/Fault)
 * Compact 80-byte structure.
 */
struct mode_transition {
    uint64_t timestamp_tsc;      // 8 bytes
    
    uint64_t prev_hash;          // 8 bytes
    uint64_t curr_hash;          // 8 bytes
    
    uint32_t requestor_pid;      // 4 bytes
    
    uint8_t  from_mode;          // 1 byte (mode_id_t)
    uint8_t  to_mode;            // 1 byte (mode_id_t)
    uint8_t  trigger_source;     // 1 byte (transition_source_t)
    uint8_t  result_code;        // 1 byte (fate_result_t)

    uint8_t  record_type;        // 1 byte (fate_record_type_t)
    uint8_t  fault_vector;       // 1 byte (x86 fault vector OR fate_lattice_action_t for lattice records)
    uint16_t _reserved0;         // 2 bytes
    uint32_t fault_error_code;   // 4 bytes (low 32 bits)
    uint64_t fault_rip;          // 8 bytes (faulting RIP)
    uint64_t fault_cr2;          // 8 bytes (page-fault address, 0 if not applicable)
    uint64_t fault_rsp;          // 8 bytes (stack pointer at fault)
    uint64_t fault_cs;           // 8 bytes (code segment at fault)
    uint64_t fault_rflags;       // 8 bytes (rflags at fault)
} __attribute__((packed));

#endif
