# Epoch III, Day 90: Area 3 — Reality-Aware Bootinfo v2 Closure

**Date:** 2026-08-16
**Status:** CLOSED
**Areas Covered:** Phase 1 (v2 ABI definition) + Phase 2 (v2 population, integrity, userspace verification)
**Serial Markers:**
  - `[BOOTINFO] v2 validation: PASS`
  - `[USER-LOG] PARADIGM: Bootinfo v2 integrity VERIFIED.`
**Matrix Gate:** `make -C kernel verify_matrix` — 3/3 PASS

---

## 1. Objective

Complete the Area 3 bootinfo evolution by landing a reality-aware Bootinfo v2 ABI and
wiring its population, cryptographic integrity protection, and verification on both the
kernel side and the Paradigm userspace side — while preserving the byte-for-byte-stable
v1 layout contract that the Day 15 Genesis Bridge closure marker depends on.

Area 3 was delivered in two phases:

- **Phase 1** — define the v2 struct layout, `boot_module_t` descriptor, all v2
  constants, and compile-time layout verifiers on both the kernel and userspace sides.
- **Phase 2** — populate the v2 fields in `genesis_bootinfo_init`, compute a BLAKE3
  integrity hash over the whole struct, expose the bootinfo physical frame to the kernel
  self-test, port a self-contained BLAKE3 library into userspace, verify the full hash in
  Paradigm, and restore the kernel-side self-test that emits the `[BOOTINFO] v2
  validation: PASS` marker.

---

## 2. Phase 1 — v2 Header Definition and Compile-Time Verification

### 2.1 v2 struct definition (`shared/include/bootinfo.h` + `kernel/include/bootinfo.h`)

The two headers are kept byte-identical by contract (the kernel-side file is a mirror of
the shared one). The `boot_info_t` struct remains `__attribute__((packed))` so that every
v1 field (magic, version, hhdm_offset, genesis_cap_slot, memmap_addr, memmap_entries,
kernel_start, kernel_end, reserved[8]) stays at its exact v1 byte offset. The v2-only
fields are appended after the v1 view and are invisible to a v1 reader.

v2 fields appended after the v1 124-byte view:

| Field | Offset | Type | Notes |
|---|---|---|---|
| `flags` | 124 | `uint64_t` | `BOOTINFO_FLAG_*` bitmask |
| `module_count` | 132 | `uint32_t` | populated entries in `modules[0..16)` |
| `reserved_v2_0` | 136 | `uint32_t` | explicit reserved |
| `fb_base` | 140 | `uint64_t` | framebuffer physical base |
| `fb_width` | 148 | `uint32_t` | pixels |
| `fb_height` | 152 | `uint32_t` | pixels |
| `fb_pitch` | 156 | `uint32_t` | bytes per scanline |
| `fb_pixel_format` | 160 | `uint32_t` | `BOOTINFO_PIXEL_*` |
| `reserved_v2_1/2/3` | 164/168/172 | `uint32_t` each | explicit reserved |
| `integrity_hash` | 176 | `uint64_t[4]` (32 bytes) | BLAKE3 of the whole struct with hash zeroed |
| `modules[BOOTINFO_MODULE_COUNT]` | 208 | `boot_module_t[16]` | module table |

Total size: 1360 bytes.

### 2.2 `boot_module_t` descriptor (72 bytes, packed)

| Field | Offset | Type |
|---|---|---|
| `phys_base` | 0 | `uint64_t` |
| `size` | 8 | `uint64_t` |
| `entry` | 16 | `uint64_t` |
| `content_hash[32]` | 24 | `uint8_t[32]` |
| `index` | 56 | `uint32_t` |
| `type` | 60 | `uint32_t` |
| `flags` | 64 | `uint32_t` |
| `reserved` | 68 | `uint32_t` |

### 2.3 Constants

