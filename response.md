# Bootinfo v2 — Phase 1 Header Design

This document is the design for the new Bootinfo v2 ABI headers. It is the
result of four rounds of review; the design choices below are the ones that
survived. Each section ends with a brief justification so a future reader
can see why a decision was made, not just what the decision is.

The deliverable for Phase 1 is **one** header (`shared/include/bootinfo.h`),
one kernel verify file, and one userspace verify file. There is no kernel-side
`bootinfo.h`. The kernel compiles with `-Ishared/include` and `#include`s the
single canonical header directly.

---

## 1. The complete `bootinfo_t` struct definition

### Layout strategy

The v1 struct occupies exactly **128 bytes** (8 × 8 = 64 bytes of named
fields, plus 64 bytes of `reserved[8]`). The first rule of v2 is that
**all eight current `uint64_t` fields stay at their existing offsets 0–63,
and the 64 bytes of `reserved[8]` stay where they are.** New v2 content
is layered on top of and after the reserved block. The reserved block is
repurposed — it is renamed in v2 documentation, not freed — so the v2
layout is a clean superset of v1.

### Why repurpose `reserved[8]` rather than only append

`reserved[8]` is 64 bytes of zeros today. Repurposing it as named v2
fields gives us room for the flags word, the module count, and the
framebuffer metadata without growing the struct at all. A v1 reader
still sees `reserved[8]` and the 64 bytes happen to be meaningful to a
v2 reader — the v1 reader ignores them. The reserved array is renamed
in v2 documentation to a small inline v2 prelude; the **module table
and the integrity hash are appended after offset 128.**

The struct layout already encodes the union semantics that "treat this
64-byte region as either `reserved[8]` (v1 view) or named fields (v2
view)" implies. A separate C `union` is not needed and would not help —
both views agree on the byte layout; only the field names differ.

### Concrete struct definition

```c
/* shared/include/bootinfo.h — single canonical ABI definition.
 *
 * For a v1 reader, the 64 bytes at offsets 64..127 are reserved[8].
 * For a v2 reader, those same 64 bytes are the v2 prelude below.
 * The byte layout is identical; only the interpretation differs.
 */
typedef struct boot_info {
    /* ----- v1 fields (offsets 0..63, byte-for-byte preserved) ----- */
    uint64_t magic;                 /* offset   0 — BOOTINFO_MAGIC */
    uint64_t version;               /* offset   8 — BOOTINFO_VERSION (now 2) */
    uint64_t hhdm_offset;           /* offset  16 */
    uint64_t genesis_cap_slot;      /* offset  24 */
    uint64_t memmap_base;           /* offset  32 */
    uint64_t memmap_count;          /* offset  40 */
    uint64_t kernel_phys_start;     /* offset  48 */
    uint64_t kernel_phys_end;       /* offset  56 */

    /* ----- v2 prelude (offsets 64..127). A v1 reader sees this as
     * uint64_t reserved[8] and ignores it. A v2 reader interprets
     * the words as named fields.                                    */
    uint64_t flags;                 /* offset  64 — BOOTINFO_FLAG_* */
    uint32_t module_count;          /* offset  72 — populated entries
                                                in modules[0..16) */
    uint32_t reserved_v2_0;         /* offset  76 — pad to 8 */
    uint64_t fb_base;               /* offset  80 — physical base */
    uint32_t fb_width;              /* offset  88 — pixels */
    uint32_t fb_height;             /* offset  92 — pixels */
    uint32_t fb_pitch;              /* offset  96 — bytes per scanline */
    uint32_t fb_pixel_format;       /* offset 100 — BOOTINFO_PIXEL_* */
    uint32_t reserved_v2_1;         /* offset 104 — pad to 8 */
    uint64_t reserved_v2_2;         /* offset 112 — pad to 128 */

    /* ----- v2-only fields (offsets 128+) -----
     * The integrity field is intentionally NOT at the end of the
     * struct. See "Integrity hash placement" below for the full
     * rationale; the short version is that placing the hash at a
     * fixed offset and defining its scope to be the entire struct
     * (with the hash bytes replaced by zero) means the hash never
     * has to move when v3 adds fields. */
    uint64_t integrity_hash[4];     /* offset 128 — BOOTINFO_INTEGRITY_SIZE
                                                bytes = 32; BLAKE3 of the
                                                struct with these 32 bytes
                                                treated as zero input. */
    boot_module_t modules[BOOTINFO_MODULE_COUNT]; /* offset 160 — module table */

} boot_info_t;
```

