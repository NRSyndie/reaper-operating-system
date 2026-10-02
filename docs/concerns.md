# Reaper-OS Concerns (Current Findings)

This document records potential errors and risks identified during static code inspection.  
It now also tracks remediation status after implementation/testing updates.

## Scope

- Review type: static source inspection
- Focus: correctness, safety, ABI consistency, runtime behavior
- Excluded: style preferences, conventional-vs-unconventional architecture choices

## Findings

### 1) Scheduler may resume invalid execution state when no runnable thread exists
- Severity: Critical
- Files: `kernel/scheduler.c:115`, `kernel/scheduler.c:128`, `kernel/scheduler.c:149`, `kernel/scheduler.c:155`
- Concern:
  - In `schedule()`, if no ready thread exists and `current_thread` is not `THREAD_RUNNING`, the code executes `sti; hlt` and returns without selecting a valid runnable context.
- Risk:
  - Returning to a blocked/zombie thread context can produce undefined behavior or hard-to-reproduce lockups.

### 2) Kernel secure-context PCID path conflicts with kernel-mode PCID validation
- Severity: Critical
- Files: `kernel/mode.c:257`, `kernel/vmm.c:130`
- Concern:
  - `mode_enter_secure_context()` calls `vmm_switch(..., PCID_KERNEL_SECURE, MODE_KERNEL)`.
  - `vmm_switch()` panics in `MODE_KERNEL` unless PCID is exactly `PCID_KERNEL` (0).
- Risk:
  - Triggering secure-context transition can intentionally/accidentally panic the kernel.

### 3) Thread count incremented twice during thread creation
- Severity: High
- Files: `kernel/thread.c:56`, `kernel/thread.c:59`
- Concern:
  - `owner->thread_count++` is executed twice in `thread_create()`.
- Risk:
  - Incorrect lifecycle accounting can delay or prevent process destruction and cause resource leaks.

### 4) Lattice frame-pointer capacity mismatch (possible out-of-bounds write)
- Severity: High
- Files: `kernel/lattice.c:17`, `kernel/lattice.c:35`, `kernel/lattice.c:43`
- Concern:
  - `page_count` accepts up to 1024.
  - `frames` pointer array storage is one page (4KB), which only fits 512 `uint64_t` entries.
- Risk:
  - For `page_count > 512`, writing `lattice->frames[i]` can corrupt adjacent memory.

### 5) Page-fault handler advances RIP by fixed byte length
- Severity: High
- Files: `kernel/idt.c:167`
- Concern:
  - On write-fault path (`regs->err_code & 2`), handler does `regs->rip += 3`.
  - x86 instruction lengths are variable.
- Risk:
  - Control flow may jump into middle of instruction stream, causing further faults or corruption.

### 6) `SYS_VOID_LOG` uses raw user pointer with `%s` in kernel context
- Severity: High
- Files: `kernel/syscall.c:144`
- Concern:
  - Kernel prints `(const char*)a0` directly without robust user-memory copy/validation.
- Risk:
  - Invalid/unmapped pointer can fault kernel or read unintended memory.

### 7) `SYS_FATE_READ` copies directly into user buffer in kernel context
- Severity: High
- Files: `kernel/syscall.c:392`
- Concern:
  - `memcpy(user_buf, kernel_buf, ...)` occurs after only a coarse pointer/range check.
- Risk:
  - Invalid user mapping can cause kernel fault during copy.

### 8) `SYS_MAP` does not enforce capability-right subsets on PTE flags
- Severity: Medium
- Files: `kernel/syscall.c:222`, `kernel/syscall.c:223`
- Concern:
  - User-provided `flags` are masked and installed without explicit rights-policy validation.
  - Source comment acknowledges this gap.
- Risk:
  - Mapping authority can exceed intended capability policy boundaries.

### 9) `SYS_CAP_RETYPE` has insertion-failure leak and incomplete lineage linkage
- Severity: Medium
- Files: `kernel/syscall.c:280`, `kernel/syscall.c:282`, `kernel/syscall.c:283`
- Concern:
  - On `cap_insert` failure, `new_ident` is not freed.
  - At discovery time, lineage linkage was incomplete, so revocation semantics might not propagate as intended.
- Risk:
  - Memory/resource leak and weaker authority graph consistency.