- `BOOTINFO_MAGIC 0x5245415053454E47ULL` ("REAPGENG") — unchanged from v1.
- `BOOTINFO_INTEGRITY_SIZE 32` — BLAKE3 output bytes.
- `BOOTINFO_MODULE_COUNT 16` — module table capacity.
- `BOOT_MODULE_TYPE_NONE..FONT` — module type tags; `NONE` is 0 (empty-slot sentinel).
- `BOOT_MODULE_FLAG_NONE..LOADED` — module flag bits.
- `BOOTINFO_FLAG_FRAMEBUFFER`, `BOOTINFO_FLAG_MODULE_TABLE` — v2 bootinfo flags.
- `BOOTINFO_PIXEL_FORMAT_*` — framebuffer pixel format tags.
- `BOOTINFO_VERSION` — bumped from 1 to 2 in Phase 2 (see §4.6).

### 2.4 Static-assert verifiers on both sides

- **`kernel/verify_bootinfo.c`** — uses `_Static_assert` (`-std=gnu11`):
  - `sizeof(boot_info_t) == 1360`, `sizeof(boot_module_t) == 72`.
  - `_Alignof(boot_info_t) == 1` and `_Alignof(boot_module_t) == 1` — **alignof == 1
    guards** that make permanently removing the `packed` attribute a loud build failure
    rather than a silent shift of the v1 byte offsets the Day 15 marker validates.
  - Every named field offset for both structs (v1 view and v2-only fields).
  - `integrity_hash` size == `BOOTINFO_INTEGRITY_SIZE`, `content_hash` size == 32.
  - Constant values for `BOOTINFO_MAGIC`, `BOOTINFO_VERSION`, `BOOTINFO_MODULE_COUNT`,
    `BOOTINFO_INTEGRITY_SIZE`.
  - `BOOT_MODULE_TYPE_NONE == 0` empty-slot invariant.

- **`shared/verify_bootinfo.c`** — uses the C99 typedef-array trick
  (`typedef char verify_X[N ? 1 : -1]`) because the userspace build uses `-std=gnu99`
  which does not provide `_Static_assert`. The checks are byte-for-byte identical to the
  kernel verifier. Wiring this file into the userspace build is described in §4.8.

### 2.5 The 52-byte sizing bug caught by static asserts

During the v2 layout design, a candidate struct computed the module-table start offset
incorrectly (a 52-byte error) which would have pushed `modules[]` to the wrong offset and
broken every field after it. The `_Static_assert(offsetof(boot_info_t, modules) == 208,
...)` guard (and the matching `verify_modules_offset == 208` typedef on the userspace
side) fails the build the moment the prelude sizing drifts, turning a silent ABI
corruption into an immediate, loud build failure. This is the verifier's core value:
it locks the v2 binary contract so future edits cannot silently shift field offsets.

---

## 3. Phase 2 — v2 Population, Integrity, and Verification

### 3.1 v2 field population (`kernel/genesis.c:genesis_bootinfo_init`)

After the existing v1 field writes, the function now populates all v2 fields:

- `bootinfo->flags = BOOTINFO_FLAG_FRAMEBUFFER | BOOTINFO_FLAG_MODULE_TABLE`.
- `bootinfo->module_count` = `module_request.response->module_count`, capped at
  `BOOTINFO_MODULE_COUNT` (16).
- Framebuffer fields read from `framebuffer_request.response->framebuffers[0]` (the same
  Limine response consumed by the ocular subsystem):
  - `fb_base` = `fb->address - bootinfo->hhdm_offset` (physical base, not virtual).
  - `fb_width`, `fb_height` from Limine.
  - `fb_pitch = fb->pitch` — **raw bytes per scanline, no division by 4.** Earlier
    iterations divided by 4, which corrupts the value for any non-32bpp format; the raw
    value is stored and consumers derive pixels-per-row from pitch + pixel format.
  - `fb_pixel_format = BOOTINFO_PIXEL_FORMAT_RGB888`.
