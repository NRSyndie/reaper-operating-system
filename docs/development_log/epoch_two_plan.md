# Reaper-OS Epoch II: The Architect's Vision (Alignment)

## 🌑 Core Philosophy: The Voidborn Reality
Epoch II focuses on transforming ReaperCore from a basic microkernel into a **Secure Substrate**. We are moving toward a decentralized **Void Workspace** where authority is hierarchical, context-aware, and communication is passive.

## 🏗️ Tier 1: Essential & Proven (Current Implementation Phase)
*Total Timeline: 4 Weeks*

### 1. Law 1: Derivation Trees (Hierarchical Authority) [DONE]
Authority is no longer a flat list of capabilities; it is a family tree.
- **Law:** Every Capability (Rune) tracks its parent. 
- **Mechanism:** If a parent Rune is deleted, the kernel recursively revokes every child Rune derived from it across all processes.
- **Goal:** Absolute security and safe resource delegation.
- **Status:** Implemented (Epoch II, Day 22)

### 2. Law 4: Conditional Runes (Mode-Aware Power) [DONE] (Day 19)
Capabilities are not static; they are sensitive to the state of the universe.
- **Law:** Runes can be "locked" to specific system Modes (Casual, Secure, etc.).
- **Mechanism:** A syscall using a Rune will fail if the current system Mode does not match the Rune's internal requirements.
- **Goal:** Context-aware security that changes as the OS shifts realities.
- **Status:** Implemented (Epoch II, Day 19)

### 3. Law 6: Shared Memory (Resonance Lattices) [DONE] (Day 26)
High-performance, zero-copy communication.
- **Law:** Processes communicate through shared physical frames rather than kernel-mediated messages.
- **Mechanism:** The kernel facilitates the creation of a "Lattice" (shared memory region) between two worlds. Information flows passively.
- **Prismatic Implementation:** Enforces unidirectional flow (Source RW -> Echo RO) and implicit MMU synchronization (The Void Wall).
- **Goal:** Eliminate the "Microkernel Tax" for high-volume data streaming.
- **Status:** Implemented (Epoch II, Day 26)

### 4. Law 7: Occult Synthesis (Bounded Authority) [SHELVED]
New powers through capability synthesis are intentionally not in active Epoch II scope.
- **Status:** Shelved to Potential Considerations for future architecture phases.
- **Reason:** Open-ended pairwise synthesis increases policy/verification complexity and is not required for Epoch II MVP viability.

---

## ⏳ Tier 2: Valuable But Deferred (Epoch III)
These features are high-value but will be implemented once the core Tier 1 mechanisms are stable.

- **Law 3: Probability Spaces:** Advanced demand paging and speculative materialization (replacing with basic demand paging in the interim).
- **Law 8: Ephemeral Realities:** Using Ghost Mode to spin up and collapse temporary "Ghost Universes" for high-risk tasks.

---

## 🏛️ The Void Workspace (Implementation Strategy)
- **Removal of Paradigm:** We will dismantle the single "God-Daemon" model. 
- **Decentralized Boot:** The kernel will spawn independent "Reality Fragments" that use Derivation Trees to manage their own sub-authority.
- **Honest Computation:** The result is a system that is difficult to misuse and fundamentally honest about resource ownership.

---

## 🌌 Pentabrid Expansions (Primitive Evolution)

These expansions evolve Epoch I foundations into the full Pentabrid architecture.

### **I. Authority & Capabilities (The Rune Loom)**
1.  **Law 1: Recursive Revocation**: Upgrade `cap_identity_t` with `parent`/`child` pointers to enable a system-wide "Snap" where revoking a root capability instantly annihilates all derived runes.
2.  **Law 7: Occult Synthesis [SHELVED]**: Reserved as a future design option only, to be reconsidered as a tightly whitelisted template mechanism rather than open-ended pairwise capability algebra.
3.  **Ephemeral Runes (Leased Authority)**: Capabilities that automatically expire after a specific number of scheduler "pulses" or a system "Phase Shift."
4.  **Mode-Locked Minting**: Restricting `SYS_CAP_MINT` so that certain high-tier capabilities can only be derived when the system is in specific Modes (e.g., Lockdown).

### **II. Memory & Isolation (Sight)**
5.  **Law 2: Shadow Mapping [DONE]**: Strict-map rollout completed (Days 27-29) with real runtime validation; user-space now uses strict map/unmap paths and strict invariants are enforced in staged compatibility mode. Revalidated again in Day 36 repeated headless boots after Fate-read refinements, then automated in Day 38 via repeatable runtime matrix checks.
6.  **Law 5: PCID Colorization**: Formally bind PCID ranges to the 5 system Realities to ensure TLB entries never leak between "Realities." [DONE] (Day 25)
7.  **Law 9: Temporal Scouring [DONE]**: Implemented PMM epoch-aware allocation scrubbing (`hyper_scrub` on epoch mismatch, `fast_zero` on same-epoch) with runtime marker/counters. Fully documented in `docs/components/memory/law9_temporal_scouring.md`.
8.  **Fragile RAM Runes**: A new capability type representing a "Guard Page" that triggers a thread-kill if accessed.

