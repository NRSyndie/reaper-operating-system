==============================
Reaper-OS: Development Roadmap
==============================

.. table:: Epoch I: The Primordial Void (Core Kernel Foundation)
   :widths: 15 25 30 30

   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day/Step      | Goal                             | Weaknesses & Threats                           | Rationale                                                                                                             |
   +===============+==================================+================================================+=======================================================================================================================+
   | Day 1 [DONE]  | Physical Memory Audit            | Reliance on bootloader accuracy; potential     | Establishes the "Source of Truth" for the                                                                             |
   |               |                                  | overlap with reserved hardware MMIO regions.   | physical world. Prerequisite for all allocation.                                                                      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 2 [DONE]  | Physical Memory Manager (PMM)    | Memory overhead of the Bitmap Ledger; search   | Simple, deterministic tracking of frame ownership.                                                                     |
   |               |                                  | latency increases as memory fragments.         | Essential for the "Iron Gate" security model.                                                                         |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 3 [DONE]  | Virtual Memory Manager (VMM)     | Risk of "Triple Faults" during mapping;        | Enables Higher-Half isolation, keeping the lower                                                                      |
   |               |                                  | complexity of recursive page table updates.    | half open for different "Realities."                                                                                  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 4 [DONE]  | PCID & INVPCID Integration       | Hardware dependency (legacy CPUs); risk of     | Eliminates the "Microkernel Tax" by allowing                                                                          |
   |               |                                  | ID exhaustion or recycling bugs.               | reality shifts without flushing CPU caches (TLB).                                                                     |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 4 [DONE]  | Universe Layer Foundation        | Integration complexity; risk of circular       | Implements the "Cosmic State" (Modes) and "Fate                                                                       |
   |               | (Modes & Fate Strings)           | dependencies with PMM/VMM.                     | Strings" early to ensure future subsystems are                                                                        |
   |               |                                  |                                                | "Mode-Aware" from birth.                                                                                              |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 5 [DONE]  | Multi-Mode Universe Logic        | Complexity of privilege escalation rules;      | Finalizes the "Fate Strings" integrity chaining and                                                                   |
   |               |                                  | risk of state machine deadlocks.               | 5-tier privilege escalation rules.                                                                                    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 6 [DONE]  | The Soul Forge (Slab)            | Internal fragmentation in slabs; risk of       | Provides O(1) allocation for internal objects,                                                                        |
   |               |                                  | memory exhaustion if objects are leaked.       | ensuring the "Void" remains low-latency.                                                                              |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 7 [DONE]  | Capability List (C-List)         | Fixed-size arrays limit Rune count; dynamic    | Transforms security checks into a fast array                                                                          |
   |               |                                  | resizing adds O(n) latency spikes.             | index (O(1)), removing the "Iron Gate" overhead.                                                                      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 8 [DONE]  | Interrupts & Exceptions          | Risk of "Triple Faults"; complex stack         | Establishes the IDT and Page Fault handlers.                                                                          |
   |               | (IDT & Trap Gates)               | switching between Realities (Phase Shifts).    | Essential for VMM stability and system safety.                                                                        |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 9 [DONE]  | Syscall & IPC Infrastructure     | Registry vulnerabilities; register-based       | Implements the SYSCALL/SYSRET path and the                                                                            |
   |               | (Void Gate)                      | data leakage between security modes.           | basic IPC Endpoint stubs for Voidborn communication.                                                                  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 10 [DONE] | The Process Substrate            | Race conditions in scheduler; overhead of      | Defines Thread (Soul) and Process (World)      |
   |               | (Scheduler & Context Switching)  | per-process PML4 cloning and PCID management.  | structures. Implements PCID-optimized Phase    |
   |               |                                  |                                                | Shifts and the Round-Robin "Pulse."            |
   +---------------+----------------------------------+------------------------------------------------+------------------------------------------------+
   | Day 11 [DONE] | The Genesis Handshake            | The "Privilege Gap"—Paradigm starts with       | ELF Loader + User Mode trampoline. Spawns the                                                                         |
   | (Stage 1)     | (User Mode Transition)           | total power; bricked system if it crashes.     | first process (Paradigm) to construct reality.                                                                        |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 12 [DONE] | Base Solidification              | Complexity of recursive reclamation;           | Implements User-Mode fault isolation, the                                                                             |
   |               | (Debt Repayment)                 | IPC rendezvous deadlocks.                      | Resource Reaper, and basic Synchronous IPC.                                                                           |
   |               |                                  |                                                | Completes the "Minimal Working Product."                                                                              |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 13 [DONE] | Full Context Preservation        | Performance overhead of XSAVE; hardware        | Saves SSE/FPU state during context switches.                                                                          |
   |               | (SSE/FPU Support)                | compatibility (AVX/SSE).                       | Prevents register corruption in user-space math.                                                                      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 14 [DONE] | Lifecycle Syscalls               | Race conditions during wait(); cleanup         | Implements SYS_YIELD, SYS_EXIT, and SYS_WAIT.                                                                         |
   |               | (Yield, Exit, Wait)              | order for complex process trees.               | Provides the basic API for program execution.                                                                        |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 15 [DONE] | The Genesis Bridge               | Security of bootloader-passed objects;         | Injects the first capability into Paradigm.                                           |
   |               | (Initial Cap Injection)          | fragmentation of the initial C-Space.          | Allows user-space to begin constructing reality.                                       |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 16 [DONE] | Authority over Memory            | Risk of double-mapping; physical frame         | Syscalls for map() and unmap() via CAPS.                                              |
   |               | (Map/Unmap Syscalls)             | hijacking if validation is weak.               | Enables user-space to manage its own address space.                                   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 17 [DONE] | Safety & Stability Finalization  | Overhead of lock contention; interrupt         | Stack Guard Pages, Kernel Spinlocks, and                                              |
   |               | (Guards & Locks)                 | latency jitter.                                | Spurious Interrupt handlers. Final Epoch I Seal.                                      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 18 [DONE] | Paradigm Evolution               | User-space complexity; ELF loading             | Replace assembly stub with C-based Paradigm                                           |
   |               | (User Space C Daemon)            | security (bounds checking).                    | daemon loaded as a Multiboot module.                                                  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 19 [DONE] | Law 4: Conditional Runes         | Mode transition serialization; invisibility    | Implement Mode-Aware capabilities (Conditional                                        |
   |               | (Reality Gating)                 | vs. revocation logic.                          | Runes) to enable O(1) global policy shifts.                                           |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 20 [DONE] | The Lattice Bridge (Streaming)   | Register corruption in syscall entry;          | Implementation of high-volume shared-memory                                           |
   |               | (Resonance Lattices)             | page faulting on invalid RBP.                  | IPC via CAP_TYPE_LATTICE.                             |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 21 [DONE] | Fatal Forensics                  | Cryptographic chain corruption; buffer         | Implementation of Fate String auditing from                                           |
   |               | (Auditing)                       | alignment in user space.                       | User Space via Auditor Capabilities.                  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 24 [DONE] | Foundation Hardening & Ocular    | Complexity of asynchronous composition;        | Implementation of O(1) PMM, kmalloc, Doorbell         |
   |               | Projection                       | performance overhead of hyper-scrubbing.       | IPC, and Ocular Projection Engine.                    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 25 [DONE] | PCID Colorization (Law 5)        | Hardware dependency; risk of range exhaustion  | Formally binds PCID ranges to security Realities.     |
   |               |                                  | during high process density.                   | Prevents TLB leaks and enforces isolation.            |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 26 [DONE] | Prismatic Lattices (Law 6)       | Complexity of Void Wall fault handling;        | Implementation of high-performance, zero-copy         |
   |               |                                  | asymmetric permission enforcement.             | shared memory with implicit MMU synchronization.      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 27 [DONE] | Syscall Boundary + Law 2         | Kernel fault risk from malformed user pointers | Hardened user-copy boundaries, introduced strict       |
   |               | Strict Mapping Foundation        | and high blast radius of map-path regressions. | `SYS_MAP/SYS_UNMAP` path, and added syscall metrics.  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 28 [DONE] | Law 2 Strict Adoption Pass       | Regression risk while migrating user-space     | Migrated Paradigm Shadow Mapping to strict map path   |
   |               | (Paradigm Migration)             | mapping flows and enforcing new invariants.    | and added deterministic strict negative-path probes.  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 29 [DONE] | Law 2 Runtime Validation         | Environment mismatch (`make run` GTK) could    | Completed strict unmap adoption in Paradigm and       |
   |               | + Strict Unmap Adoption          | obscure strict-path behavior without fallback.  | verified strict probes via headless serial runtime.   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 30 [DONE] | Fate Strings Rejection Auditing  | Illegal transition attempts were not persisted  | Added fate `result_code`, logged rejected transitions |
   |               |                                  | in audit history, reducing forensic visibility. | in-chain, and validated retrieval/user verification.   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 31 [DONE] | Fate Strings Revalidation Pass   | Single-run validation can hide non-determinism | Re-ran build and headless runtime checks; reconfirmed |
   |               |                                  | or environment-sensitive regressions.           | stable Fate String and strict-path behavior.          |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 32 [DONE] | Fault-to-String Integration      | Fault context was not persisted in Fate ledger,| Added fault metadata and record-type filtering to     |
   |               | (#GP/#PF, filtered reads)        | limiting durable forensic reconstruction.       | Fate Strings; integrated #GP/#PF IDT logging path.    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 33 [DONE] | Full Fault Context Capture       | Minimal fault fields limited post-mortem       | Added CR2/RSP/CS/RFLAGS to Fate fault records and     |
   |               |                                  | analysis fidelity for forensic workflows.       | validated full-context visibility in user-space.      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 34 [DONE] | Real Fault Probe Validation      | Synthetic injection can mask mismatches between | Removed synthetic fault injection and verified real    |
   |               | (No Synthetic Injection)         | idealized and real exception paths.             | recoverable #PF evidence in Fate Strings post-lattice.|
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 35 [DONE] | Refinement Pass                  | Status drift and schema-growth side effects can | Marked Law 2 complete in plan docs and hardened        |
   |               | (Law 2 + Fate Read Hardening)    | reduce confidence in maintainability/security.  | `SYS_FATE_READ` scratch allocation against overflow.    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 36 [DONE] | Fate Read Stability              | User-buffer sizing mismatches can trigger       | Right-sized Paradigm Fate buffers/counts and validated   |
   |               | Revalidation                     | false negatives and obscure forensics confidence.| stable strict-path + Fate audit behavior over 3 boots. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 37 [DONE] | Release Discipline Checklist     | Multi-file status updates can drift and cause   | Added a standard release checklist to keep reports,       |
   |               |                                  | inconsistent closure records.                   | roadmap, and version logs synchronized every cycle.       |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 38 [DONE] | Automated Runtime Matrix         | Manual repeated runtime checks can miss         | Added scripted 3-run headless Law2+Fate validation with   |
   |               | (Law 2 + Fate)                   | regressions and are not reliably repeatable.    | required/forbidden serial marker assertions.              |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 39 [DONE] | Law 9 Documentation Closure      | Law 9 behavior existed in code/runtime logs but | Added a dedicated Law 9 implementation/validation spec and |
   |               | (Temporal Scouring Spec)         | lacked a single source-of-truth operations spec.| synchronized roadmap, reports, and version history links. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 40 [DONE] | ReadOnly Lattices +              | Lattice listener topology and attachment        | Added broadcast lattice minting (source + RO listeners),   |
   |               | Lattice Forensics Detach Path    | lifecycle events lacked explicit audit surface. | lattice Fate record/filter support, and `SYS_LATTICE_DETACH`. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 41 [DONE] | Syscall Boundary Closure Pass    | Signed/unsigned boundary mismatch in Fate read  | Hardened `SYS_FATE_READ` count validation, added negative   |
   |               | (Fate Count + Probe Visibility)  | could silently accept malformed user input and  | count probe, and emitted per-probe PASS/FAIL runtime logs. |
   |               |                                  | hide which probe failed in aggregate reporting. | Improves release-time triage and closure confidence.  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 42 [DONE] | Epoch II Closure Ratification    | Closure drift risk after late-stage hardening   | Ratified Epoch II closure decisions, synchronized roadmap    |
   |               | + Epoch III Plan Kickoff         | and deferred-scope handoff to next epoch.       | status docs, and established an Epoch III vision/security/   |
   |               |                                  |                                                | performance execution plan.                                   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 43 [DONE] | Security Contract Freeze         | Ambiguous cross-process audit authority and     | Reserved `SYS_AUDIT` in ABI with fail-closed kernel behavior, |
   |               | (`SYS_AUDIT` + Zero-Residue)     | partial zero-residue policy can create security | finalized zero-residue policy baseline, and completed build/  |
   |               |                                  | regressions during feature expansion.           | ISO/matrix verification gates for the hardening slice.        |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 44 [DONE] | PMM Architecture Migration       | Legacy frame-only allocation path limits        | Landed policy-aware PMM API plus zoned buddy-backed order      |
   |               | (Zoned Policy -> Buddy Target)   | scalability, fragmentation control, and         | allocation/free paths while preserving wrapper compatibility    |
   |               |                                  | security-intent expression per caller.          | and passing full user/kernel/ISO/matrix validation gates.      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 45 [DONE] | VMM Region Contract Final        | Partial bridge implementations can leave map/   | Completed contract-op/result model, transactional apply+rollback, |
   |               | Closure (Map/Unmap + Rollback)   | unmap asymmetry and weak failure semantics.     | unmap parity, metrics/self-test gates, and matrix-validated runtime. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 46 [WIP]  | Final-Product Envelope           | Architecture drift across mode/PCID/audit paths | Re-baseline around execution envelopes with gated compile/verify |
   |               | Re-Baseline (Phase 0/1)          | can reduce determinism and release confidence.  | /apply/attest delivery and compatibility-first migration.        |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 47 [WIP]  | Day 5R Multi-Mode Envelope       | Legacy mode-transition policy can drift from     | Define envelope-native multi-mode transition contract and map      |
   |               | Logic Kickoff (Phase 2 Bridge)   | scheduler/capability behavior during migration.  | Day 5 legality/escalation semantics into compile/verify/apply flow. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 48 [DONE] | Day 6/Day 7 Final-Product        | Allocator/capability closure drift can leave    | Completed allocator and capability final-product redesign closure,   |
   |               | Redesign Closure                 | foundational primitives below release bar.      | expanded redesign self-tests, and revalidated full build/runtime gates. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 49 [DONE] | Day 8 Gatekeeper Final-Product   | Fault-entry stack safety and gate semantics     | Added bootstrap-safe TSS defaults, IST routing for critical vectors,  |
   |               | Redesign Closure                 | ambiguity can reduce exception-path reliability.| IDT metrics/self-test APIs, and runtime redesign marker validation.    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 50 [DONE] | Day 8 Gate-Semantics             | Documentation/code drift on debug/breakpoint   | Rectified IDT policy to explicit `#DB` trap-gate and `#BP` user       |
   |               | Rectification                    | gate attributes can hide contract regressions.  | trap-gate semantics; extended self-test assertions and revalidated matrix. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 51 [DONE] | Day 45 VMM Contract              | Bridge-only VMM mapping flow can leave partial | Finalized contract-driven map/unmap with explicit result semantics,      |
   |               | Final-Closure Pass               | state and weakly-auditable failure behavior.   | rollback guarantees, runtime self-tests, and matrix-verified gates.      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 52 [DONE] | Syscall Gate Test Strategy       | ABI redesign without closure-grade test matrix | Froze syscall-gate final-product test strategy and wired required gates   |
   |               | Freeze                           | can create hidden regressions at release time. | into release checklist and syscall contract documentation.                |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 53 [DONE] | Syscall ABI v2 Gate              | Direct userspace invocation of internal syscall | Implemented single-entry `SYS_GATE_CALL` ABI with `GATE_OP_*` operations, |
   |               | Envelope Implementation          | numbering reduces boundary control and agility. | migrated userspace wrappers, and validated matrix/runtime markers.         |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+



.. table:: Foundational Debt (To be repaid in Epoch II/III)



   :widths: auto



   +----------------+-------------------------------------------------------------------------+--------------------------------------------------------------+

   | Component      | Debt Description                                                        | Repayment Plan                                               |

   +================+=========================================================================+==============================================================+

   | PMM [DONE]     | Bitmap Ledger is O(N) scan.                                             | [REPAID] O(1) PMM implemented (Epoch II, Day 24).            |

   +----------------+-------------------------------------------------------------------------+--------------------------------------------------------------+

   | Slab [DONE]    | `slab_create_cache` uses raw page alloc (wasteful).                     | [REPAID] Replaced with kmalloc (Epoch II, Day 24).           |

   +----------------+-------------------------------------------------------------------------+--------------------------------------------------------------+

   | Caps [DONE]    | Naive Revocation (Delete doesn't cascade).                              | [REPAID] Cascading revocation implemented (Epoch II, Day 22).|

   +----------------+-------------------------------------------------------------------------+--------------------------------------------------------------+

   | Console        | Serial-only output; VGA stubbed out.                                    | [REPAID] Ocular Projection Engine.                           |

   +----------------+-------------------------------------------------------------------------+--------------------------------------------------------------+

   | Sched [DONE]   | Reaper only runs on pure IDLE; boot soul never idles.                   | [REPAID] Periodic reaping implemented (Epoch II, Day 24).    |

   +----------------+-------------------------------------------------------------------------+--------------------------------------------------------------+

   | Interrupts     | No TSS/IST setup. Kernel stack overflow causes Triple Fault.            | [DONE] TSS and IST configured for stack safety.              |

   +----------------+-------------------------------------------------------------------------+--------------------------------------------------------------+



All foundational debts listed above have been successfully repaid... table:: Future Epochs: Scaling the Multiverse
   :widths: 15 25 30 30

   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Epoch         | Goal                             | Weaknesses & Threats                           | Rationale                                                                                                             |
   +===============+==================================+================================================+=======================================================================================================================+
   | II: Alignment | Vision Alignment                 | Complexity of User-Space transition;           | Aligns the raw kernel with the "Voidborn"                                                                             |
   |               | (ELF, Paradigm, Modes)           | huge surface area increase with Paradigm.      | vision. The Paradigm Daemon must "construct                                                                           |
   |               |                                  |                                                | reality" to prove the architecture works.                                                                             |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | III: Harden   | Security + Performance           | Cross-subsystem policy coupling, scheduler     | Consolidates deferred Epoch II closure items: cross-process                                                           |
   |               | Consolidation                    | determinism pressure, and forensic/cleanup      | audit semantics, full zero-residue hardening, mode-aware                                                             |
   |               |                                  | overhead under mixed workloads.                 | scheduling policy, and measurable runtime confidence gates.                                                          |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | IV: Expand    | Hypervisor + Device Capability   | Virtualization complexity, hardware diversity, | Extends the secure substrate into virtualization/device                                                               |
   |               | Integration                      | and larger attack surface at device boundaries.| abstractions while preserving capability-first isolation                                                               |
   |               |                                  |                                                | and auditability guarantees.                                                                                          |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+

--------------------------
Project Estimates & Status
--------------------------

*   **Current Phase:** Epoch III, Day 53 (Day 5R multi-mode envelope logic bridge remains active; syscall ABI v2 gate envelope and supporting closure slices completed on February 18, 2026).
*   **Total Estimated Time:** 13 - 19 Weeks (~4 Months).
*   **Strategy:** Reaper Envelope (Compatibility-First ABI + Hardware-Enforced Envelope Tiers).

*Note: Time is not a constraint; quality and correctness are the priority.*