- `modules[]` populated for each `i < module_count`:
  - `phys_base = (uint64_t)mod->address - bootinfo->hhdm_offset`.
  - `size = mod->size`.
  - `entry = 0` — document: *"populated by GENESIS_OP_SPAWN at runtime, not at bootinfo
    construction"*; no ELF header parsing is performed during bootinfo population.
  - `type` = module index 0 → `BOOT_MODULE_TYPE_PARADIGM`, all others →
    `BOOT_MODULE_TYPE_DATA`, with a comment documenting the kernel policy.
  - `flags = BOOT_MODULE_FLAG_EXECUTABLE | BOOT_MODULE_FLAG_TRUSTED`.
  - `index = i`.

### 3.2 BLAKE3 integrity hash (`kernel/genesis.c`)

The integrity hash is the last field written. The procedure:

1. `fast_zero(bootinfo->integrity_hash, sizeof(bootinfo->integrity_hash))` — the 32 hash
   bytes are replaced by zero before the hash call (per the ABI semantics documented in
   `bootinfo.h`).
2. `blake3_hasher_init`, `blake3_hasher_update(&hasher, bootinfo,
   sizeof(*bootinfo))`, then `blake3_hasher_finalize(..., (uint8_t*)bootinfo->integrity_hash,
   32)`.

Because the hash covers the whole struct with the hash bytes zeroed, and the hash is then
written back into `integrity_hash`, the stored value is a self-referential BLAKE3 binding
of the struct. The all-zero value is reserved as the "kernel could not compute" sentinel;
a reader who finds all-zero `integrity_hash` must refuse to trust any v2-only field.

### 3.3 `genesis_bootinfo_phys` accessor (`kernel/genesis.c`, `kernel/include/genesis.h`)

`genesis_bootinfo_init` records
`genesis_bootinfo_phys = (uint64_t)bootinfo - bootinfo->hhdm_offset`, exposing the bootinfo
physical frame via `genesis_get_bootinfo_phys()`. This lets the kernel self-test reach the
struct from the kernel's own page table (via the HHDM) rather than from the userspace
virtual address `0x1000`, where the frame is mapped only in Paradigm's address space.

### 3.4 Userspace BLAKE3 library (`user/lib/blake3/`)

BLAKE3 is required on both sides for cryptographic integrity verification. The kernel's
`blake3_impl.h` includes `include/utils.h` (a kernel-only header pulling in `cpu.h`). This
was traced to the `blake3_impl.h` include list; none of the three implementation files
(`blake3.c`, `blake3_dispatch.c`, `blake3_portable.c`) use anything from `utils.h`. The
port therefore copies only the self-contained sources into `user/lib/blake3/` and provides
a **minimal userspace-compatible `blake3_impl.h`** that:

- removes `#include "include/utils.h"` (the only kernel dependency),
- adds `#include <string.h>` (standard `memset`/`memcpy` for the blake3 code),
- removes the `strlen` redefinition (conflicts with `<string.h>`),
- keeps everything else identical: the `blake3_flags` enum, `INLINE` macro,
  `MAX_SIMD_DEGREE*`, the `IV[8]` constant, `MSG_SCHEDULE[7][16]`, all helper functions,
  and all function declarations.

`user/Makefile` gained `-Ilib`, `-fno-stack-protector`, and the three blake3 `.c` files in
`SRCS_C`. `user/lib/string.c` gained a `memcmp` implementation (the freestanding link
previously lacked it).

### 3.5 Paradigm v2 validation + full hash verification (`user/paradigm/main.c`)

Paradigm's bootinfo check now branches on `version == 2`, then validates:

- `BOOTINFO_FLAG_FRAMEBUFFER` set
- `BOOTINFO_FLAG_MODULE_TABLE` set
- `fb_width > 0 && fb_height > 0`
- `module_count > 0`
- **Full BLAKE3 hash verification**: copy the whole struct with `memcpy`, zero the hash
  bytes with `memset`, `blake3_hasher_init`/`update`/`finalize`, then `memcmp` the computed
  hash against the stored `integrity_hash`. Any mismatch is treated as fatal and calls
  `sys_exit(1)`.

