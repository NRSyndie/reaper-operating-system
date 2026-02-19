# Reaper-OS Mode Data Structures (The Soul of the Machine)

This document defines the in-memory structures that hold the "Cosmic State" of the operating system.

## 1. The Core Enumeration (Realities)

```c
typedef enum {
    MODE_VOID     = 0, // Uninitialized / Boot State
    MODE_CASUAL   = 1, // Standard Operation
    MODE_SECURE   = 2, // Active Defense
    MODE_LOCKDOWN = 3, // System Survival (Suffocation)
    MODE_GHOST    = 4  // Offensive Operation (Ephemeral)
} mode_id_t;
```

## 2. Mode-Specific Contexts (The Union)

```c
// Context for Secure Mode (Defense)
typedef struct {
    uint64_t entered_at_tsc;      // When did we enter Secure?
    uint32_t active_threat_count; // Number of unresolved threats
    uint64_t last_threat_tsc;     // Timestamp of last incident
    bool     vpn_tunnel_active;   // Status of the forced VPN
} secure_context_t;

// Context for Lockdown Mode (Survival)
typedef struct {
    uint64_t entered_at_tsc;
    bool     network_severed;     // Hardware drivers put to sleep?
    bool     fs_readonly;         // Global Read-Only flag active?
    char     trigger_reason[128]; // Why are we here?
} lockdown_context_t;

// Context for Ghost Mode (Offensive)
typedef struct {
    uint64_t session_id;          // Random ID for this ghost session
    void*    overlay_fs_root;     // Pointer to overlay filesystem
    void*    ghost_page_table;    // Separate PML4 for Ghost isolation
    uint64_t forensic_log_start;  // Physical address of session log
} ghost_context_t;

// The Reality Context Union
typedef union {
    secure_context_t   secure;
    lockdown_context_t lockdown;
    ghost_context_t    ghost;
} mode_context_t;
```

## 3. The Fate String Structure (History)

Defined in `docs/fate_strings_design.md`. (Refined for Phase 1 with 128-byte reason and 32-byte hash fields).

```c
typedef struct {
    uint64_t timestamp_tsc;
    uint8_t  from_mode;
    uint8_t  to_mode;
    uint8_t  trigger_source;
    uint8_t  _reserved1;
    uint32_t requestor_pid;
    uint8_t  prev_hash[32];      // Stores 8-byte FNV1a in Phase 1
    uint8_t  record_hash[32];    // Stores 8-byte FNV1a in Phase 1
    uint32_t repeat_count;
    char     reason[128];
    uint8_t  signature[64];
} fate_event_t;
```

## 4. The Global State (The Source of Truth)

```c
typedef struct {
    // --- Synchronization ---
    spinlock_t transition_lock;

    // --- Identity ---
    volatile mode_id_t current_mode;
    volatile mode_id_t previous_mode;

    // --- The Active Reality ---
    mode_context_t active_context;

    // --- Immutable History (Fate Strings) ---
    fate_event_t fate_history[256];
    uint64_t     fate_head_index;
    uint64_t     fate_generation_id;

    // --- Statistics (Telemetry) ---
    struct {
        uint64_t time_in_casual_ticks;
        uint64_t time_in_secure_ticks;
        uint64_t time_in_lockdown_ticks;
        uint64_t time_in_ghost_ticks;
        
        uint64_t transitions_total;
        uint64_t transitions_auto;
        uint64_t transitions_manual;
    } stats;

} mode_state_t;
```