### 10) Lattice references are incremented on attach but not released in process teardown
- Severity: Medium
- Files: `kernel/process.c:77`, `kernel/process.c:45`, `kernel/process.c:63`
- Concern:
  - `process_attach_lattice()` increments refcount via `lattice_ref(lattice)`.
  - `process_destroy()` does not release attached lattice references.
- Risk:
  - Persistent lattice/frame leaks across process destruction.

### 11) Shared capability enum is out of sync with kernel capability enum
- Severity: Medium
- Files: `shared/include/capability.h:20`, `kernel/include/capability.h:32`
- Concern:
  - Kernel includes `CAP_TYPE_AUDITOR`; shared header does not.
- Risk:
  - ABI drift between user-space and kernel expectations, especially for future user-space enum consumers.

## Status Update (2026-02-08)

Resolved:
- Item 1 (scheduler idle invalid return path): fixed by retry-based idle loop in `kernel/scheduler.c`.
- Item 2 (kernel PCID contradiction): fixed by allowing `PCID_KERNEL_SECURE` in `MODE_KERNEL` path in `kernel/vmm.c`.
- Item 3 (double thread_count increment): fixed in `kernel/thread.c`.
- Item 4 (lattice frames capacity overflow): fixed with dynamic frames-array allocation in `kernel/lattice.c`.
- Item 5 (fixed `RIP += 3` fault skip): removed from `kernel/idt.c`.
- Item 6 and Item 7 (unsafe user pointer usage): hardened via user-copy helpers in `kernel/syscall.c`.
- Item 8 (`SYS_MAP` rights policy gap): added rights-masked PTE flag enforcement in `kernel/syscall.c`.
- Item 9 (`SYS_CAP_RETYPE` leak/lineage TODO): replaced with `cap_retype()` lineage-aware path in `kernel/capability.c` + syscall usage in `kernel/syscall.c`.
- Item 10 (lattice ref leak in process teardown): fixed in `kernel/process.c`.
- Item 11 (shared/kernel capability enum drift): synchronized in `shared/include/capability.h`.

Additionally completed:
- `SYS_WAIT` now has a minimal implemented contract (`kernel/syscall.c`, `kernel/thread.c`, `kernel/include/process.h`).
- `SYS_UNMAP` now has an implemented syscall path with authority checks (`kernel/syscall.c`).
- Introduced strict rollout path for Law 2 mapping semantics:
  - `SYS_MAP` strict-only control (`a4 == 1`)
  - `SYS_UNMAP` strict-only control (`a2 == 1`)
- Added syscall observability counters and periodic diagnostics for rollout safety:
  - fault/invalid/permission rejection counts
  - map/unmap strict usage and failure-reason counters
  - TLB flush count
- GNU-stack metadata warnings for assembly were removed via `.note.GNU-stack` in:
  - `kernel/interrupts.s`
  - `user/lib/start.s`

Runtime/build verification completed:
- Full rebuild and ISO generation pass.
- Headless QEMU smoke boot pass with successful Paradigm flow (`kernel/serial_smoke.log`).

Residual risks:
- User-copy hardening is improved, but still uses software range/mapping checks rather than hardware-fault-safe guarded copy mechanisms.
- `SYS_WAIT` currently provides minimal same-process "wait for any peer exit" semantics, not full process-wait API richness.
- Strict map/unmap is now strict-only in the public user API and closure-gated by kernel-owned Law 2 attestations.

---

## Status Update (2026-08-16) — External-review remediation plan

This section records the concrete, finite remediation plan for a third-party
architecture review. Each item is a single named action with a real artifact
or an explicit written choice behind it. No aggregate/prose claims here; each
row states exactly what is checked and how.

### A) Section 9 hardening claims — four distinct self-tests (do NOT fold together)

The vision document (`docs/project_vision_and_architecture.md` §9) currently
claims deterministic capability-based memory protection is "mathematically
superior" to ASLR. Until each hardening control has its own runtime self-test,
§9 may state only **"SMEP/SMAP enforced (verified)"** and list the other three
as **"planned, unverified"** — no aggregate claim covering all four.

