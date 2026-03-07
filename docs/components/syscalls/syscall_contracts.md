# Reaper-OS Syscall Contracts (Hardening Baseline)

This document freezes the argument and failure contracts for high-risk syscall surfaces targeted by the current hardening pass.

Associated final-product test policy:

- `docs/components/syscalls/syscall_gate_testing_strategy.md`

## Syscall ABI v2 (Gate Envelope)

User space enters kernel space through a single syscall number:

- `SYS_GATE_CALL`

The actual operation is selected by gate op id (`GATE_OP_*`) and a fixed payload envelope:

- `a0`: gate op id
- `a1`: user pointer to `gate_call_msg_t`
- `a2`: payload size (must be `sizeof(gate_call_msg_t)`)
- `a3`: reserved flags (`0` for current surface)
- `a4`: reserved (`0`)

`gate_call_msg_t` carries per-operation arguments in `args[0..5]`.
Kernel translates gate op -> internal handler id and applies the same validation/security rules described below.

## `GATE_OP_MODE_TRANSITION` (25)

ABI:

- `a0`: target mode (`MODE_CASUAL|MODE_SECURE|MODE_LOCKDOWN|MODE_GHOST`)
- `a1`: auth evidence flags (`MODE_AUTH_*`)

Required invariants:

- Transition is evaluated by kernel envelope pipeline (`compile -> verify -> apply -> attest`).
- Illegal transitions fail closed with `-1`.
- Transition policy checks include source legality, auth requirements, and cooldown/de-escalation windows.

## `GATE_OP_CAP_MINT` (3)

ABI:

- `a0`: source capability slot
- `a1`: destination capability slot
- `a2`: new rights bitmask
- `a3`: badge
- `a4`: allowed mode mask

Required invariants:

- Caller must own a valid source capability with grant rights.
- Mode mask must be fail-closed valid:
  - non-zero
  - subset of `CAP_MODE_VALID_MASK` only (no undefined bits).
- Derived mode set is monotonic:
  - `child.allowed_modes = parent.allowed_modes & requested_allowed_modes`
  - zero effective mode set is rejected.
- Rights remain monotonic narrowing (`new_rights` subset of parent rights).

## General Return Rules

- `0`: Success.
- `-1`: Rejected or failed (invalid args, permission, or fault check failure).
- `1`: Non-error "not ready/no event" for `SYS_WAIT` non-blocking semantics.

## Pointer Safety Contract

All pointer-taking syscalls must pass user-range and mapping checks before any kernel copy:

- Read from user buffer: must be user-space canonical and mapped readable.
- Write to user buffer: must be user-space canonical and mapped writable.
- Invalid pointers must fail with `-1`, never panic the kernel.

## `SYS_MAP` (9)

ABI:

- `a0`: parent page-table capability slot.
- `a1`: table index (`0..511`).
- `a2`: child capability slot (must be non-zero).
- `a3`: requested PTE flags.
- `a4 bit0`: tolerated strict marker for ABI continuity (semantics are strict regardless).

Required invariants:

- Parent capability must exist and be `CAP_TYPE_PAGETABLE`.
- Parent must include `CAP_RIGHT_WRITE`.
- Index must be in range.
- Child must be `CAP_TYPE_RAM` or `CAP_TYPE_PAGETABLE`.
- Effective mapping flags are masked by child capability rights.
- Reject unknown user PTE bits outside `{PRESENT, USER, WRITABLE, NX}`.
- For non-leaf links (`child == CAP_TYPE_PAGETABLE`), require `USER|WRITABLE`.
- `child == 0` is rejected; unmapping is only via `SYS_UNMAP`.

## `SYS_UNMAP` (10)

ABI:

- `a0`: parent page-table capability slot.
- `a1`: table index (`0..511`).
- `a2 bit0`: tolerated strict marker for ABI continuity (semantics are strict regardless).

Required invariants:

- Parent capability must be `CAP_TYPE_PAGETABLE`.
- Parent must include `CAP_RIGHT_WRITE`.
- Index must be in range.

## `SYS_FATE_READ` (14)

ABI:

- `a0`: user destination buffer (`struct mode_transition*`).
- `a1`: requested record count.
- `a2`: auditor capability slot.
- `a3`: read mode filter:
  - `0`: all records
  - `1`: transition records only
  - `2`: fault records only
  - `3`: lattice attach/detach records only