### Integrity hash placement (reverted from a prior draft)

A previous draft of this document placed `integrity_hash` at the end of
the struct so the hashing rule could be "hash everything before the
hash." That placement was rejected after review. The rule is now the
more durable one:

> The integrity hash covers the entire struct, with the 32 bytes of
> `integrity_hash` itself replaced by zero before the BLAKE3 call.

With this rule, the hash field is just another field. Its position in
the struct is uninteresting. v3 can add fields anywhere — before the
hash, after the hash, interleaved with it — and the hash still covers
the whole struct without the hash having to move. The implementation
cost is one `memset` (or a temporary copy with the 32 hash bytes
zeroed) per computation. The benefit is that the integrity field's
offset is the only invariant the v3 ABI must preserve, and v3 is free
to evolve the rest of the struct as needed.

### Exact field offsets and total size

| Offset | Field                  | Size (bytes) | Notes                          |
|-------:|------------------------|-------------:|--------------------------------|
|   0    | `magic`                |  8           | v1, unchanged                  |
|   8    | `version`              |  8           | v1, now 2                      |
|  16    | `hhdm_offset`          |  8           | v1, unchanged                  |
|  24    | `genesis_cap_slot`     |  8           | v1, unchanged                  |
|  32    | `memmap_base`          |  8           | v1, unchanged                  |
|  40    | `memmap_count`         |  8           | v1, unchanged                  |
|  48    | `kernel_phys_start`    |  8           | v1, unchanged                  |
|  56    | `kernel_phys_end`      |  8           | v1, unchanged                  |
|  64    | `flags`                |  8           | reused v1 reserved[0]          |
|  72    | `module_count`         |  4           | reused v1 reserved[1] (low)    |
|  76    | `reserved_v2_0`        |  4           | reused v1 reserved[1] (high)   |
|  80    | `fb_base`              |  8           | reused v1 reserved[2]          |
|  88    | `fb_width`             |  4           | reused v1 reserved[3] (low)    |
|  92    | `fb_height`            |  4           | reused v1 reserved[3] (high)   |
|  96    | `fb_pitch`             |  4           | reused v1 reserved[4] (low)    |
| 100    | `fb_pixel_format`      |  4           | reused v1 reserved[4] (high)   |
| 104    | `reserved_v2_1`        |  4           | reused v1 reserved[5] (low)    |
| 108    | `reserved_v2_2` (low)  |  4           | reused v1 reserved[5] (high)   |
| 112    | `reserved_v2_2` (high) |  4           | reused v1 reserved[6] (low)    |
| 116    | — (padding)            |  4           | reused v1 reserved[6] (high)   |
| 120    | — (padding)            |  8           | reused v1 reserved[7]          |
| 128    | `integrity_hash[4]`    | 32           | new in v2                      |
| 160    | `modules[16]`          | 16 × sizeof(boot_module_t) | new in v2         |

The struct total size is **`160 + (BOOTINFO_MODULE_COUNT × sizeof(boot_module_t))`**.
With `BOOTINFO_MODULE_COUNT = 16` and `sizeof(boot_module_t) = 80` (see §2),
the total is **160 + 1280 = 1440 bytes**.

> Note: `integrity_hash[4]` and `modules[16]` use explicit array sizes so
> that `offsetof` and `sizeof` are deterministic across compilers. The
> module table starts at offset 160 because the integrity hash is a
> fixed 32 bytes; that is a property of the integrity field's size, not
> of its position, and would not change if v3 inserted fields after
> `integrity_hash` but before `modules`.

---

## 2. The `boot_module_t` struct definition

### Design constraints

- Naturally aligned, no `packed`
- Must include: index, phys base, size, ELF entry, type, flags, content hash
- BLAKE3 content hash is 32 bytes (matches `integrity_hash` shape)
- **No name field.** Names are diagnostic data only and do not belong
  in the kernel ABI. If debugging later needs names, they live in a
  separate debug manifest or build artifact, not here.
- 16-byte aligned so the table is friendly to common cache line sizes
  without padding the struct out to a round 128 bytes for aesthetic
  reasons

### Concrete struct definition

