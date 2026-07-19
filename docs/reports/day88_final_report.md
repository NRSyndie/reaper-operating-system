# Epoch III, Day 88: Area 1 Genesis Syscall Dispatch Closure

**Date:** 2026-07-19  
**Status:** DONE  
**Modules:** `kernel/genesis.c`, `kernel/main.c`, `kernel/capability.c`, `kernel/process.c`, `shared/include/syscall.h`, `shared/include/capability.h`, `kernel/include/genesis.h`, `docs/reports/day88_final_report.md`, `docs/reports.md`, `docs/development_log/TODO.rst`, `docs/development_log/epoch_three_backlog.md`, `docs/development_log/versions.rst`

---

## 1. Scope & Objective

Complete the Area 1 Genesis syscall dispatch closure by implementing all missing
functionality required before the formal closure gate can pass:

1. `GENESIS_OP_DELEGATE` case in `genesis_syscall_dispatch`
2. `GENESIS_OP_SPAWN` userspace copy path (replace stub `return -1`)
3. `GENESIS_OP_SPAWN` caps struct correctness (per-request slot fields, security model)
4. Eight-step self-test exercising DELEGATE positive and negative verification

---

## 2. What Was Implemented

### 2.1 Process Registry (`kernel/process.c`)

`process_find_by_pid`, `process_register_live`, and `process_unregister_live` were
previously restored in Day 88 (initial pass). These functions use a spinlock-protected
fixed-size registry (`MAX_PROCESSES 64`) to track live processes. They are the
foundation for the DELEGATE case: `process_find_by_pid(req.target_pid)` is used to
locate the target process's cspace before performing direct cap injection.

### 2.2 Three New Capability Types (`shared/include/capability.h`)

Added to the `cap_type_t` enum:

| Type | Purpose |
|------|---------|
| `CAP_TYPE_REALITY_CTRL` | Reality control authority — grants the holder authority to manipulate a named Reality (virtual universe) within the Pentabrid layer model. |
| `CAP_TYPE_AUDIT_WRITE` | Audit record emission authority — grants the holder authority to append records to the Fate String audit ring. |
| `CAP_TYPE_SCHED_AUTH` | Broad process-level scheduling authority — authorizes scheduling policy decisions at the process granularity. |

`CAP_TYPE_SCHED_AUTH_ROOT` and `CAP_TYPE_SCHED_AUTH_THREAD` were present from the Day 54
ESAK scheduler landing. Together these four types form the DELEGATE whitelist.

### 2.3 Genesis Exhaustion State (`kernel/capability.c`)

`cap_genesis_is_exhausted()` and `cap_genesis_exhaust()` implement an atomic one-way
flag that permanently closes the Genesis Bridge once `GENESIS_OP_DESTROY` is invoked.
The flag is checked at the top of `genesis_syscall_dispatch` before any operation
proceeds, providing a fail-closed early exit for all post-exhaustion call attempts.

### 2.4 `genesis_syscall_dispatch` — All Three Ops (`kernel/genesis.c`)

#### `genesis_copy_from_user` (new static helper)
Local safe-copy function mirroring `syscall.c`'s `copy_from_user`. Kept in the same
compilation unit to avoid exporting a new kernel ABI symbol. Checks `src < USER_VA_LIMIT`
(0x0000800000000000) before `memcpy`. Used by both SPAWN and DELEGATE for the userspace
copy path.

#### `genesis_is_delegatable_type` (new static whitelist)
Returns `true` only for the four approved cap types:
- `CAP_TYPE_REALITY_CTRL`
- `CAP_TYPE_AUDIT_WRITE`
- `CAP_TYPE_SCHED_AUTH`
- `CAP_TYPE_SCHED_AUTH_ROOT`

All other types fall through to `return (uint64_t)-1`. This is the single authoritative
location for the DELEGATE whitelist. Adding a new delegatable type requires an explicit
code change here, not a default-open policy.

#### `GENESIS_OP_SPAWN` — Three Fixes Applied

| Field | Before | After | Rationale |
|-------|--------|-------|-----------|
| Userspace path | `return (uint64_t)-1` stub | `genesis_copy_from_user` | Enables userspace invocation |
| `genesis_cap_slot` | `1` (hardcoded) | `0` (sentinel) | Spawned daemons must not auto-receive Genesis authority |
| `sched_root_slot` | `5` (hardcoded) | `req.out_sched_root_slot` | Caller specifies target slot |
| `sched_thread_slot` | `6` (hardcoded) | `req.out_sched_thread_slot` | Caller specifies target slot |
| `pagetable_slot` | `req.out_pagetable_slot` | unchanged | Already correct |
| Mode | `MODE_CASUAL` | `owner->mode` | Spawned process inherits caller's security envelope |

> **TODO (Area 3):** `ram_slot = 3` and `audit_slot = 4` remain hardcoded. These will
> be exposed in `genesis_spawn_req_t` when bootinfo v2 lands.

#### `GENESIS_OP_DELEGATE` — New Case

Full implementation with dual kernel/userspace copy paths and whitelist enforcement:

```
1. Copy genesis_delegate_req_t (kernel: memcpy, userspace: genesis_copy_from_user)
2. genesis_is_delegatable_type(req.cap_type) — fail closed if not on whitelist
3. process_find_by_pid(req.target_pid) — locate target process
4. cap_identity_create(req.object_ptr, req.cap_type, req.cap_rights,
                       req.badge, req.allowed_modes)
5. cap_insert(target->cspace, req.target_slot, new_ident)
6. cap_identity_free on insert failure
7. return 0 on success
```

The caller (Paradigm) does NOT need to possess the delegated capability type. This
bypass of normal `cap_mint` possession checks is the architectural point of Genesis
authority. The whitelist is the security boundary.