On success Paradigm emits:
```
[USER-LOG] PARADIGM: Bootinfo v2 integrity VERIFIED.
```

Earlier iterations fell back to a CRC32 check; that was rejected as non-cryptographic and
replaced with the full BLAKE3 verification using the ported userspace library.

### 3.6 Kernel self-test (`kernel/main.c`)

After `genesis_bridge_spawn()` and `test_genesis_lifecycle()` completes, a self-test reads
the bootinfo via the kernel HHDM:

```c
boot_info_t* bi = (boot_info_t*)pmm_phys_to_virt(genesis_get_bootinfo_phys());
```

and verifies `magic == BOOTINFO_MAGIC`, `version == BOOTINFO_VERSION`, both v2 flags set,
`module_count > 0`, and that `integrity_hash` is not all-zero across all 32 bytes (iterated
as a byte pointer over the `uint64_t[4]` field). On success it emits:
```
[BOOTINFO] v2 validation: PASS
```

### 3.7 `bootinfo->version = 1` hardcode bug fixed to `BOOTINFO_VERSION`

When the v2 ABI was originally authored, `genesis_bootinfo_init` wrote
`bootinfo->version = 1` regardless of the `BOOTINFO_VERSION` constant. After the constant
was bumped to 2, the kernel was still writing `version = 1`, so Paradigm silently took the
v1 fallback path and the `VERIFIED.` marker never appeared. Fix: hardcode replaced with
`bootinfo->version = BOOTINFO_VERSION`, keeping the field in lock-step with the ABI
constant.

### 3.8 Userspace verifier wiring (`user/Makefile`, `user/shared/verify_bootinfo.c`)

- Added `-I../shared/include` so `#include "bootinfo.h"` in the verify file resolves.
- Added `shared/verify_bootinfo.c` to `SRCS_C`; a copy is staged at
  `user/shared/verify_bootinfo.c` for the freestanding build.
- The C99 typedef-array checks in the userspace verifier now assert
  `BOOTINFO_VERSION == 2`, matching the kernel-side `_Static_assert`.

---

## 4. Version Bump

### 4.1 BOOTINFO_VERSION 1 → 2

Changed `#define BOOTINFO_VERSION 1` → `2` in both `kernel/include/bootinfo.h` and
`shared/include/bootinfo.h`. The two verify files were updated in lock-step:

- `kernel/verify_bootinfo.c`: `_Static_assert(BOOTINFO_VERSION == 2, ...)`.
- `shared/verify_bootinfo.c`: `typedef char verify_bootinfo_version
  [(BOOTINFO_VERSION == 2) ? 1 : -1];`

The bump was intentionally deferred to this phase so the Day 15 marker
`[TEST] Day 15 Bootinfo Bridge: SUCCESS.` and the `PARADIGM: Genesis bridge probe PASS.`
conformance marker (which checked `version == 1`) would not break mid-flight. Phase 2
lands the bump together with the userspace-side check update so both stay in sync.

---

## 5. Files Changed

