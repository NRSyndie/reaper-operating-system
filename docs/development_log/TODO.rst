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
   |               | (Debt Repayment)                 | IPC rendezvous deadlocks.                      | Resource Reaper, and Synchronous IPC rendezvous as                                                                    |
   |               |                                  |                                                | a Vision/Security/Performance baseline with matrix-gated closure markers.                                            |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 13 [DONE] | Full Context Preservation        | Performance overhead of XSAVE; hardware        | Saves SSE/FPU state during context switches, with                                                                     |
   |               | (SSE/FPU Support)                | compatibility (AVX/SSE).                       | matrix-gated closure markers for init/isolation/stability.                                                           |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 14 [DONE] | Lifecycle Syscalls               | Race conditions during wait(); cleanup         | Implements SYS_YIELD, SYS_EXIT, and SYS_WAIT with                                                                    |
   |               | (Yield, Exit, Wait)              | order for complex process trees.               | matrix-gated closure markers and explicit lifecycle probes.                                                          |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 15 [DONE] | The Genesis Bridge               | Security of bootloader-passed objects;         | Injects the first capability into Paradigm with                                        |
   |               | (Initial Cap Injection)          | fragmentation of the initial C-Space.          | matrix-gated closure markers for module/cap/bootinfo bridge integrity.                 |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 16 [DONE] | Authority over Memory            | Risk of double-mapping; physical frame         | Syscalls for map() and unmap() via CAPS, with                                         |
   |               | (Map/Unmap Syscalls)             | hijacking if validation is weak.               | matrix-gated closure markers for strict rights and unmap/remap lifecycle integrity.   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 17 [DONE] | Safety & Stability Finalization  | Overhead of lock contention; interrupt         | Stack Guard Pages, Kernel Spinlocks, and                                              |
   |               | (Guards & Locks)                 | latency jitter.                                | Spurious Interrupt handlers with matrix-gated closure markers for lock/canary/IRQ7/15.|
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 18 [DONE] | Paradigm Evolution               | User-space complexity; ELF loading             | Replace assembly stub with C-based Paradigm                                           |
   |               | (User Space C Daemon)            | security (bounds checking).                    | daemon loaded as a Multiboot module with matrix-gated ELF/bootstrap closure markers.  |
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
   | Day 46 [DONE] | Final-Product Envelope           | Architecture drift across mode/PCID/audit paths | Implemented kernel execution-envelope compile/verify/apply/attest |
   |               | Re-Baseline (Phase 0/1)          | can reduce determinism and release confidence.  | pipeline with rollback markers while preserving compatibility.    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 47 [DONE] | Day 5R Multi-Mode Envelope       | Legacy mode-transition policy can drift from     | Added kernel envelope transition markers and Paradigm acceptance/   |
   |               | Logic Kickoff (Phase 2 Bridge)   | scheduler/capability behavior during migration.  | rejection probes tied to Day 5 legality/escalation semantics.      |
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
   | Day 54 [DONE] | ESAK Scheduler Authority +       | Scheduler authority drift and SMP budget races | Added root/thread sched-auth capabilities, deterministic weighted RR tokens,   |
   |               | Atomic Budget Hardening          | can violate ceiling/revocation invariants.     | atomic process-budget primitives, revoke-immediate dequeue path, expanded       |
   |               |                                  |                                                | runtime markers, and ratified BSP-only ESAK product profile marker.            |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 55 [DONE] | Day 11 Void Gate                 | Mode/Reality leakage to occupants and stale    | Implemented 4-stage Entry Pipeline (Compile/Verify/Apply/Attest), redacted      |
   |               | Redesign Closure                 | scheduler leases compromise isolation.         | `SYS_MODE_QUERY`, and enforced epoch-aware lease verification in scheduler.     |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 56 [DONE] | Day 12 Closure Ratification      | Historical Day 12 claims lacked deterministic   | Added deterministic Day 12 closure markers and matrix gates for fault isolation, |
   |               |                                  | closure-gated runtime evidence.                 | rendezvous, lifecycle, and process annihilation behavior.                       |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 57 [DONE] | Day 13 Closure Ratification      | Extended-state/context claims could drift        | Added deterministic Day 13 closure markers, forbidden fail markers, and repeat-  |
   |               |                                  | without enforced matrix and repeat-run gates.    | run suite validation for FPU/SSE init and cross-thread isolation.               |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 58 [DONE] | Day 14 Closure Ratification      | Lifecycle syscall behavior lacked release-gated | Added deterministic Day 14 markers/fail markers and repeat-run closure gates for |
   |               |                                  | closure proof under repeated runtime boots.     | wait/yield/lifecycle ABI contract validation.                                   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 59 [DONE] | Day 15 Closure Ratification      | Genesis bridge behavior required deterministic   | Added deterministic Day 15 markers/fail markers, extended matrix gates, and      |
   |               |                                  | closure evidence across matrix and repeat runs. | repeat-run suite for module/capability/bootinfo bridge contract coverage.       |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 60 [DONE] | Day 16 Closure Ratification      | Map/unmap contract regressions could pass        | Added deterministic Day 16 strict-rights and unmap/remap closure markers, fail-  |
   |               |                                  | without explicit required/forbidden runtime gates.| closed paths, and repeat-run validation with synchronized docs.                |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 61 [DONE] | Day 17 Closure Ratification      | Hardening invariants (spinlocks/canary/IRQ      | Added deterministic Day 17 markers/fail markers and repeat-run closure suite to  |
   |               |                                  | filters) needed explicit closure ratification.   | make hardening regressions release-blocking.                                    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 62 [DONE] | Day 18 Closure Ratification      | ELF/bootstrap behavior required enforceable      | Added deterministic Day 18 closure markers, fail markers, matrix gates, and      |
   |               |                                  | closure evidence instead of historical claims.   | repeat-run suite for ELF header/loader/daemon bootstrap contract checks.        |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 63 [DONE] | Day 19 Closure Ratification      | Conditional Rune mode-mask semantics required    | Hardened CAP mode-mask semantics and added deterministic Day 19 closure markers, |
   |               |                                  | stricter fail-closed validation and closure gates.| fail markers, matrix gates, and repeat-run suite evidence.                    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 64 [DONE] | Day 20 Closure Ratification      | Lattice create/attach/detach invariants were     | Hardened Day 20 lattice invariants and added deterministic closure markers/fail  |
   |               |                                  | vulnerable to drift without closure-grade probes.| markers with matrix and repeat-run ratification.                               |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 65 [DONE] | Day 21 Closure Ratification      | Auditor Fate-read authority and bounds checks    | Hardened Day 21 auditor invariants and added deterministic forensic closure      |
   |               |                                  | required explicit, deterministic closure gates.   | markers/fail markers with matrix and repeat-run evidence.                       |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 66 [DONE] | Day 22 Closure Ratification      | Derivation-tree/revocation evidence needed       | Added deterministic Day 22 lineage closure markers/fail markers and repeat-run   |
   |               |                                  | synchronized matrix, suite, and report artifacts.| suite with synchronized matrix/release/conformance gates.                      |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 67 [DONE] | Day 23 Closure Ratification      | Allocator closure evidence gap remained without   | Added deterministic Day 23 allocator closure marker/fail marker and repeat-run   |
   |               |                                  | deterministic marker gates and repeat-run checks.| suite with synchronized matrix/release/conformance artifacts.                  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 68 [DONE] | Day 24 Closure Ratification      | PMM/Law9/Ocular probes needed explicit closure   | Added deterministic Day 24 closure markers/fail markers, Ocular readiness API,   |
   |               |                                  | ratification to keep regressions release-blocking.| and repeat-run suite plus synchronized closure artifacts.                     |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 69 [DONE] | Day 25 Closure Ratification      | PCID switch hygiene and fail-closed validation    | Hardened Day 25 PCID/vmm_switch fail-closed checks, added scrub contract         |
   |               |                                  | needed deterministic closure gates.              | markers/fail markers, and ratified matrix + repeat-run evidence.               |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 70 [DONE] | Day 26 Closure Ratification      | Law 6 Void Wall/attunement closure behavior      | Added deterministic Day 26 Law 6 closure markers/fail markers, hardened lattice |
   |               |                                  | required repeatable matrix/suite ratification.   | fault-window validation, and ratified repeat-run evidence.                     |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 71 [DONE] | Day 27 Closure Ratification      | Boundary-hardening/strict-foundation regressions | Added deterministic Day 27 boundary + strict closure markers/fail markers and    |
   |               |                                  | needed deterministic release-blocking gates.      | extended matrix + repeat-run ratification with synchronized closure artifacts.  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 72 [DONE] | Law 2 Closure Hardening          | Marker-centric closure trust and strict/legacy    | Added kernel-owned Law 2 attestation records/markers, strict-only map/unmap ABI  |
   |               | (Kernel Attestation + Strict API)| ambiguity left Day 28/29/30 vulnerable to drift. | cutover, and ratified matrix + Day 28/29/30 suites with synchronized contracts.   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 73 [DONE] | Day 29 Enhancement Ratification  | Day 29 pass/fail-only evidence and missing reject | Added reason-coded Day 29 kernel attestation coverage, strict-unmap performance    |
   |               | (Reason Coverage + Perf Gate)    | class/perf-budget closure gates allowed residual  | budget gates, expanded Paradigm reject probes, and upgraded matrix/day29 suites    |
   |               |                                  | ambiguity in strict-unmap behavior quality.       | to require reason/performance markers as release-blocking evidence.                |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 74 [DONE] | Day 30 Enhancement Ratification  | Day 30 reject evidence lacked required reason      | Added Day 30 reject-reason mask coverage gates, attestation scan-latency budget,   |
   |               | (Reason Coverage + Perf Gate)    | class coverage and explicit performance budgeting. | deterministic reject probes, and upgraded matrix/day30 suites with release-blocking|
   |               |                                  |                                                  | reason/performance markers.                                                         |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 75 [DONE] | Day 31 Enhancement Ratification  | Day 31 revalidation remained a one-off pass     | Added deterministic Day 31 double-attestation parity and drift-budget gates,       |
   |               | (Determinism + Drift Gate)       | without release-blocking determinism/perf checks.| explicit Day 31 success/fail markers, and upgraded matrix/day31 suite enforcement. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 76 [DONE] | Day 32 Enhancement Ratification  | Day 32 fault integration lacked deterministic   | Added Day 32 fault-filter isolation, fault metadata integrity, and bounded fault   |
   |               | (Filter + Perf Gate)             | closure gates for filter correctness/perf budget.| read-budget markers/fail paths with upgraded matrix/day32 suite enforcement.       |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 77 [DONE] | Day 33 Enhancement Ratification  | Day 33 full-context capture lacked deterministic| Added Day 33 full-context integrity/vector-sanity/performance gates with explicit  |
   |               | (Context + Perf Gate)            | closure gates and explicit runtime budgets.      | Day 33 success/fail markers and upgraded matrix/day33 suite enforcement.           |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 78 [DONE] | Day 34 Enhancement Ratification  | Day 34 real-path validation lacked deterministic| Added Day 34 real-fault-path/provenance/performance gates with explicit Day 34     |
   |               | (Real Path + Perf Gate)          | closure gates and bounded audit runtime evidence.| success/fail markers and upgraded matrix/day34 suite enforcement.                  |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 79 [DONE] | Slot 1 / Step 3 Concurrency      | SMP race conditions in shared data structures;  | Implemented `rwlock`, `seqlock`, and `rcu` primitives  |
   |               | Primitives                       | lock contention overhead.                       | with deterministic boot self-tests.                   |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 80 [DONE] | Slot 1 / Step 4 Buffered IPC     | Buffer overflow in message queues; latency in   | Implemented circular-buffered IPC endpoints with      |
   |               |                                  | rights-aware cap invocation.                    | mandatory rights-mask enforcement.                    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 81 [DONE] | Slot 1 / Step 5 Memory Upgrades  | Complexity of #PF handling for COW; huge page   | Implemented Demand Paging, COW, and Huge Page support |
   |               |                                  | fragmentation.                                  | via software-PTE bits and hardened #PF dispatcher.    |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 82 [DONE] | Slot 1 / Step 1 & 2 SMP + TLB    | AP core bring-up race conditions; TLB          | Implemented SMP AP bring-up and IPI-based TLB         |
   |               |                                  | shootdown latency over IPI.                     | shootdown for cross-core consistency.                 |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 83 [DONE] | Audit Foundation & Fate String   | Sealed-storage root entropy remains deferred   | Implemented 128-byte aligned, SMP-safe atomic audit   |
   |               | Redesign                         | for Ghost Mode fallback hardening.             | lattice with BLAKE3 chaining, RDRAND primary seed, overflow tracking, and reality-bound seed rotation. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 84 [DONE] | ACPI Layer 1 & 2 Foundation      | Firmware table diversity; AML-dependent        | Implemented RSDP/RSDT/XSDT discovery plus static parsing |
   |               |                                  | interpretation remains explicitly deferred.    | for MADT, FADT, HPET, MCFG, and DMAR with boot self-test evidence. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 85 [DONE] | Architecture + Documentation     | Architecture drift and stale implementation    | Synchronized architecture/status/report/version docs with |
   |               | Synchronization                  | claims obscured the real system state.         | the current Genesis/Paradigm/Sentinel and Day 83/84 code reality. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 86 [DONE] | DMA Authority Contract + DMAR    | Hardware isolation could drift into implicit   | Froze canonical DMAR inventory, explicit IOMMU state/    |
   |               | Truth Freeze                     | policy without a truth/audit model.            | degraded policy, and audit-visible inventory semantics before VT-d enablement. |
   +---------------+----------------------------------+------------------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
   | Day 87 [DONE] | Paradigm Stack Baseline Closure  | Early userspace bootstrap could fault below    | Expanded the Genesis-mapped Paradigm user stack from one  |
   |               |                                  | the fixed entry RSP and invalidate runtime     | page to eight pages, removed the observed `0x7ffdd0` boot fault, and revalidated the runtime matrix in three consecutive clean passes. |
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



