# Reaper-OS Layer 3: Fate Strings (Design & Implementation)

Fate Strings are the "Immutable History" of the operating system. They record Mode Transitions (`Casual` ↔ `Lockdown`) and critical security events in a cryptographically bound chain. This ensures that history cannot be rewritten by an attacker to hide their tracks.

## 1. The Data Structure (The "Atom" of History)

Every event is a link in a cryptographic chain. The structure is fixed at 128 bytes to ensure predictable ring buffer math and stable binary layout.

```c
typedef struct {
    uint64_t timestamp;        // Kernel Tick (Global Chronos)
    uint8_t prev_hash[32];     // Chain Link (Hash of previous record)
    uint64_t actor_id;         // [Mode:16][TID:48]
    uint64_t target_id;        // Dependent on event (PID, ObjPtr, etc.)
    uint16_t event_type;       // AUDIT_EVENT_T
    uint16_t result_code;      // AUDIT_RESULT_T
    uint32_t gap_seq;          // Number of dropped records since previous entry
    audit_meta_t metadata;     // 32-byte union (Faults, Sched, Caps)
    uint8_t hash[32];          // Integrity Hash of this record
} __attribute__((packed)) audit_record_t;
```
**Total Size:** 128 bytes (verified via `static_assert`).

---

## 2. Implementation Architecture

### The Lattice (Ring Buffer)
The system employs a high-performance ring buffer (`audit_lattice_t`) with a capacity of 1024 slots.
- **Concurrency**: SMP-safe via `stdatomic.h`. Head/Tail pointers use Acquire/Release semantics.
- **Reservation Policy**: A threshold of `LATTICE_CAPACITY - 2` is enforced. 
    - Normal records are dropped when the threshold is reached to protect buffer integrity.
    - Two slots are reserved for **Emergency Overflows** and **Phase Shifts**.

### Cryptographic Chaining
History is bound by a rolling hash chain.
- **Hash**: The kernel uses **BLAKE3** for both record chaining and seed derivation.
- **Reality-Bound Seeding**: The chain is re-keyed on every Phase Shift.
    - `Current_FateSeed = BLAKE3(Root_System_Seed | Reality_ID | Epoch)`
    - Record chaining uses `BLAKE3(prev_hash | record_content | FateSeed)`.
    - This ensures that an attacker cannot predict future hashes even if they compromise a single Reality's history.

---

## 3. Instrumentation Matrix

| Event | Metadata | Trigger Point |
| :--- | :--- | :--- |
| `THREAD_CREATE` | TID, Auth Mode | `thread_create` |
| `THREAD_DESTROY`| TID | `thread_destroy` |
| `PHASE_SHIFT` | Source, Auth Flags | `mode_request_transition_ex` |
| `CAP_DENIED` | ObjPtr, Rights | `cap_lookup`, `cap_mint` |
| `CAP_MINT` | ObjPtr, Rights | `cap_mint` (Success) |
| `SCHED_STALL` | TID, Stall Ticks | `scheduler.c` (Starvation check) |
| `OVERFLOW` | Buffer Distance | `audit_strike` (Threshold breach) |

---

## 4. Integrity & Verification

### The "Gap" Policy
If the system is flooded beyond its processing capacity, it prioritizes **Integrity over Completeness**:
1.  Records are dropped.
2.  An `AUDIT_EVENT_OVERFLOW` is struck using a reserved slot.
3.  The `gap_seq` of the next successful record records the count of lost history.
4.  The cryptographic chain remains unbroken; the gap is a documented part of the history.

### Security Milestones
- **MILESTONE_GHOST_HARDENING (Seed)**: Transition from `RDRAND` with weak `TSC` fallback to sealed-storage-backed or equivalent durable CSPRNG seeding for the `Root_System_Seed`.
- **Hash milestone closed**: The previous placeholder mixer has already been replaced with the vendored **BLAKE3** implementation in the kernel.
