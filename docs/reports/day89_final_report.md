# Epoch III, Day 89: Area 2 — PID-Privilege Removal Closure

**Date:** 2026-07-23
**Status:** CLOSED
**Serial Marker:** `[KERNEL] pid-privilege-removal: PASS`
**Matrix Gate:** `make -C kernel verify_matrix` — 3/3 PASS

---

## 1. Objective

Remove all raw `pid == 1` and `pid != 1` privilege checks from kernel syscall handlers and replace them with explicit, transferable capability checks. This makes privilege purely a function of which capabilities a process holds, not which PID it was assigned at boot — a core Reaper-OS security invariant.

Two syscall handlers were affected:

| Handler | Old Gate | New Gate |
|---|---|---|
| `SYS_MODE_QUERY` | `pid == 1` → return real mode | `CAP_TYPE_REALITY_CTRL` + `CAP_RIGHT_READ` at caller-supplied slot |
| `SYS_SCHED_AUTH_ROOT_MINT` | `pid != 1` → reject | `CAP_TYPE_REALITY_CTRL` + `CAP_RIGHT_GRANT` at caller-supplied slot |

---

## 2. Changes

### 2.1 `process_has_capability_at` Helper

**Files:** `kernel/include/capability.h`, `kernel/capability.c`

Added a direct O(1) slot-lookup helper:

```c
bool process_has_capability_at(const void *proc_ptr,
                                uint32_t slot,
                                cap_type_t expected_type,
                                uint16_t required_rights);
```

This function:

1. Resolves the process's CNode and performs `cap_lookup(cnode, slot)`.
2. Verifies the identity is non-NULL and not expired (epoch alive).
3. Checks `identity->type == expected_type`.
4. Verifies the current mode is within the capability's allowed mode mask.
5. Confirms `(identity->rights & required_rights) == required_rights`.

The function returns `true` only if all five conditions hold. Any failing condition returns `false` without logging — fail-closed by default.

Rationale for direct slot lookup (O(1)) over scanning all slots: scanning all slots would accept accidentally-present or revoked capabilities that happen to match the type — incorrect for security. The caller must declare which slot holds the authority; the kernel validates that slot only. This mirrors POSIX capability-set discipline applied to capability slots.

### 2.2 `sys_mode_query_handler`

**File:** `kernel/syscall.c`

```c
uint64_t sys_mode_query_handler(process_t *caller, uint64_t reality_ctrl_slot) {
    if (reality_ctrl_slot == 0) {
        return (uint64_t)MODE_CASUAL;
    }
    if (process_has_capability_at(caller, (uint32_t)reality_ctrl_slot,
                                  CAP_TYPE_REALITY_CTRL, CAP_RIGHT_READ)) {
        return (uint64_t)mode_get_current();
    }
    return (uint64_t)MODE_CASUAL;
}
```

- `a0` carries `reality_ctrl_slot`.
- Slot 0 is treated as "no capability provided" — `MODE_CASUAL` returned unconditionally.
- A valid `CAP_TYPE_REALITY_CTRL` with `CAP_RIGHT_READ` returns the real system mode via `mode_get_current()`.
- Any process without such a capability sees `MODE_CASUAL`.

### 2.3 `sys_sched_auth_root_mint_handler`

**File:** `kernel/syscall.c`

```c
uint64_t sys_sched_auth_root_mint_handler(process_t *caller,
                                           uint32_t reality_ctrl_slot,
                                           uint32_t dst_slot,
                                           uint64_t max_total_budget,
                                           uint64_t period_ns) {
    if (reality_ctrl_slot == 0) {
        return (uint64_t)-1;
    }
    if (!process_has_capability_at(caller, reality_ctrl_slot,
                                   CAP_TYPE_REALITY_CTRL, CAP_RIGHT_GRANT)) {
        return (uint64_t)-1;
    }
    uint64_t max_accumulated = max_total_budget * SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER;
    return scheduler_mint_root_auth(caller, dst_slot, max_total_budget,
                                    max_accumulated, period_ns);
}
```