All foundational debts listed above have been successfully repaid.

.. table:: Future Epochs: Scaling the Multiverse
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

---------------------------------------------------
Implementation Gap Checklist (Confirmed Missing)
---------------------------------------------------

The following items were re-audited in current kernel/userspace sources and are
still missing as concrete implementations (not merely renamed).

Slot 1: Standard Microkernel Tasks (Direct Baseline Additions)
===============================================================

These are features we can implement in mostly conventional form first.

Memory Management

- [x] Memory path upgrades (demand paging, COW, huge pages).
  (Implemented via software-PTE bits and #PF handler logic).

Scheduling and IPC

- [ ] Priority-based scheduler policy.
  (Current policy is deterministic weighted RR tokens, not priority scheduling.)
- [ ] Real-time scheduling classes (SCHED_FIFO / SCHED_RR semantics).
- [ ] Tickless scheduling (NO_HZ-style dynamic tick suppression).
  (Current runtime uses periodic PIT heartbeat.)
- [x] Buffered message queues.
  (Implemented via `ipc_endpoint_t` circular buffer and rights-aware `sys_cap_invoke`).
- [ ] IPC call-chain propagation/tracking.

SMP and Concurrency

- [x] SMP bring-up (booting AP cores in runtime path).
  (Implemented via per-CPU GDT/TSS/IDT and scheduler activation).
- [x] TLB shootdown over IPI for cross-core address-space invalidation.
- [ ] Full multicore load balancing across active AP run queues.
  (Current same-mode steal scaffolding exists, but AP runtime is not active.)
- [x] RCU primitives.
  (Baseline `rcu` reader/epoch synchronization primitive is now present in `kernel/include/utils.h`.)
- [x] Reader-writer locks.
  (Baseline `rwlock` primitives are now present in `kernel/include/utils.h`.)
- [x] Seqlocks.
  (Baseline `seqlock` primitives are now present in `kernel/include/utils.h`.)

Security Hardening and Reliability

- [ ] ASLR for kernel virtual layout randomization.
  (Kernel image is linked at a fixed high-half base today.)
- [ ] KASLR boot-time randomized kernel load offset.
- [ ] SMEP/SMAP enablement and enforcement path.
- [ ] KPTI (kernel/user page table isolation split).
- [ ] User-space stack guard pages.
  (Kernel stack canaries exist; explicit user stack guard mapping is not present.)
- [ ] Retpoline / Spectre-v2 compiler+runtime mitigation path.
- [ ] Panic-to-restart orchestration policy.
  (Kernel panic exists and explicit reboot primitive exists, but panic path halts.)
- [ ] Watchdog timer integration.
- [ ] GDB remote debugging stub.

Device and Virtualization Baseline

- [x] ACPI Layer 1/2 table discovery and static parsing.
  (Implemented via `acpi_init()` + `acpi_find_table()` with MADT/FADT/HPET/MCFG/DMAR parsing and `[TEST] ACPI Layer 1+2: SUCCESS.`.)
- [ ] DMA mapping API (map/unmap + ownership/IOVA lifecycle).
  (PMM has a DMA zone, but no dedicated DMA mapping interface.)
- [x] IOMMU inventory initialization and degraded-policy truth model.
  (Implemented via `iommu_init()` inventory classification and boot self-tests; VT-d translation enablement remains open.)
- [ ] MSI/MSI-X programming and interrupt routing support.
- [ ] VT-x/AMD-V (or EL2 on ARM) initialization path.
- [ ] VMCS/VMCB lifecycle management.
- [ ] EPT/NPT nested translation support.
- [ ] VM-exit handling pipeline.

Slot 2: Reaper-Core Tasks (Vision/Security/Performance Adaptations)
====================================================================

These tasks require Reaper-Core-specific behavior and contracts, not only
drop-in conventional implementations.

- [ ] Per-mode capability spaces (or equivalent authority partitioning model).
  (Mode gating exists today; this is a deeper authority-space split.)
- [ ] Device capability delegation model integrated with lineage and revocation.
- [ ] MPK/PKU integration with mode/envelope policy (PCID fallback preserved).
- [ ] Cache-line alignment policy for hot structs with measured closure budgets.
- [ ] Slot-1 feature adaptations to Fate markers and closure gates
  (required/forbidden markers + repeat-run evidence for each new subsystem).
- [ ] Mode/reality-aware policy shaping for new scheduler, memory, and IPC paths
  (fail-closed behavior under transitions and epoch changes).

Slot 1 Start Order
==================

Start with foundations that unlock the rest of Slot 1:

Current kickoff status:
- [x] Slot 1 / Step 1 landed (SMP bring-up + CPU ID correctness + IPI transport skeleton).
- [x] Slot 1 / Step 2 landed (TLB shootdown over IPI).
- [x] Slot 1 / Step 3 landed (`rwlock` + `seqlock` + baseline `rcu` primitives with kernel boot self-test markers).
- [x] Slot 1 / Step 4 landed (Buffered IPC queues + rights-aware endpoint invocation).
- [x] Slot 1 / Step 5 landed (Memory path upgrades: demand paging, COW, huge pages).

- [x] 1) SMP bring-up + CPU ID correctness + IPI transport skeleton.
- [x] 2) TLB shootdown over IPI.
- [x] 3) Conventional lock primitives + baseline RCU (`rwlock`, `seqlock`, `rcu`).
- [x] 4) Buffered IPC queues.
- [x] 5) Memory path upgrades (demand paging, COW, huge pages).