### 2.5 `genesis_inject_initial_caps` Guard (`kernel/genesis.c`)

Added `if (caps->genesis_cap_slot != 0)` around the Genesis cap injection block.
`genesis_cap_slot = 0` is the sentinel meaning "do not inject a Genesis capability."
Only `genesis_bridge_spawn` (the primordial Paradigm boot) passes `genesis_cap_slot = 1`.
All subsequent spawns via `GENESIS_OP_SPAWN` pass `0`, preventing Genesis authority
leakage to arbitrary daemons.

### 2.6 Eight-Step Self-Test (`kernel/main.c`)

`test_genesis_lifecycle` expanded from 7 steps to 8 steps. Step 3b added between
spawn verification (Step 3) and cleanup (Step 4) while the test process is still alive:

**Step 3b — DELEGATE positive path:**
- Delegates `CAP_TYPE_REALITY_CTRL` (badge `0xDA7A`, object `0xCAFEBABE`) to the
  spawned test process's cspace slot 10.
- Verifies via `cap_lookup(target->cspace, 10)` that the cap is present, has type
  `CAP_TYPE_REALITY_CTRL`, and has badge `0xDA7A`.

**Step 3b — DELEGATE negative path (whitelist rejection):**
- Attempts to delegate `CAP_TYPE_RAM` to slot 11.
- Asserts that `genesis_syscall_dispatch` returns `(uint64_t)-1`.
- Both must pass before the test emits `PASS`.

**Updated `genesis_spawn_req_t` in Step 3:**
Explicitly sets `out_sched_root_slot = 5` and `out_sched_thread_slot = 6`, which are
now consumed by the SPAWN caps struct instead of being hardcoded.

---

## 3. Why It Was Added

- **Security model correctness:** Hardcoding `genesis_cap_slot = 1` in SPAWN gave every
  spawned daemon Genesis authority automatically — defeating the core security model.
  Every daemon should receive only the capabilities Genesis explicitly delegates.
- **Mode inheritance:** Hardcoding `MODE_CASUAL` silently downgraded spawned processes
  from the caller's actual security envelope. `owner->mode` propagates the correct
  context.
- **DELEGATE was the missing primitive:** Without `GENESIS_OP_DELEGATE`, there was no
  mechanism to hand authority (e.g., `CAP_TYPE_AUDIT_WRITE`) directly to a daemon like
  Sentinel without routing through Paradigm's capability list — violating the Pentabrid
  architecture's direct-injection model.
- **Userspace path completeness:** The `copy_from_user` stub gap would have silently
  rejected all real userspace invocations of `SYS_CAP_INVOKE` with Genesis operations,
  making the Genesis Bridge effectively kernel-only at runtime.

---

## 4. Verification Evidence

### Serial Log (headless boot, `kernel/serial_delegate_test.log`)

```
[GENESIS] sys_genesis_invoke: SPAWN PID 9 found in registry.
[GENESIS] sys_genesis_invoke: DELEGATE to PID 9 PASSED.
[GENESIS] sys_genesis_invoke: Post-exhaustion call REJECTED.
[GENESIS] sys_genesis_invoke: PASS
[LAW2_ATTEST] day=28 result=PASS reason=0 strict_map=9 fail_index=2 fail_child=1 fail_flags=2
[LAW2_ATTEST] day=29 result=PASS reason=0 strict_unmap=6 strict_map=9 reason_mask=0x1f unmap_max=747722 unmap_avg=433731 budget=2000000
[LAW2_ATTEST] day=30 result=PASS reason=0 reject_reason_events=9 reason_mask=0x7 scan_cycles=2532508 budget=5000000
```

No `PANIC`, no `KPANIC`, no `[DAY*-FAIL]` forbidden markers.

All existing markers confirmed present:
- `[TEST] Day 8 Gatekeeper redesign: SUCCESS.`
- `[TEST] Day 12 Fault Isolation: SUCCESS.`
- `[TEST] Day 15 Genesis Module Contract: SUCCESS.`
- `[TEST] Day 19 Mode Mask Validation: SUCCESS.`
- `[TEST] Day 22 Recursive Revocation Contract: SUCCESS.`
- `[TEST] Day 24 Foundation Hardening Contract: SUCCESS.`
- `[TEST] Day 25 PCID Partition Contract: SUCCESS.`
- `[TEST] Day 80 Audit Foundation Contract: SUCCESS.`
- *(all other Day 12–34 markers also present)*

### `make -C kernel verify_matrix` (3/3 PASS)

```
[matrix] runs=3 timeout=35s iso=reaper-os.iso
[matrix] run 1/3
[matrix] run 1 PASS (./serial_matrix_run1.log)
[matrix] run 2/3
[matrix] run 2 PASS (./serial_matrix_run2.log)
[matrix] run 3/3
[matrix] run 3 PASS (./serial_matrix_run3.log)
[matrix] PASS: all 3 runs met required markers and avoided forbidden markers.
```

---

## 5. Known Limits / Follow-Up

- `ram_slot = 3` and `audit_slot = 4` remain hardcoded in the SPAWN caps struct.
  These will be exposed as `out_ram_slot` and `out_audit_slot` fields in
  `genesis_spawn_req_t` when bootinfo v2 lands (Area 3).
- The process registry remains a fixed-size array (`MAX_PROCESSES 64`). Larger
  deployments will require a dynamic hash table (deferred post-Epoch III).
- `genesis_copy_from_user` does not walk page tables for mapped verification — it
  performs an address-range check only. Full `vmm_is_user_mapped` integration is
  deferred to the MPK/VMFUNC hardware-backend migration (Area 2).
- Area 1 of the Epoch III backlog is now formally closed, enabling work to proceed
  on Area 2 (Hardware-backend envelope migration: MPK/VMFUNC).
