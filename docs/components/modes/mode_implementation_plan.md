# Reaper-OS Mode Subsystem Implementation Plan

This document outlines the file organization and architectural strictures for implementing the Universe Layer (Modes).

## 1. File Organization

We will create three specific files to enforce separation of concerns and encapsulation:

| File Path | Scope | Purpose |
| :--- | :--- | :--- |
| `kernel/include/mode.h` | **Public** | The Interface. Used by PMM, VMM, and Drivers to query state. |
| `kernel/include/mode_internal.h` | **Private** | The Soul. Defines the internal state structures and locking mechanisms. Only used by `mode.c`. |
| `kernel/mode.c` | **Kernel** | The Implementation. Holds the global state and transition logic. |

---

## 2. Detailed File Contents

### A. `kernel/include/mode.h` (The Public Contract)
This header defines *what* the modes are, but not *how* they are stored.

**Contents:**
1.  **Enumerations:**
    *   `enum reaper_mode` (VOID, CASUAL, SECURE, LOCKDOWN, GHOST).
    *   `enum transition_source` (KERNEL, DAEMON, USER).
2.  **Structures (Public):**
    *   `struct mode_transition` (The Fate String Record).
        *   *Note:* This corresponds to `fate_event_t` from design docs. Exposed so user-space can eventually read the audit log.
3.  **Function Declarations:**
    *   `void mode_init(void);` - Bootstrap the subsystem.
    *   `int mode_request_transition(enum reaper_mode target_mode, enum transition_source source, const char* reason);` - The main entry point.
    *   `enum reaper_mode mode_get_current(void);` - Fast read access.
    *   `const char* mode_name(enum reaper_mode mode);` - String conversion.
    *   `bool mode_is_secure(void);` - Helper (Returns true if SECURE or LOCKDOWN).

### B. `kernel/include/mode_internal.h` (The Private Soul)
This header defines the internal machinery. It should **never** be included by other kernel subsystems to prevent accidental state corruption.

**Contents:**
1.  **Includes:**
    *   `#include "mode.h"`
    *   `#include <utils.h>` (for types, spinlocks)
2.  **Internal Structures:**
    *   `union mode_context` (The variant data: `secure_context_t`, `ghost_context_t`, etc.).
    *   `struct mode_state` (The Global Singleton).
        *   Current/Previous mode.
        *   `fate_history` (Ring buffer of `struct mode_transition`).
        *   `spinlock_t lock`.
        *   Statistics.
3.  **Internal Helpers:**
    *   `uint64_t internal_hash_fnv1a(const void* data, size_t len);`
    *   `void internal_log_fate_event(enum reaper_mode from, enum reaper_mode to, ...);`

### C. `kernel/mode.c` (The Implementation)
The actual code logic.

**Contents:**
1.  **Global Variable:**
    *   `static struct mode_state g_state;` (Restricted visibility).
2.  **Initialization:**
    *   `mode_init()`: Zeroes memory, inits spinlock, sets `CASUAL` (after boot).
3.  **Transition Logic (`mode_request_transition`):**
    *   **Locking:** Acquire `g_state.lock`.
    *   **Validation:** Check against the **Transition Matrix** (Illegal transitions return error).
    *   **Execution:**
        *   Update `previous_mode`.
        *   Update `current_mode`.
        *   Clear/Init `active_context`.
    *   **Logging:** Generate `struct mode_transition` event, calculate hashes, append to Ring Buffer.
    *   **Unlocking:** Release lock.

---

## 3. Integration Strategy

### Encapsulation Rules
1.  **PMM/VMM Access:**
    *   Subsystems needing to know the current mode (e.g., PMM allocating a Ghost page) must call `mode_get_current()`.
    *   They must **not** access `g_state` directly.
2.  **Transition Authority:**
    *   Currently, any kernel code can call `mode_request_transition`.
    *   **Future:** This function will restrict `source == DAEMON` to a specific system call handler, ensuring only the Paradigm Daemon can drive the state machine (except for `KERNEL_AUTO` panic triggers).

### Build System
*   Add `kernel/mode.o` to the `OBJS` list in `kernel/Makefile`.