Required invariants:

- Auditor capability must exist and be `CAP_TYPE_AUDITOR`.
- Auditor capability must include `CAP_RIGHT_READ`.
- Destination buffer must be 8-byte aligned.
- Destination range must be mapped writable in caller address space.
- Count is capped internally (currently `128`).
- Read mode outside `{0,1,2,3}` must fail with `-1`.
- Kernel copy count must remain bounded by requested count and fail closed on mismatch.

Record semantics:

- Returned records are ordered newest-first.
- `record_type` values:
  - `0`: transition record
  - `1`: fault record
  - `2`: lattice forensics record
- `result_code` values:
  - `0`: accepted transition event
  - `1`: rejected/illegal transition attempt
- For transition records (`record_type == 0`), `fault_error_code` carries deterministic policy reject reason code (`MODE_REJECT_*`) on rejected events.
- Fault metadata fields (for `record_type == 1`):
  - `fault_vector`: x86 exception vector
  - `fault_error_code`: low 32 bits of CPU error code
  - `fault_rip`: faulting instruction pointer
  - `fault_cr2`: CR2 fault address (`#PF`), else `0`
  - `fault_rsp`: stack pointer at fault
  - `fault_cs`: code segment selector at fault
  - `fault_rflags`: rflags snapshot at fault

- Lattice metadata fields (for `record_type == 2`):
  - `fault_vector`: lattice action (`1`: attach, `2`: detach)
  - `fault_error_code`: lattice page count
  - `fault_rip`: role (`1`: source/RW, `0`: listener/RO)
  - `fault_cr2`: lattice attachment base virtual address

## Auditor Contract (Epoch II Closure Note)

- Epoch II auditor capability is delivered as `CAP_TYPE_AUDITOR` and exercised via `SYS_FATE_READ`.
- A standalone `SYS_AUDIT` syscall was not part of the Epoch II ABI surface.
- Cross-process cryptographic verification semantics are deferred to Epoch III because current Epoch II process metadata does not expose a stable authority model for third-party target selection/ownership proofs.
- Closure decision (Epoch II):
  - **Epoch II:** `CAP_TYPE_AUDITOR + SYS_FATE_READ` is the supported auditor path.
  - **Epoch III:** reserve explicit cross-process audit syscall once target identity and delegation semantics are formalized.

## `SYS_AUDIT` (20) — Epoch III Contract Freeze

ABI (frozen for Day 43; implementation intentionally fail-closed):

- `a0`: target process identity token (PID-space identifier, exact semantics frozen but not yet enforced).
- `a1`: audit flags (must be `0` for current freeze window).
- `a2`: optional output buffer pointer (reserved; ignored by current stub).
- `a3`: optional output record/request count (reserved; ignored by current stub).

Current required behavior:

- Syscall number is reserved in shared/kernel/user ABI headers.
- Kernel must return `-1` unconditionally (fail-closed) until full authority/delegation model lands.
- Kernel emits one-time diagnostic marker indicating frozen-but-unimplemented state.

Security rationale:

- Prevents accidental permissive behavior while cross-process authority semantics are still under design.
- Keeps ABI stable so user/kernel integration work can proceed without reopening syscall numbering.

## `SYS_SCHED_METRICS` (21)

ABI:

- `a0`: user destination buffer (`gate_sched_metrics_t*`).
- `a1`: user destination buffer size (must be `>= sizeof(gate_sched_metrics_t)`).

Required invariants:

- Destination buffer must be writable user memory for the full metrics structure.
- Buffer size below struct size must fail with `-1`.
- Call is read-only and must not mutate scheduler state beyond lock-safe metric reads.

Record semantics:

- `schedule_count`: scheduler invocations on the current CPU queue.
- `switch_count`: successful context switches where `old != next`.
- `remote_enqueue`: enqueue/wake calls targeting a non-local CPU queue.
- `migrations`: observed handoffs where previous and next thread `last_cpu` differ.
- `denied_enqueue`, `denied_wake`, `denied_dispatch`: mode-policy denials at scheduler boundaries.
- `denied_no_auth`: scheduling authority denials at enqueue/wake/dispatch.
- `denied_mode_mismatch`: envelope-mode mismatch denials.
- `budget_exhaustions`: thread/process budget exhaustion blocks.
- `envelope_switches`: active envelope switch count on current CPU.
- `active_security_epoch`: scheduler-observed security epoch.
- `cpu_id`: current logical CPU id.
- `active_mode`: scheduler active envelope mode.
- `ready_depth`, `zombie_depth`: queue depths at snapshot time.

