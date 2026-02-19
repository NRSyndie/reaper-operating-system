#ifndef REAPER_MODE_INTERNAL_H
#define REAPER_MODE_INTERNAL_H

#include <mode.h>
#include <utils.h> // Assuming this contains spinlock_t or equivalent

/*
 * Mode-Specific Contexts
 * Variant data stored in a union to save memory and enforce isolation.
 */

typedef struct {
    uint64_t entered_at_tsc;
    uint32_t active_threat_count;
    uint64_t last_threat_tsc;
    bool     vpn_tunnel_active;
} secure_context_t;

typedef struct {
    uint64_t entered_at_tsc;
    bool     network_severed;
    bool     fs_readonly;
    char     trigger_reason[128];
} lockdown_context_t;

typedef struct {
    uint64_t session_id;
    void*    overlay_fs_root;
    void*    ghost_page_table;
    uint64_t forensic_log_start;
} ghost_context_t;

typedef union {
    secure_context_t   secure;
    lockdown_context_t lockdown;
    ghost_context_t    ghost;
} mode_context_t;

/*
 * Global System State (The Soul)
 * Restricted to mode.c via this internal header.
 */
struct mode_state {
    // Synchronization
    uint32_t transition_lock; // Simple spinlock variable for now

    // Identity
    volatile mode_id_t current_mode;
    volatile mode_id_t previous_mode;
    volatile uint8_t   global_mode_mask; // Bitmask: (1 << current_mode)

    // Active Reality Context
    mode_context_t active_context;

    // Fate String Ledger (History)
    struct mode_transition fate_history[MODE_HISTORY_SIZE];
    uint64_t               fate_head_index;
    uint64_t               fate_generation_id;

    // Statistics
    struct {
        uint64_t time_in_casual_ticks;
        uint64_t time_in_secure_ticks;
        uint64_t time_in_lockdown_ticks;
        uint64_t time_in_ghost_ticks;
        
        uint64_t transitions_total;
        uint64_t transitions_auto;
        uint64_t transitions_manual;
    } stats;

    // Monotonic security epoch (Law 9 temporal scouring anchor)
    volatile uint64_t security_epoch;
};

#endif /* REAPER_MODE_INTERNAL_H */
