# Reaper-OS Mode Subsystem API

This document describes the public interface for the **Universe Layer** (Mode Subsystem). These functions are declared in `kernel/include/mode.h`.

## 1. Lifecycle Management

### `void mode_init(void)`
Initializes the mode subsystem. 
- **Action:** Sets the system state to `MODE_VOID`, then performs the first transition to `MODE_CASUAL`.
- **Timing:** Must be called after PMM and VMM are active.
- **Idempotency:** Safe to call multiple times; subsequent calls return immediately.

## 2. State Management

### `int mode_request_transition(mode_id_t target, transition_source_t source)`
Requests a shift in the system's Reality.
- **Parameters:**
    - `target`: The `mode_id_t` to enter.
    - `source`: The `transition_source_t` (KERNEL, DAEMON, or USER).
- **Returns:** `0` on success, `-1` if the transition is illegal or redundant.
- **Enforcement:** Validates against the transition matrix (e.g., blocks `GHOST -> CASUAL` and `LOCKDOWN -> CASUAL`).

### `int mode_request_transition_ex(mode_id_t target, transition_source_t source, uint32_t auth_flags)`
Extended transition request with explicit auth evidence flags.
- **Parameters:** same as `mode_request_transition` plus `auth_flags` (`MODE_AUTH_*`).
- **Returns:** `0` on success, `-1` on reject.
- **Audit behavior:** rejected attempts persist Fate transition records with deterministic reason codes in `fault_error_code`.

## 3. State Queries

### `mode_id_t mode_get_current(void)`
Returns the current system mode.
- **Performance:** O(1), lock-free, atomic.

### `mode_id_t mode_get_previous(void)`
Returns the mode the system was in before the most recent transition.
- **Usage:** Useful for forensic analysis or rolling back temporary changes.

### `bool mode_is_secure(void)`
Helper that returns `true` if the system is in `MODE_SECURE` or `MODE_LOCKDOWN`.

### `uint64_t mode_get_security_epoch(void)`
Returns the monotonic mode-security epoch used by PMM Law 9 temporal scouring.
- **Usage:** PMM compares this epoch with `frame_metadata.epoch` to decide `hyper_scrub` vs `fast_zero` on allocation.
- **Update source:** Epoch increments on accepted mode transitions.

### `const char* mode_get_name(mode_id_t mode)`
Returns the human-readable string representation of a mode (e.g., "GHOST").

## 4. Forensics

### `int mode_get_history(struct mode_transition *buffer, size_t count)`
Copies the most recent transition records into a provided buffer.
- **Parameters:**
    - `buffer`: Array of `struct mode_transition`.
    - `count`: Maximum number of records to copy.
- **Returns:** Number of records successfully copied.
- **Integrity:** Uses the internal transition lock to ensure data consistency during the copy.
- **Ordering:** Newest records are copied first.

### `int mode_get_history_filtered(struct mode_transition *buffer, size_t count, fate_read_mode_t mode)`
Copies most-recent Fate records using a record-type filter.
- **Parameters:**
    - `buffer`: Array of `struct mode_transition`.
    - `count`: Maximum number of records to copy.
    - `mode`: `FATE_READ_ALL`, `FATE_READ_TRANSITIONS`, `FATE_READ_FAULTS`, `FATE_READ_LATTICE`, or `FATE_READ_ATTEST`.
- **Returns:** Number of copied records.
- **Ordering:** Newest-first within the selected filter.

### `void mode_log_fault_event(uint8_t vector, uint64_t error_code, uint64_t rip, uint64_t cr2, uint64_t rsp, uint64_t cs, uint64_t rflags, bool from_user)`
Appends a fault-type Fate record (used by exception paths).
- **Current integration:** Used for Page Fault (`#PF`, vector 14) and General Protection Fault (`#GP`, vector 13).
- **Metadata:** Captures `fault_vector`, `fault_error_code`, `fault_rip`, `fault_cr2`, `fault_rsp`, `fault_cs`, and `fault_rflags`.

### `void mode_log_law2_attestation(uint8_t day_id, fate_result_t result, uint16_t reason_code, uint32_t detail)`
Appends a kernel-owned Law 2 closure attestation record (`FATE_RECORD_ATTEST`).
- **Usage:** Day 28/29/30 closure evidence path.
- **Metadata:** `fault_vector=day_id`, `result_code=PASS/FAIL`, `fault_error_code=reason_code`, `fault_rip=detail`.

## 5. Convenient Macros

### `KERNEL_MODE`
Evaluates to the current `mode_id_t`. Used for inline checks in subsystems.