```c
typedef struct boot_module {
    uint64_t phys_base;              /* offset  0 — physical base address */
    uint64_t size;                   /* offset  8 — size in bytes */
    uint64_t entry;                  /* offset 16 — ELF entry, 0 if N/A */
    uint8_t  content_hash[32];       /* offset 24 — BLAKE3 of module bytes */
    uint32_t index;                  /* offset 56 — module index, 0..15 */
    uint32_t type;                   /* offset 60 — BOOT_MODULE_TYPE_* */
    uint32_t flags;                  /* offset 64 — BOOT_MODULE_FLAG_* */
    uint32_t reserved;               /* offset 68 — explicit, future use */
    /* total: 72 bytes; tail-padded to 80 for 16-byte alignment */
} boot_module_t;
```

### Why 80 bytes, not 72 or 128

- **72 bytes** is the natural size with all fields declared. 16 × 72 =
  1152 bytes for the table.
- **80 bytes** adds 8 bytes of explicit named `reserved` padding to
  bring each entry to a 16-byte boundary. 16 × 80 = 1280 bytes. This
  is a real micro-architectural reason (alignment to 16 means each
  entry starts on a cache-line-friendly boundary on most x86 CPUs) and
  is not aesthetic.
- **128 bytes** is what an earlier draft used. That was rejected:
  padding to 128 wastes 768 bytes of the table for no architectural
  benefit. The 16-byte alignment is the right ceiling.

### Offsets

| Offset | Field          | Size | Type    |
|-------:|----------------|-----:|---------|
|   0    | `phys_base`    |  8   | uint64  |
|   8    | `size`         |  8   | uint64  |
|  16    | `entry`        |  8   | uint64  |
|  24    | `content_hash` | 32   | uint8[32] |
|  56    | `index`        |  4   | uint32  |
|  60    | `type`         |  4   | uint32  |
|  64    | `flags`        |  4   | uint32  |
|  68    | `reserved`     |  4   | uint32  |
|  72    | (tail padding) |  8   | —       |
| **80 total** |

`sizeof(boot_module_t)` = **80 bytes**. The module table is `16 × 80 =
1280` bytes, starting at bootinfo offset **160** and ending at **1440**.
The full bootinfo struct is **1440 bytes**.

---

## 3. Constants and enums

All of the following go in `shared/include/bootinfo.h` — the only
header. Constants are `#define` (matching the existing style of using
`0x5245415045524F53`).

### Existing constants (unchanged value, version changes)

```c
#define BOOTINFO_MAGIC    0x5245415045524F53ULL  /* "REAPEROS" */
#define BOOTINFO_VERSION  2                     /* was 1 */
```

### Module table sizing

```c
#define BOOTINFO_MODULE_COUNT  16
```

There is no `BOOT_MODULE_NAME_MAX`; the name field is gone.

### Module types

```c
#define BOOT_MODULE_TYPE_NONE        0u
#define BOOT_MODULE_TYPE_PARADIGM    1u   /* userspace daemon image */
#define BOOT_MODULE_TYPE_GENESIS     2u   /* bootstrap userspace image */
#define BOOT_MODULE_TYPE_DATA        3u   /* opaque data blob */
#define BOOT_MODULE_TYPE_RAMDISK     4u   /* initial root filesystem */
#define BOOT_MODULE_TYPE_FONT        5u   /* font asset for recovery UI */
```

The list is open-ended. New types are added at the end;
`BOOT_MODULE_TYPE_NONE = 0` is reserved for "slot is empty" so a v1
user iterating the table sees sensible defaults.

### Module flags

```c
#define BOOT_MODULE_FLAG_NONE         0u
#define BOOT_MODULE_FLAG_EXECUTABLE   (1u << 0)  /* is an ELF */
#define BOOT_MODULE_FLAG_REQUIRED     (1u << 1)  /* boot fails if integrity
                                                    check fails */
#define BOOT_MODULE_FLAG_TRUSTED      (1u << 2)  /* content_hash is signed
                                                    by build key (future) */
#define BOOT_MODULE_FLAG_LOADED       (1u << 3)  /* kernel has loaded it */
```

### Bootinfo flags (`bootinfo.flags`)

```c
#define BOOTINFO_FLAG_NONE            0u
#define BOOTINFO_FLAG_FRAMEBUFFER     (1u << 0)  /* framebuffer metadata is
                                                    present and valid */
#define BOOTINFO_FLAG_MODULE_TABLE    (1u << 1)  /* module_count > 0 */
```