## `SYS_SCHED_AUTH_ROOT_MINT` (22)

ABI:

- `a0`: mode binding (`MODE_CASUAL|MODE_SECURE|MODE_LOCKDOWN|MODE_GHOST`)
- `a1`: `max_total_budget`
- `a2`: `refill_period_ticks`
- `a3`: `max_accumulated`
- `a4`: destination capability slot

Required invariants:

- Caller must own privileged mint authority (currently PID 1 bootstrap rule).
- Destination slot must be empty.
- Mode binding must be a valid non-kernel user mode.
- Budgets/period must be non-zero.
- Minted object type is `CAP_TYPE_SCHED_AUTH_ROOT`.

Failure behavior:

- Return `-1` on privilege, slot, mode, or parameter violations.

## `SYS_SCHED_AUTH_THREAD_DERIVE` (23)

ABI:

- `a0`: root auth slot (`CAP_TYPE_SCHED_AUTH_ROOT`)
- `a1`: destination slot for derived thread auth
- `a2`: `max_slice`
- `a3`: `weight`
- `a4`: `local_max_accumulated`

Required invariants:

- Root cap must exist and be `CAP_TYPE_SCHED_AUTH_ROOT`.
- Derivation is reduction-only:
  - `max_slice <= root.max_total_budget`
  - `local_max_accumulated <= root.max_accumulated`
- Derived thread auth inherits root mode binding.
- Destination slot must be empty.

Failure behavior:

- Return `-1` on missing/invalid root, ceiling violation, slot conflict, or malformed arguments.

## Observability Counters

The kernel tracks counters for:

- fault rejections, invalid-argument rejections, permission rejections
- `SYS_MAP`/strict-map calls and failure reasons (strict is always on)
- `SYS_UNMAP`/strict-unmap calls (strict is always on)
- TLB flush count from mapping operations

Periodic diagnostics are emitted from the syscall path to support rollout validation.

## `SYS_LATTICE_CREATE` (12)

ABI:

- `a0`: page count.
- `a1`: source lattice capability destination slot (RW+GRANT).
- `a2`: optional read-only listener count (`0..2`).
- `a3`: listener slot 0 (used if `a2 >= 1`).
- `a4`: listener slot 1 (used if `a2 >= 2`).

Required invariants:

- Page count must pass lattice creation limits.
- Listener count must be `<= 2`.
- Source/listener destination slots must be distinct.
- Source slot must be non-zero.
- Listener destination slots must be non-zero when used.
- Source cap insert must succeed.
- Listener caps are minted from source as `CAP_RIGHT_READ` only.
- On mint failure, partially created slots are rolled back.

Notes:

- Legacy single-cap behavior remains: pass `a2=0`.
- Broadcast creation provides one writer/source and up to two read-only listener caps in one syscall.

## `SYS_LATTICE_ATTACH` (13)

ABI:

- `a0`: lattice capability slot.
- `a1`: user virtual base address for lattice mapping.

Required invariants:

- Capability must exist and be `CAP_TYPE_LATTICE`.
- Capability must include `CAP_RIGHT_READ`.
- Virtual address must be page-aligned and in user canonical range.
- Lattice span (`page_count * PAGE_SIZE`) must remain within user virtual bounds.
- Attachment must not overlap existing lattice attachments in the same process.
- Duplicate attachment of the same `(lattice, vaddr)` pair is rejected fail-closed.

## `SYS_ATTUNE` (17)

ABI:

- `a0`: lattice capability slot.
- `a1`: new crystal index.

Required invariants:

- Capability must exist and be `CAP_TYPE_LATTICE`.
- Caller must hold `CAP_RIGHT_WRITE` on the lattice capability.
- Crystal index must be within lattice page bounds.

## `SYS_LATTICE_DETACH` (19)

ABI:

- `a0`: lattice capability slot.
- `a1`: previously attached base virtual address.

Required invariants:

- Capability must exist and be `CAP_TYPE_LATTICE`.
- Attachment must match both lattice identity and base virtual address.
- On success, process attachment slot is cleared and a lattice detach Fate record is appended.