| File | Change |
|---|---|
| `shared/include/bootinfo.h` | v2 struct fields, `boot_module_t`, all v2 constants; `BOOTINFO_VERSION` bumped to 2 |
| `kernel/include/bootinfo.h` | mirror copy; `BOOTINFO_VERSION` bumped to 2 |
| `kernel/verify_bootinfo.c` | `_Static_assert` layout guards; `BOOTINFO_VERSION == 2` |
| `shared/verify_bootinfo.c` | C99 typedef-array layout guards; `BOOTINFO_VERSION == 2` |
| `kernel/genesis.c` | v2 field population; BLAKE3 integrity hash; `bootinfo->version = BOOTINFO_VERSION`; `genesis_bootinfo_phys` assignment; `genesis_get_bootinfo_phys()` |
| `kernel/include/genesis.h` | `genesis_get_bootinfo_phys()` declaration |
| `kernel/main.c` | kernel bootinfo v2 self-test emitting `[BOOTINFO] v2 validation: PASS` |
| `user/lib/blake3/` | ported `blake3.h`, `blake3.c`, `blake3_portable.c`, `blake3_dispatch.c`, minimal `blake3_impl.h` |
| `user/lib/string.c` | added `memcmp` |
| `user/paradigm/main.c` | v2 branch + field validation + BLAKE3 hash verification |
| `user/Makefile` | `-Ilib`, `-I../shared/include`, `-fno-stack-protector`, blake3 `.c` sources, `shared/verify_bootinfo.c` |

---

## 6. Verification Evidence

### 6.1 Serial markers (headless boot)

```
[GENESIS] sys_genesis_invoke: PASS
[BOOTINFO] v2 validation: PASS
...
[USER-LOG] PARADIGM: BootInfo Verified.
[USER-LOG] PARADIGM: Genesis bridge probe PASS.
[USER-LOG] [USER-LOG] PARADIGM: Bootinfo v2 integrity VERIFIED.
```

Forbidden markers absent: `[BOOTINFO] v2 validation: FAIL`, bootinfo-mismatch/magic
failures, and all `[DAY*-FAIL]` markers.

### 6.2 Matrix gate — 3/3 PASS

```
make -C kernel verify_matrix
```

```
[matrix] run 1 PASS
[matrix] run 2 PASS
[matrix] run 3 PASS
[matrix] PASS: all 3 runs met required markers and avoided forbidden markers.
```

---

## 7. Known Limits / Follow-Up

- `fb_pixel_format` is hard-set to `BOOTINFO_PIXEL_FORMAT_RGB888`; deriving it from the
  Limine framebuffer masks (`red_mask_*`, `green_mask_*`, `blue_mask_*`) is deferred.
- `modules[].content_hash` remains all-zero; per-module BLAKE3 content hashing is deferred.
- `modules[].entry` remains 0; per the ABI, it is populated by `GENESIS_OP_SPAWN` at
  runtime, not at bootinfo construction.
- The `user/shared/verify_bootinfo.c` staged copy duplicates the shared-side verifier; a
  future clean-up may reference the shared file directly with an adjusted VPATH instead of
  keeping a staged copy.

---

## 8. Acceptance Criteria

All met:

- [x] v2 header definition, `boot_module_t`, all constants defined in both mirrored headers.
- [x] Static-assert verifiers on both kernel (`_Static_assert`) and userspace
      (C99 typedef-array) sides, with `alignof == 1` guards.
- [x] The 52-byte sizing bug is caught by the `offsetof == 208` module-table guard.
- [x] v2 fields populated in `genesis_bootinfo_init` (flags, module_count, fb_*, modules[]).
- [x] BLAKE3 integrity hash computed last over the whole struct with hash bytes zeroed.
- [x] `genesis_bootinfo_phys` / `genesis_get_bootinfo_phys()` exposed for the kernel self-test.
- [x] Userspace BLAKE3 port in `user/lib/blake3/`, wired into the build.
- [x] Paradigm validates v2 fields and performs full BLAKE3 verification (`VERIFIED.` marker).
- [x] Kernel self-test reads via `pmm_phys_to_virt(genesis_get_bootinfo_phys())` and emits
      `[BOOTINFO] v2 validation: PASS`.
- [x] `bootinfo->version = BOOTINFO_VERSION` fix (no longer hardcoded to 1).
- [x] Both required markers present in the serial log; `verify_matrix` 3/3 PASS.
- [x] Area 3 marked complete in `TODO.rst`, `epoch_three_backlog.md`, and `versions.rst`.