- `a0` = `reality_ctrl_slot`, `a1` = `dst_slot`, `a2` = `max_total_budget`, `a3` = `period_ns`.
- The gate requires `CAP_TYPE_REALITY_CTRL` with `CAP_RIGHT_GRANT`. Only a process that can delegate reality control authority can mint root scheduler authority — consistent with the authority hierarchy.
- `CAP_RIGHT_GRANT` (not `CAP_RIGHT_INVOKE`) was chosen because minting a root scheduling token is a delegation act, not a self-invocation.

### 2.4 `SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER`

**File:** `kernel/syscall.c`

```c
/*
 * SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER
 *
 * Determines the bounded burst window for a root scheduling authority token.
 * The maximum accumulated budget is: max_total_budget * this multiplier.
 *
 * Policy rationale: a 4x multiplier allows up to four full-period budgets to
 * accumulate before the ceiling is reached. This prevents unbounded token
 * hoarding while permitting short bursts after idle periods. The value of 4
 * was chosen to match the historical a3 parameter range used during initial
 * ESAK scheduler design (Day 54) and is now a named constant rather than a
 * silently hardcoded expression.
 *
 * To change the burst policy, update this constant and regenerate the
 * scheduler self-test evidence.
 */
#define SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER 4ULL
```

Previously `max_accumulated` was passed as `a3` from userspace. The named constant documents the policy explicitly and makes any future policy change a visible, auditable source change.

### 2.5 Initial Capability Injection — `genesis_bridge_spawn`

**Files:** `kernel/include/genesis.h`, `kernel/genesis.c`

Added `reality_ctrl_slot` field to `genesis_initial_caps_t`:

```c
typedef struct {
    uint32_t genesis_cap_slot;       /* slot for CAP_TYPE_GENESIS; 0 = skip */
    uint32_t reality_ctrl_slot;      /* slot for CAP_TYPE_REALITY_CTRL; 0 = skip */
    uint32_t auditor_cap_slot;
    uint32_t pml4_cap_slot;
    uint32_t lattice_cap_slot;
    uint32_t sched_auth_cap_slot;
    uint32_t sched_root_auth_cap_slot;
} genesis_initial_caps_t;
```

In `genesis_bridge_spawn` (primordial Paradigm boot path):

```c
.reality_ctrl_slot = 7,   /* Paradigm receives CAP_TYPE_REALITY_CTRL */
```

In `GENESIS_OP_SPAWN` (daemon spawn path):

```c
.reality_ctrl_slot = 0,   /* Child daemons do NOT inherit reality control */
```

**Bootstrap resolution:** Genesis injects `CAP_TYPE_REALITY_CTRL` at slot 7 into Paradigm's CNode during `genesis_inject_initial_caps`, before Paradigm executes any syscall. No circular dependency exists. Child daemons spawned via `GENESIS_OP_SPAWN` receive `reality_ctrl_slot = 0` — injection is skipped. They must be explicitly delegated a `CAP_TYPE_REALITY_CTRL` via `GENESIS_OP_DELEGATE` to gain mode-query or root-mint authority.

### 2.6 Self-Test — `test_pid_privilege_removal`

**File:** `kernel/main.c`

The self-test calls handler functions directly (no SYSCALL instruction), using dynamically assigned PIDs (not forced to specific values). Two test processes are created: one authorized (has `CAP_TYPE_REALITY_CTRL` in its CNode at slot 5), one unauthorized (slot 5 empty).

**Positive path — `sys_mode_query_handler`:**
- A `CAP_TYPE_REALITY_CTRL | CAP_RIGHT_READ` capability is inserted at slot 5 of the authorized process.
- Handler returns `mode_get_current()` — verified.

**Negative path — `sys_mode_query_handler`:**
- Unauthorized process calls handler with `reality_ctrl_slot = 5` (slot empty).
- Handler returns `MODE_CASUAL` — verified.

**Positive path — `sys_sched_auth_root_mint_handler`:**
- Authorized process calls handler with `reality_ctrl_slot = 5`, `dst_slot = 10`.
- Capability gate passes; handler reaches `scheduler_mint_root_auth`.

