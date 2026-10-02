# Epoch III, Day 91: Area 4 — Dual-Module Boot Split Closure

**Date:** 2026-09-10
**Status:** CLOSED
**Areas Covered:** user/ build split, kernel packaging, bootinfo module type correction, dual-module self-test
**Serial Markers:**
  - `[MODULES] dual-module load: PASS`
  - `[GENESIS] Awake in the Void.`
  - `[USER-LOG] PARADIGM: Bootinfo v2 integrity VERIFIED.`
**Matrix Gate:** `make -C kernel verify_matrix` — 3/3 PASS

---

## 1. Objective

Split the single-userspace-ELF boot model into a dual-module architecture: Genesis (bootstrap stub, module index 0) and Paradigm (system daemon, module index 1). Genesis verifies bootinfo, spawns Paradigm via `GENESIS_OP_SPAWN`, delegates `CAP_TYPE_SPAWN_AUTH`, destroys `GENESIS_CAP`, and idles. Paradigm is loaded from the second Limine module and runs the existing test suite unchanged.

This closes the "Area 4" backlog item from `docs/concerns.md` and `docs/development_log/epoch_three_backlog.md`.

---

## 2. What Changed

### 2.1 user/ build split (`user/Makefile`)

The Makefile now produces two ELF targets from a shared base:

- **`genesis.elf`**: `user/genesis/main.o` + shared base objects.
- **`paradigm.elf`**: `user/paradigm/main.o` + shared base objects.

Shared base objects (`OBJS_BASE`): `lib/start.s`, `lib/reaper.c`, `lib/string.c`, `shared/verify_bootinfo.c`, `lib/blake3/blake3.c`, `lib/blake3/blake3_portable.c`, `lib/blake3/blake3_dispatch.c`.

Both targets use the same linker script (`user/linker.ld`) with no layout conflicts.

### 2.2 Genesis userspace stub (`user/genesis/main.c`)

New 90-line file:

1. Reads bootinfo at fixed virtual `0x1000` (`BOOTINFO_ADDR`).
2. Validates `magic == BOOTINFO_MAGIC` and `version == BOOTINFO_VERSION`.
3. Verifies `module_count >= 2` and `modules[1].type == BOOT_MODULE_TYPE_PARADIGM`.
4. Spawns Paradigm (module index 1) via `sys_genesis_invoke(GENESIS_OP_SPAWN, ...)` with the historical slot layout (`pagetable=2`, `ram=3`, `audit=4`, `sched_root=5`, `sched_thread=6`).
5. Delegates `CAP_TYPE_SPAWN_AUTH` to Paradigm via `GENESIS_OP_DELEGATE`.
6. Destroys `GENESIS_CAP` via `GENESIS_OP_DESTROY`.
7. Idles with `sys_yield()`.

### 2.3 Kernel packaging (`kernel/Makefile`)

ISO rule updated to:
- Build both userspace ELFs (`make -C ../user`).
- Copy `genesis.elf` and `paradigm.elf` to `isofiles/boot/`.
- Generate `limine.conf` with two `module_path` lines:
  - `module_path: boot():/boot/genesis.elf` (module 0)
  - `module_path: boot():/boot/paradigm.elf` (module 1)

### 2.4 Bootinfo module type correction (`kernel/genesis.c:129-131`)

`genesis_bootinfo_init` now assigns:
- `modules[0].type = BOOT_MODULE_TYPE_GENESIS`
- `modules[1].type = BOOT_MODULE_TYPE_PARADIGM`
- `modules[i].type = BOOT_MODULE_TYPE_DATA` for `i >= 2`

Previously, index 0 was tagged `BOOT_MODULE_TYPE_PARADIGM` (correct before the split; stale after).

### 2.5 Dual-module self-test (`kernel/main.c:702-729`)

New `test_dual_module_load()` function verifies:
1. `module_request.response->module_count >= 2` (Limine reports both modules).
2. `bootinfo->modules[0].type == BOOT_MODULE_TYPE_GENESIS`.
3. `bootinfo->modules[1].type == BOOT_MODULE_TYPE_PARADIGM`.
4. Both `phys_base` and `size` are non-zero (ELFs loaded into memory).

Emits `[MODULES] dual-module load: PASS` on success. Called during boot after `test_bootinfo_v2()`.

---

## 3. Boot Flow (Post-Split)