There is intentionally no `BOOTINFO_FLAG_INTEGRITY_OK`. The integrity
field's presence and validity is communicated by the all-zero hash
sentinel (see "Integrity hash sentinel semantics" below), not by a
flag bit. One source of truth per fact.

### Pixel formats

```c
#define BOOTINFO_PIXEL_FORMAT_UNKNOWN  0u
#define BOOTINFO_PIXEL_FORMAT_RGB888   1u   /* packed 24 bpp, RGB byte order */
#define BOOTINFO_PIXEL_FORMAT_BGR888   2u   /* packed 24 bpp, BGR byte order */
#define BOOTINFO_PIXEL_FORMAT_RGBA8888 3u   /* 32 bpp */
#define BOOTINFO_PIXEL_FORMAT_BGRA8888 4u   /* 32 bpp */
#define BOOTINFO_PIXEL_FORMAT_XRGB8888 5u   /* 32 bpp, X ignored */
```

### Integrity

```c
#define BOOTINFO_INTEGRITY_SIZE  32   /* BLAKE3 output, 4 × uint64 */
```

### Integrity hash scope (formal)

```
Hash = BLAKE3(
    bytes of boot_info_t [0, sizeof(boot_info_t))
    with bytes [offsetof(boot_info_t, integrity_hash),
                offsetof(boot_info_t, integrity_hash) + 32)
    replaced by 32 zero bytes
)
```

Equivalently, the implementation may copy the struct to a temporary
buffer, `memset` the 32 hash bytes to zero, and call BLAKE3 on the
temporary. The kernel sets `integrity_hash` to the output *after*
computing it. Userspace recomputes the same way to verify.

The hash field is **not** required to be at any particular offset.
v3 may add fields after `integrity_hash`; the hash still covers them
because the scope is the whole struct. The only invariant is that
`integrity_hash` is exactly 32 bytes and its offset is stable across
a given ABI version.

### Integrity hash sentinel semantics (rigorously specified)

The value of `integrity_hash` is reserved to mean exactly one of two
things:

1. **All-zero (`{0, 0, 0, 0}`).** Sentinel value meaning "the kernel
   could not or did not compute a hash for this bootinfo." Userspace
   must treat this as fatal: refuse to trust any v2-only field
   (`flags`, `module_count`, framebuffer, integrity, modules) and
   halt with a clear "bootinfo integrity unavailable" message.
2. **Anything else.** A BLAKE3 output produced by the algorithm above.
   Userspace must recompute and compare; the bootinfo is trusted only
   if the comparison matches.

The all-zero value is reserved for the sentinel meaning and **must
never be assigned any other meaning** in this ABI version or any
forward-compatible successor. BLAKE3 outputs are effectively never
all-zero for any non-trivial input, so a legitimate hash collision
with the sentinel is not a practical concern; if it ever became a
concern, v3 would introduce an explicit `HASH_PRESENT` flag and
re-define the sentinel, not re-use the all-zero value.

This is the only place the all-zero value is mentioned. It is not a
general-purpose "absent" sentinel for any other field.

---

## 4. Layout verification plan

The brief is explicit: **no static assertions in the shared header.**
They live in dedicated verification files that are compiled as part
of the kernel build and the userspace lib build, but not in the header
itself.

### Where the asserts live

- `kernel/verify_bootinfo.c` — compiled into the kernel self-test path
- `shared/verify_bootinfo.c` — compiled into the userspace test build
  (and re-linked into `user/lib/` if a test build is enabled)

Both files `#include "bootinfo.h"` from the **same** path
(`shared/include/bootinfo.h`). The kernel builds with
`-Ishared/include` so its `#include "bootinfo.h"` resolves to the
shared header directly. There is no kernel-side `bootinfo.h`.

### What is asserted