--------------------------
Project Estimates & Status
--------------------------

*   **Current Phase:** Epoch III Closure Ratification + Slot 1 Standard Microkernel Foundations (Days 46/47/54/55 closed on February 24, 2026; Days 12/13/14/15 closures ratified on March 1, 2026; Days 16/17/18/19/20/21/22/23/24/25/26/27 closures ratified on March 2, 2026; Day 72 Law 2 closure hardening ratified on March 7, 2026; Day 73 Day 29 enhancement ratification completed on March 7, 2026; Day 74 Day 30 enhancement ratification completed on March 7, 2026 with reason/performance-gated evidence; Day 75 Day 31 enhancement ratification completed on March 7, 2026 with deterministic revalidation and drift-budget gating; Day 76 Day 32 enhancement ratification completed on March 7, 2026 with fault-filter and read-budget gating; Day 77 Day 33 enhancement ratification completed on March 7, 2026 with full-context integrity and audit-budget gating; Day 78 Day 34 enhancement ratification completed on March 7, 2026 with real-path provenance and audit-budget gating; Day 79 Slot 1 Step 3 baseline concurrency primitives landed on March 15, 2026; Day 80 Slot 1 Step 4 buffered IPC queues landed on April 19, 2026; Day 81 Slot 1 Step 5 memory path upgrades landed on April 19, 2026; Day 82 Slot 1 Step 1/2 SMP bring-up and TLB shootdown landed on April 19, 2026 with matrix revalidation; Day 83 audit foundation hardening landed with BLAKE3-backed chaining and RDRAND-first seeding on April 26, 2026; Day 84 ACPI Layer 1/2 table discovery and static parsing landed on April 29, 2026; Day 85 architecture/documentation synchronization landed on April 29, 2026; Day 86 DMA authority contract and DMAR truth freeze landed on April 29, 2026; Day 87 Paradigm stack baseline closure completed on May 13, 2026 with three consecutive clean matrix invocations after the bootstrap stack fix. Broader memory-hardening items such as VT-d translation enablement, production DMA ownership lifecycle, boot-time randomized KASLR, and explicit user stack guard pages remain in progress).
*   **Total Estimated Time:** 13 - 19 Weeks (~4 Months).
*   **Strategy:** Reaper Envelope (Compatibility-First ABI + Hardware-Enforced Envelope Tiers).

*Note: Time is not a constraint; quality and correctness are the priority.*