**Negative path — `sys_sched_auth_root_mint_handler`:**
- Unauthorized process calls with `reality_ctrl_slot = 5` (slot empty).
- Handler returns `(uint64_t)-1` at the capability gate — verified.

**Cleanup:** Both test processes destroyed via `process_destroy()` after the test.

**Serial emission:**
```
[KERNEL] pid-privilege-removal: PASS
```

---

## 3. Files Changed

| File | Change |
|---|---|
| `kernel/include/capability.h` | Added `process_has_capability_at` declaration |
| `kernel/capability.c` | Implemented `process_has_capability_at` |
| `kernel/include/genesis.h` | Added `reality_ctrl_slot` field to `genesis_initial_caps_t` |
| `kernel/genesis.c` | Injected `CAP_TYPE_REALITY_CTRL` at slot 7 in `genesis_bridge_spawn`; set `reality_ctrl_slot = 0` in `GENESIS_OP_SPAWN` |
| `kernel/include/syscall.h` | Declared `sys_mode_query_handler` and `sys_sched_auth_root_mint_handler` |
| `kernel/syscall.c` | Defined `SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER`; implemented handler functions; updated dispatcher |
| `kernel/main.c` | Added `test_pid_privilege_removal()` and invoked from `kernel_main()` |

---

## 4. Verification Evidence

### 4.1 Serial Marker

```
[KERNEL] pid-privilege-removal: PASS
```

Forbidden marker absent:

```
[KERNEL] pid-privilege-removal: FAIL
```

### 4.2 Matrix Gate — 3/3 PASS

```
make -C kernel verify_matrix
```

```
[MATRIX] run 1 PASS
[MATRIX] run 2 PASS
[MATRIX] run 3 PASS
```

All three runs produced the `pid-privilege-removal: PASS` marker. No required marker was absent and no forbidden marker was present across any run.

---

## 5. Security Properties After Closure

| Property | Before Area 2 | After Area 2 |
|---|---|---|
| Mode query privilege | Hardcoded `pid == 1` | `CAP_TYPE_REALITY_CTRL` + `CAP_RIGHT_READ` at declared slot |
| Root scheduler minting | Hardcoded `pid != 1` reject | `CAP_TYPE_REALITY_CTRL` + `CAP_RIGHT_GRANT` at declared slot |
| Capability check cost | O(1) PID compare | O(1) direct slot lookup via `cap_lookup` |
| Delegability | No — only PID 1 qualifies | Yes — any process holding the capability qualifies |
| Revocability | No — cannot revoke a PID | Yes — revoke the capability or delete the slot |
| Audit surface | No capability event | Capability access visible in Fate strings |

---

## 6. Pitfalls Addressed

| Pitfall | Mitigation |
|---|---|
| Slot-scan accepting revoked/accidentally present caps | Direct O(1) slot lookup — caller must declare the authoritative slot |
| Silent hardcode for `max_accumulated` | `SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER` named constant with policy documentation |
| Bootstrap chicken-and-egg for `SYS_SCHED_AUTH_ROOT_MINT` | Genesis injects `CAP_TYPE_REALITY_CTRL` at slot 7 before any syscall runs |
| Child daemons inheriting reality control | `reality_ctrl_slot = 0` sentinel in `GENESIS_OP_SPAWN` |
| Self-test forcing specific PIDs | PIDs assigned dynamically; handlers called directly |

---

## 7. Open Items

None. All acceptance criteria met:

- [x] `process_has_capability_at` implemented and used.
- [x] `sys_mode_query_handler` replaces `pid == 1` with capability gate.
- [x] `sys_sched_auth_root_mint_handler` replaces `pid != 1` with capability gate.
- [x] `SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER` named and documented.
- [x] `reality_ctrl_slot = 7` injected at primordial boot.
- [x] `reality_ctrl_slot = 0` sentinel preserved in spawn path.
- [x] Self-test covers positive and negative paths for both handlers.
- [x] Serial marker `[KERNEL] pid-privilege-removal: PASS` confirmed.
- [x] `make -C kernel verify_matrix` 3/3 PASS confirmed.