```c
/* struct shape */
_Static_assert(sizeof(boot_info_t)   == 1440, "boot_info_t size");
_Static_assert(sizeof(boot_module_t) == 80,   "boot_module_t size");
_Static_assert(alignof(boot_info_t)   == 8,   "boot_info_t alignment");
_Static_assert(alignof(boot_module_t) == 16,  "boot_module_t alignment");

/* v1 field offsets preserved */
_Static_assert(offsetof(boot_info_t, magic)              == 0,   "magic offset");
_Static_assert(offsetof(boot_info_t, version)            == 8,   "version offset");
_Static_assert(offsetof(boot_info_t, hhdm_offset)        == 16,  "hhdm_offset");
_Static_assert(offsetof(boot_info_t, genesis_cap_slot)   == 24,  "genesis_cap_slot");
_Static_assert(offsetof(boot_info_t, memmap_base)        == 32,  "memmap_base");
_Static_assert(offsetof(boot_info_t, memmap_count)       == 40,  "memmap_count");
_Static_assert(offsetof(boot_info_t, kernel_phys_start)  == 48,  "kernel_phys_start");
_Static_assert(offsetof(boot_info_t, kernel_phys_end)    == 56,  "kernel_phys_end");

/* v2 prelude offsets (must match what v1 calls reserved[0..7]) */
_Static_assert(offsetof(boot_info_t, flags)              == 64,  "flags");
_Static_assert(offsetof(boot_info_t, module_count)       == 72,  "module_count");
_Static_assert(offsetof(boot_info_t, fb_base)            == 80,  "fb_base");
_Static_assert(offsetof(boot_info_t, fb_width)           == 88,  "fb_width");
_Static_assert(offsetof(boot_info_t, fb_height)          == 92,  "fb_height");
_Static_assert(offsetof(boot_info_t, fb_pitch)           == 96,  "fb_pitch");
_Static_assert(offsetof(boot_info_t, fb_pixel_format)    == 100, "fb_pixel_format");

/* v2-only fields */
_Static_assert(offsetof(boot_info_t, integrity_hash)     == 128, "integrity_hash");
_Static_assert(offsetof(boot_info_t, modules)            == 160, "modules");
_Static_assert(sizeof(((boot_info_t*)0)->integrity_hash)
               == BOOTINFO_INTEGRITY_SIZE, "integrity_hash size");

/* module entry shape */
_Static_assert(offsetof(boot_module_t, phys_base)    == 0,   "mod phys_base");
_Static_assert(offsetof(boot_module_t, size)         == 8,   "mod size");
_Static_assert(offsetof(boot_module_t, entry)        == 16,  "mod entry");
_Static_assert(offsetof(boot_module_t, content_hash) == 24,  "mod content_hash");
_Static_assert(offsetof(boot_module_t, index)        == 56,  "mod index");
_Static_assert(offsetof(boot_module_t, type)         == 60,  "mod type");
_Static_assert(offsetof(boot_module_t, flags)        == 64,  "mod flags");
_Static_assert(offsetof(boot_module_t, reserved)     == 68,  "mod reserved");
_Static_assert(sizeof(boot_module_t) == 80,  "boot_module_t size");
_Static_assert(BOOTINFO_MODULE_COUNT == 16, "module count");

/* constants are sane */
_Static_assert(BOOTINFO_MAGIC   == 0x5245415045524F53ULL, "magic constant");
_Static_assert(BOOTINFO_VERSION == 2,                    "version constant");
```

### How we guarantee there is only one definition of the ABI

There is no kernel-side `bootinfo.h`. The shared header at
`shared/include/bootinfo.h` is the only definition of the struct, the
constants, and the enums. The kernel compiles with `-Ishared/include`
so its `#include "bootinfo.h"` resolves to that single file. The
userspace lib builds the same way. There is no forwarding wrapper, no
diff step, no `make` rule that compares two files. Drift is
structurally impossible because there is only one file to drift.

The verify files on each side are still useful: they catch the case
where someone breaks the struct layout by editing the shared header
without updating the kernel or userspace code that depends on it.
That is a different failure mode from header drift, and the static
asserts catch it on the side that would notice.

---

## 5. ABI compatibility story

### How v1 layout is preserved

- All eight named `uint64_t` fields of v1 remain at offsets 0–63
  with identical types.
- The 64 bytes that v1 called `reserved[8]` remain in place at offsets
  64–127. A v1 reader still indexes them as `reserved[i]` (or simply
  ignores them); a v2 reader interprets them as `flags`,
  `module_count`, `fb_*`, and explicit `reserved_v2_*` padding.
- The v2-only fields (`integrity_hash`, `modules`) start at offset
  128 and grow the struct to 1440 bytes. A v1 reader cannot see them;
  a v2 reader that finds a v1 kernel's struct cannot trust them.

### What happens if someone reads the struct with v1 headers