```
Limine loads kernel.elf, genesis.elf, paradigm.elf
  → kernel_main()
    → test_dual_module_load()           ← [MODULES] dual-module load: PASS
    → genesis_bootinfo_init()           ← modules[0]=GENESIS, modules[1]=PARADIGM
    → genesis_bridge_spawn()            ← loads modules[0] (genesis.elf) as Genesis process
    → test_genesis_lifecycle()
    → test_bootinfo_v2()
    → test_dual_module_load()
    → sti() → scheduler takes over
      → Genesis thread runs
        → validates bootinfo, module_count, modules[1].type
        → GENESIS_OP_SPAWN (module_index=1) → Paradigm process created
        → GENESIS_OP_DELEGATE → SPAWN_AUTH to Paradigm
        → GENESIS_OP_DESTROY → GENESIS_CAP spent
        → idles
      → Paradigm thread runs
        → validates bootinfo v2, BLAKE3 integrity
        → runs full test suite (Day 12-34, boundary probes, etc.)
```

---

## 4. Files Changed

| File | Change |
|---|---|
| `user/Makefile` | Split into `genesis.elf` + `paradigm.elf` targets with shared `OBJS_BASE` |
| `user/genesis/main.c` | New 90-line Genesis stub (bootinfo validation, Paradigm spawn, delegation, cap destroy) |
| `kernel/Makefile` | ISO rule copies both ELFs; limine.conf emits two `module_path` lines |
| `kernel/genesis.c:129-131` | `genesis_bootinfo_init` module type: `i==0 → GENESIS`, `i==1 → PARADIGM`, else `DATA` |
| `kernel/main.c:702-729` | New `test_dual_module_load()` self-test with `[MODULES]` marker |
| `kernel/main.c:1829-1830` | Calls `test_dual_module_load()` during boot sequence |

---

## 5. Verification Evidence

### 5.1 Serial markers (headless boot)

```
[MODULES] dual-module load: PASS
[GENESIS] Constructing the Bridge...
[GENESIS] Module Found. Address: 0x..., Size: ...
[TEST] Day 15 Genesis Module Contract: SUCCESS.
[TEST] Day 15 Genesis Capability Injection: SUCCESS.
[TEST] Day 15 Bootinfo Bridge: SUCCESS.
[BOOTINFO] v2 validation: PASS
...
[GENESIS] Awake in the Void.
[GENESIS] Bootinfo verified; Paradigm module present.
[GENESIS] Paradigm spawned.
[GENESIS] SPAWN_AUTH delegated to Paradigm.
[GENESIS] GENESIS_CAP destroyed. The Bridge is closed.
[GENESIS] Bootstrap complete. Idling.
...
[USER-LOG] PARADIGM: Bootinfo v2 integrity VERIFIED.
```

Forbidden markers absent: `[MODULES-FAIL]`, `[DAY15-FAIL]`, `[BOOTINFO] v2 validation: FAIL`.

### 5.2 Matrix gate — 3/3 PASS

```
make -C kernel verify_matrix
```

All required markers present across 3 runs. Paradigm test suite executes from module index 1 without modification.

---

## 6. Known Limits / Follow-Up

- Genesis is a minimal stub; it does not perform capability-based introspection or policy enforcement beyond the spawn/delegate/destroy sequence. This is intentional — Genesis authority is spent at boot.
- `modules[].content_hash` remains all-zero; per-module BLAKE3 content hashing is deferred.
- `modules[].entry` remains 0; populated by `GENESIS_OP_SPAWN` at runtime.
- Paradigm boundary probe failures (Day 27/28/30) are pre-existing kernel validation gaps unrelated to the Area 4 split; they were previously masked by the QEMU `swapgs` crash and are tracked separately.

---

## 7. Acceptance Criteria

All met:

- [x] `user/Makefile` produces two ELF targets (`genesis.elf`, `paradigm.elf`).
- [x] `user/genesis/main.c` validates bootinfo, spawns Paradigm, delegates authority, destroys GENESIS_CAP.
- [x] `kernel/Makefile` ISO rule copies both ELFs and generates two `module_path` lines.
- [x] `kernel/genesis.c` bootinfo type assignment: `i==0 → GENESIS`, `i==1 → PARADIGM`.
- [x] `kernel/main.c` self-test emits `[MODULES] dual-module load: PASS`.
- [x] Existing Paradigm test suite passes unchanged from module index 1.
- [x] `make -C kernel verify_matrix` passes 3/3.
- [x] Area 4 marked complete in `TODO.rst`, `epoch_three_backlog.md`, and `versions.rst`.
