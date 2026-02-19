# Reaper-OS Layer 3: Fate Strings (Design & Implementation Roadmap)

Fate Strings are the "Immutable History" of the operating system. They record Mode Transitions (`Casual` ↔ `Lockdown`) and critical security events in a cryptographically bound chain. This ensures that history cannot be rewritten by an attacker to hide their tracks.

## 1. The Data Structure (The "Atom" of History)

Every event is a link in a cryptographic chain. The structure is fixed-size to prevent buffer overflows and ensure O(1) storage performance.

```c
typedef struct {
    // Temporal Context
    uint64_t timestamp_tsc;      // CPU Time Stamp Counter (High precision, monotonic)
    
    // State Context
    uint8_t  from_mode;          // e.g., CASUAL (1)
    uint8_t  to_mode;            // e.g., LOCKDOWN (3)
    uint8_t  trigger_source;     // KERNEL_AUTO(0), DAEMON_REQ(1), USER_MANUAL(2)
    uint8_t  _reserved1;         // Alignment padding
    uint32_t requestor_pid;      // PID of the actor (0 if kernel-initiated)

    // Integrity Context
    // We reserve 32 bytes (256 bits) for future-proof SHA-256 support.
    // In Phase 1, we store a 64-bit FNV-1a hash in the first 8 bytes.
    uint8_t  prev_hash[32];      // Hash of the PREVIOUS event
    uint8_t  record_hash[32];    // Hash of THIS entire record
    
    // Anti-Flood Metadata
    uint32_t repeat_count;       // For coalescing identical rapid events

    // Human Context
    char     reason[128];        // "Rootkit Detected", "Unauthorized privilege escalation"
    
    // Future Proofing (Reserved)
    uint8_t  signature[64];      // Reserved for future TPM signatures (Phase 3)
} fate_event_t;
```
**Total Size:** 288 bytes (aligned for cache efficiency).

---

## 2. Implementation Roadmap

We will build this system in **Three Phases**, prioritizing foundational stability first, then resilience, and finally cryptographic perfection.

### Phase 1: The "Ring of Truth" (Current Epoch)
**Goal:** In-memory circular buffer with basic chaining.
**Why:** We need immediate visibility into state transitions during development *now*.

*   **Hashing Algorithm:** **FNV-1a (64-bit)**. 
    *   *Rationale:* Extremely fast (~5ns), easy to implement in-kernel, sufficient for detecting accidental corruption or simple tampering.
    *   *Storage:* The 64-bit result is stored in the first 8 bytes of the 32-byte hash fields.
*   **Mechanism:**
    *   `static fate_event_t fate_ledger[256];`
    *   **Overflow Policy:** Overwrite oldest (Ring Buffer).
*   **Deliverable:** Basic `fate_log()` function in Kernel.

### Phase 2: The "Flight Recorder" (Next Epoch - Reliability)
**Goal:** Resilience against Crashes and Flooding.
**Why:** To analyze *why* the system crashed or entered Lockdown.

*   **Change 1: Reserved RAM Region (PStore logic)**
    *   Allocate the ledger at a fixed, high physical address to survive soft reboots.
*   **Change 2: Event Coalescing (Anti-Flood)**
    *   If rapid identical events occur, increment `repeat_count` instead of writing new entries.

### Phase 3: The "Immutable Archive" (Future Epoch - Production)
**Goal:** Permanent, signed, unforgeable audit trail.

*   **Change 1: The "Genesis Block"**
    *   Entry #0 binds to the **Hash of the Kernel Binary**.
*   **Change 2: Upgrade to SHA-256**
    *   Switch hashing algorithm from FNV-1a to SHA-256 (utilizing the full 32-byte fields).
*   **Change 3: TPM Signing**
    *   Asynchronously sign batches of events using hardware TPM.

## 3. Integrity & Verification

### The Chain Rule
The integrity of the log is validated by walking the chain:
1.  Calculate `H' = FNV1a(Event[0])`.
2.  Read `Event[1]`. Verify `Event[1].prev_hash[0..7] == H'`.
3.  ... Repeat.

If *any* bit is flipped (corruption or tampering), the chain breaks.

### The "Panic" Policy
If the Kernel detects the Chain is broken during a write:
*   **Action:** Immediate **LOCKDOWN**.
*   **Philosophy:** A system that cannot remember its history cannot be trusted.