- The reader sees `magic == BOOTINFO_MAGIC`, `version == 2` (or `1`
  if the kernel is still on v1), and the other six v1 fields at the
  right offsets. Everything works.
- The reader sees `reserved[0..7]` as eight zero-or-garbage words and
  ignores them. If the v2 kernel has written, say, `flags =
  BOOTINFO_FLAG_MODULE_TABLE` into the first reserved slot, the v1
  reader does not care.
- The reader's struct ends at offset 128 and knows nothing about
  `integrity_hash` or `modules`. This is fine — the v1 reader was
  never going to use them.
- **Failure mode:** a v1 reader that does `sizeof(boot_info_t) == 128`
  and asserts on it will break. This is acceptable: the bootinfo
  spec at v2 mandates the size bump, and any v1 code that hardcodes
  the size was already coupled to a specific ABI version.

### What happens if someone reads the struct with v2 headers

- The reader sees all v1 fields at the same offsets, then the v2
  prelude at offsets 64–127, then `integrity_hash[4]` at 128, then
  `modules[16]` at 160.
- The reader must check `version == BOOTINFO_VERSION` (2) before
  trusting the v2-only fields, because the same struct layout may
  also be filled in by an older kernel for a transitional period.
- The reader must check `integrity_hash` against the all-zero
  sentinel first. If the hash is all-zero, the reader treats the
  bootinfo as fatal. Otherwise the reader recomputes BLAKE3 over the
  struct with the 32 hash bytes replaced by zero, and refuses the
  data if the recomputed hash disagrees.
- The reader iterates `modules[0..module_count)`, treating the rest
  as `BOOT_MODULE_TYPE_NONE` slots. `module_count` is the
  authoritative length, not `BOOTINFO_MODULE_COUNT`.

### Edge case: kernel on v1, userspace on v2

A v2 userspace reading a v1 bootinfo would see `version == 1` and
must refuse to use the v2-only fields. The expected behavior is to
panic with a clear "bootinfo v2 required, got v1" message and halt.
This is fail-closed and matches the rest of the kernel.

### Edge case: kernel on v2, userspace on v1

A v1 userspace would see the eight v1 fields and the now-meaningful
`reserved[]` words. It cannot see `integrity_hash` or `modules`. The
v1 userspace continues to function as long as it does not require
the new fields. This is the intended transitional state.

---

## 6. Open questions and trade-offs

### Decisions still uncertain

1. **Module table size of 16.** The brief pins this. Is 16 the right
   number for the long term? Genesis + Paradigm + a few data modules
   (font, ramdisk) is 4–5 entries. 16 leaves headroom but consumes
   1280 bytes of bootinfo. If the constraint is the 0x1000 mapping,
   we should consider 8 (which would give a 16 × 80 = 1280 byte
   table anyway, so 8 entries is strictly more room).
2. **Framebuffer field widths.** `uint32_t` for `width`, `height`,
   `pitch`, `format` reflects their actual semantic range and avoids
   implying 64-bit capabilities. This is settled unless a future
   display requires 64K × 64K resolution, which is not on any roadmap.
3. **Integrity hash scope.** Settled: BLAKE3 over the entire struct
   with the 32 hash bytes treated as zero. v3 may add fields
   anywhere; the hash still covers them.
4. **Module content hash scope.** Should `content_hash` cover the
   raw bytes of the loaded module as they sit in physical memory, or
   the bytes of the file before placement? The former is what
   userspace will see; the latter is what the build system produces.
   Picking "as loaded" is simpler and matches the integrity
   guarantee userspace actually needs.
5. **Where the integrity hash primitive lives.** The brief assumes
   BLAKE3 is available. The kernel already has BLAKE3 code (the audit
   system uses it per `notes.txt` line 300). For userspace, do we
   link the same BLAKE3 implementation, or do we have a smaller
   bootinfo-only hash? Linking the same one is recommended for
   consistency.

### Alternatives considered

- **Append all v2 fields after offset 128, do not repurpose
  `reserved[8]`.** Cleaner from a "do not touch v1" standpoint but
  pushes the integrity hash and module table to higher offsets,
  increasing the total size. Repurposing reserved is a deliberate
  trade: v1 readers that ignore the reserved block do not notice,
  and we save 64 bytes of header.