| Control | Verification mechanism (each its own test) | Status |
|---|---|---|
| SMEP | `kernel/main.c` self-test reads `CR4` bit 20, emits PASS/FAIL | planned |
| SMAP | `kernel/main.c` self-test reads `CR4` bit 21, emits PASS/FAIL | planned |
| KPTI | self-test compares `CR3` across a user→kernel transition: if page tables are actually split, kernel-only mappings must be provably absent from the user `CR3`, and a targeted read attempt from userspace must fault | planned |
| KASLR | self-test compares the kernel's actual load address against the linked base across ≥3 separate boots (matches the existing "3 consecutive runs" pattern); if ever identical, KASLR is not happening | planned |
| Stack guard pages | self-test intentionally provokes a controlled overflow into the guard page and confirms a fault (no silent corruption of adjacent memory) | planned |

### B) `GENESIS_OP_MINT` dispatch gate — CONFIRMED kernel-boundary (type-1), not convention

Confirmed by reading the literal current function at `kernel/genesis.c:363-469`
(`genesis_syscall_dispatch`). The gate is enforced at the kernel boundary for
every op, *before* the op `switch`:

```c
uint64_t genesis_syscall_dispatch(process_t* owner, uint32_t op, uint32_t cap_slot,
                                   uint64_t req_ptr, bool is_kernel) {
    if (!owner || !owner->cspace) return (uint64_t)-1;
    if (cap_genesis_is_exhausted())  return (uint64_t)-1;   // post-destroy: all ops die
    /* Validate the Genesis Capability in the caller's cspace. */
    cap_identity_t* ident = cap_lookup(owner->cspace, cap_slot);
    if (!ident || ident->type != CAP_TYPE_GENESIS) return (uint64_t)-1;  // must present GENESIS_CAP
    switch (op) { ... }
}
```

Two kill switches sit before the `switch`, so any future `GENESIS_OP_MINT`
added inside this dispatcher inherits both automatically:
1. `cap_genesis_is_exhausted()` — after Genesis destroys its `GENESIS_CAP`
   (`cap_genesis_exhaust()`), every op returns `-1` permanently.
2. `cap_lookup(owner->cspace, cap_slot)` must return a live `CAP_TYPE_GENESIS`
   identity in the caller's own cspace.

Additionally, spawned daemons receive `.genesis_cap_slot = 0` (`kernel/genesis.c:396`),
so they do not receive the capability at all.

**Written precondition for the B3 mint primitive:** `GENESIS_OP_MINT` must
land *inside* this gated dispatcher (not as a free-function op), and must ship
with a deterministic positive/negative probe pair before it is greenlit.

### C) One-shot provisioning — architectural decision (default: option a)

Decision to be written into the vision document §6 (Genesis's role) as an
explicit choice, not left implicit. Two candidate answers:

- **(a) Genesis is one-shot.** All authority is provisioned once at boot.
  Anything needing new authority later (new Reality, respawned daemon, Ghost
  Mode microVM) obtains it by *delegation* from an already-provisioned daemon
  (e.g. Paradigm delegating a subset of what it already holds), never by fresh
  minting. Consistent with Zero-Residue / immutable Fate Strings philosophy
  (additive-only; nothing created from nothing after Genesis dies).
- **(b) Standing mint authority.** A daemon (probably Paradigm) needs mint
  authority for the system's lifetime, meaning that daemon requires the same
  TCB-boundary scrutiny as `GENESIS_OP_MINT`.

**Default: (a).** It is more consistent with the architecture (Genesis dies,
Fate Strings are additive, capabilities are unforgeable). To override to (b) a
concrete runtime scenario requiring fresh post-boot minting must be documented
first. This requires one paragraph in `docs/project_vision_and_architecture.md`
§6 stating the choice explicitly.

### D) Locked items (no further re-litigation)

- **Area 4 B1 + B2: DONE.** Dual-module split completed 2026-09-10. See
  `docs/reports/day91_final_report.md`.
- **PID-authority gate:** build `tools/check_no_ambient_pid_authority.sh` — a
  grep-based manual gate matching the existing `tools/run_dayNN_closure_suite.sh`
  pattern (`set -euo pipefail`, argparse), that exits non-zero if a raw
  `pid == 1` / `pid != 1` / `->pid ==` privilege switch reappears in `kernel/`.
  Run by hand before closing a closure day (this project has manual gates, not CI).
- **Timeline removal:** remove the stale "13-19 Weeks (~4 Months)" estimate from
  `docs/development_log/TODO.rst`, per the stated "time is not a constraint"
  philosophy.
- **Doc-hygiene rule:** planning/implementation artifacts must not contain
  editorial scratch reasoning (internal-monologue reconciliation); plan docs are
  clean spec, and plan-vs-implementation reconciliation happens in the day report.