### **III. Communication (The Lattice Bridge)**
9.  **Lattice Pulse Signaling**: Lightweight notification over shared lattices using the **Invisible Context** (SSE/FPU registers) to avoid kernel rendezvous.
10. **ReadOnly Lattices [DONE]**: `SYS_LATTICE_CREATE` now supports broadcast-style creation with one source capability and up to two read-only listener capabilities minted in one call. Runtime validated in Paradigm and matrix logs (Day 40).
11. **Lattice Forensics [DONE]**: Fate Strings now record lattice attachment/detachment events via dedicated lattice record type/filter (`FATE_RECORD_LATTICE`, `FATE_READ_LATTICE`), including explicit `SYS_LATTICE_DETACH` path (Day 40).

### **IV. Forensics & Execution (Fatal Forensics)**
12. **Law 3: The Auditor's Eye [DONE - Epoch II scope]**: `CAP_TYPE_AUDITOR` + `SYS_FATE_READ` are active and runtime validated for authorized Fate ledger inspection. Explicit cross-process `SYS_AUDIT` semantics are deferred to Epoch III pending target-identity/delegation model formalization.
13. **Fault-to-String Integration**: Modifying IDT handlers to append the exact CPU state (RIP, Error Code) to a thread's Fate String before termination.
    - Incremental hardening complete (Epoch II, Day 30): Fate Strings now include rejected mode-transition attempts via `result_code`, improving abuse-trail visibility even before full fault-state capture.
    - Revalidation complete (Epoch II, Day 31): headless runtime logs repeatedly confirm chain integrity and rejected-transition evidence visibility from user-space audit reads.
    - Integration pass complete (Epoch II, Day 32): IDT now logs `#GP/#PF` into Fate Strings with vector/error/RIP metadata; `SYS_FATE_READ` supports record-type filtering for fault-only audits.
    - Context expansion complete (Epoch II, Day 33): Fate fault records now include `CR2`, `RSP`, `CS`, and `RFLAGS` for deeper forensic reconstruction.
    - Real-path validation complete (Epoch II, Day 34): synthetic fault injection removed; recoverable lattice first-touch `#PF` is captured and verified in user-space Fate audit.
14. **Ghost Execution (Shadow Contexts) [DEFERRED - Epoch III]**: Shadow-context orchestration is deferred to avoid late Epoch II risk in scheduler/context invariants; existing XSAVE/FXRSTOR support remains foundational.
15. **Annihilation Archives [DEFERRED - Epoch III]**: "Book of the Dead" archival path is deferred until explicit retention policy, memory budget, and query surface are finalized.

### **V. Scheduling & Stability (The Pulse)**
16. **Mode-Weighted Quantum [DEFERRED - Epoch III]**: Epoch II retains fixed quantum (`DEFAULT_QUANTUM`) to preserve deterministic validation baselines.
17. **Zero-Residue Context Switching [PARTIAL DONE / EPOCH III HARDENING REMAINS]**: Syscall return-path GPR/XMM scrubbing and per-thread extended-state save/restore are active; full cross-color context scrub policy is deferred.
18. **Reality-Aware Bootinfo [DEFERRED - Epoch III]**: Epoch II keeps a stable Bootinfo v1 bridge for single-fragment bootstrap; fragment-specific views are deferred.

---
## Potential Considerations (Shelved)
- **Law 7: Occult Synthesis**
  - Retained as a future consideration for Epoch III/IV+ architecture work.
  - If reintroduced, it should be limited to explicit synthesis templates with strict monotonic bounds and full Fate-string auditability.
  - Not part of Epoch II MVP closure criteria.

---
## Epoch II Closure Snapshot (Day 42)
- **Recently closed:** Day 41 syscall-boundary hardening and per-probe boundary audit visibility.
- **Closure decisions ratified (formerly open items):**
  1. **Auditor path:** Closed for Epoch II via `CAP_TYPE_AUDITOR + SYS_FATE_READ`; explicit cross-process `SYS_AUDIT` moved to Epoch III design queue.
  2. **Ghost Execution:** Deferred to Epoch III with explicit non-goal status for Epoch II MVP.
  3. **Annihilation Archives:** Deferred to Epoch III pending archival policy/query contract.
  4. **Scheduler closure calls:** Mode-weighted quantum deferred; zero-residue marked partial (current syscall/context protections active, full cross-color scrub deferred).
  5. **Reality-Aware Bootinfo:** Deferred to Epoch III; Bootinfo v1 remains stable Epoch II bridge.
- **Epoch II status:** MVP closure criteria considered complete; next phase is Epoch III planning/implementation kickoff.
- **Handoff artifact:** See `docs/development_log/epoch_three_plan.md` for the Epoch III execution plan aligned to vision, security, and performance.

---
**"From the Void, we forge the laws. In the Laws, we find the Peace."**