- **Use a `packed` attribute and pack everything tightly.** The
  brief explicitly forbids this. Natural alignment makes
  `static_assert` offsets simple, and the size cost is acceptable.
- **Pointer-based module table** (`boot_module_t *modules` plus a
  separate `uint64_t modules_phys`). Shrinks bootinfo to under 256
  bytes but forces a second ABI for the table location. Splits the
  contract — bad idea.
- **Including a `__version__` sentinel field in the struct.** Already
  have `version`. No need for a second one.
- **BLAKE3 vs. SHA-256 vs. CRC64 for the integrity field.**
  `notes.txt` line 300 already locks BLAKE3. No decision to make
  here.
- **Forwarding header** (`kernel/include/bootinfo.h` containing
  `#include "../shared/include/bootinfo.h"`). A small v2 convenience
  that creates a drift surface. Rejected; the kernel now compiles
  with `-Ishared/include` and includes the canonical header
  directly.
- **Union for v1/v2 view of `reserved[8]`.** A union wouldn't solve
  anything because the ABI already guarantees that those bytes
  occupy the same offsets; the only difference is the field names.
  Better documentation communicates the intent without adding
  another layer of C syntax that future maintainers would have to
  mentally decode.
- **Hash field at end of struct.** Makes the hashing rule trivial
  ("hash everything before the hash") but ties the hash coverage
  to a v2-specific layout. If v3 adds fields after the hash, the
  hash either stops covering them or the hash has to move. The
  "zeroed in place" rule sidesteps both: the hash field is just
  another field, and v3 can add fields anywhere.
- **`HASH_PRESENT` flag instead of all-zero sentinel.** Cleaner
  separation of "integrity value" from "integrity state" (the hash
  itself vs. whether the hash exists). Not adopted for v2 because
  the all-zero sentinel is workable and the flag would be a third
  source of truth alongside `integrity_hash` and the hash recompute
  check. If v3 introduces measured boot with multiple hash states
  (computed, verified, signed, inherited), an explicit `HASH_PRESENT`
  flag should be added then and the all-zero sentinel's role should
  be re-stated in the v3 ABI doc.

### Things to be careful about

- **The reserved-word reinterpretation must be documented in the
  header** with a comment that explains v1 readers will see
  `reserved[8]` here and v2 readers will see the named fields.
  Without that comment, a future engineer reading the v2 header
  will think `flags`/`module_count`/`fb_*` are part of the v1
  layout and may try to read them from a v1 kernel.
- **Static asserts must be kept out of the shared header.** The
  brief is explicit, and the reason is sound: the shared header is
  included from assembly, from C with `-std=gnu99`, and from
  contexts that may not have `_Static_assert` available. Use the
  verify files.
- **The kernel build must include `-Ishared/include`** so the single
  header is visible. This is a one-line Makefile change but it is
  load-bearing: if it is missing, the kernel build fails with a
  missing-header error, which is the correct failure mode (loud,
  immediate) but worth being explicit about.
- **`BOOT_MODULE_TYPE_NONE` must be zero** so an uninitialized module
  entry is interpretable as "empty slot" without needing to be
  explicitly cleared. This is also why `BOOT_MODULE_TYPE_*` starts
  at 1.
- **The `integrity_hash` field is reserved for BLAKE3 output.** Do
  not store anything else there. Do not use the all-zero value for
  any meaning other than the documented sentinel. If v3 introduces
  additional integrity states, it must define new fields or flags
  rather than re-purposing this one.
- **Do not add a `packed` attribute "just to be safe"** if a future
  field needs to be aligned. If a field genuinely needs unaligned
  access, give it an explicit `uint8_t[N]` storage and a typed
  accessor macro. Never `packed` the whole struct.
- **`BOOTINFO_FLAG_FRAMEBUFFER` clear must mean "no framebuffer
  metadata"**, not "the framebuffer is at address 0." The
  recovery-manager code in Paradigm must check the flag before
  touching `fb_base`.

---

If you approve this design, the Phase 1 deliverable is:

- one shared header: `shared/include/bootinfo.h`
- one kernel verify file: `kernel/verify_bootinfo.c`
- one userspace verify file: `shared/verify_bootinfo.c`
- one Makefile change in `kernel/Makefile` to add `-Ishared/include`
  to the kernel CFLAGS

No code in `kernel/genesis.c` or `user/paradigm/main.c` changes yet —
those are Phase 2/3.
