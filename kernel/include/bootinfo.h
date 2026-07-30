#ifndef REAPER_BOOTINFO_H
#define REAPER_BOOTINFO_H

#include <stdint.h>

/*
 * REAPER-OS BOOT INFORMATION (THE GENESIS BRIDGE) — v2
 *
 * This structure is passed from the Voidborn kernel to the first
 * user-space process (Paradigm) to provide the initial context
 * required to construct reality.
 *
 * ====================================================================
 *  ABI COMPATIBILITY NOTES
 * ====================================================================
 *
 *  This struct intentionally remains __attribute__((packed)) in v2
 *  to preserve the binary layout of the Bootinfo v1 ABI. The Day 15
 *  Bootinfo Bridge closure marker
 *
 *      [TEST] Day 15 Bootinfo Bridge: SUCCESS.
 *
 *  validates the v1 field offsets and types exactly as they appear
 *  below. Removing the packed attribute would shift every v1 field
 *  after `genesis_cap_slot` and break the established contract. A
 *  future major ABI revision may revisit this decision; v2 does not.
 *
 *  All v1 fields (magic, version, hhdm_offset, genesis_cap_slot,
 *  memmap_addr, memmap_entries, kernel_start, kernel_end, reserved[8])
 *  appear first and at the same byte offsets as in v1. The v2-only
 *  fields (flags, module_count, fb_*, integrity_hash, modules) appear
 *  after the v1 view and are not visible to a v1 reader.
 *
 *  BOOTINFO_VERSION remains 1 in Phase 1. The version bump to 2 is
 *  a Phase 2 kernel change that lands together with the kernel-side
 *  population of the v2 fields and the userspace-side update of the
 *  Genesis bridge probe check. A future reader that finds
 *  BOOTINFO_VERSION == 2 must not trust any v2-only field whose
 *  version is < 2; a reader that finds BOOTINFO_VERSION < 2 must
 *  refuse the v2-only fields and halt fail-closed.
 *
 * ====================================================================
 *  INTEGRITY HASH SEMANTICS
 * ====================================================================
 *
 *  `integrity_hash` is a 32-byte BLAKE3 output. Its scope is the
 *  entire struct, with these 32 bytes themselves replaced by zero
 *  before the BLAKE3 call. The hash is "just another field" — its
 *  position in the struct is uninteresting, and v3 may add fields
 *  before, after, or interleaved with it without the hash having
 *  to move.
 *
 *  The all-zero value ({0,0,0,0}) is reserved exclusively as the
 *  "kernel could not compute" sentinel. Userspace must treat an
 *  all-zero integrity_hash as fatal and refuse to trust any v2-only
 *  field. The all-zero value is not assigned any other meaning in
 *  this ABI version or any forward-compatible successor; if a future
 *  revision needs additional integrity states it must introduce new
 *  fields or flags rather than re-purposing this one.
 *
 * ====================================================================
 *  MODULE TABLE
 * ====================================================================
 *
 *  Each `boot_module_t` is 72 bytes (packed, no name field). The
 *  table holds BOOTINFO_MODULE_COUNT entries; populated entries are
 *  `modules[0 .. module_count)`. Slots past `module_count` are
 *  reserved for future use and must read as `BOOT_MODULE_TYPE_NONE`.
 *
 *  Module names are intentionally not part of the kernel ABI. If
 *  diagnostic names are required they live in a separate debug
 *  manifest or build artifact, not in the bootinfo struct.
 *
 * ====================================================================
 *  KERNEL-SIDE MIRROR
 * ====================================================================
 *
 *  This file is a copy of `shared/include/bootinfo.h`. Both files
 *  must remain byte-identical (struct definitions, constants, enums,
 *  comments). The two-file pattern is intentional for v2: deleting
 *  the kernel-side copy requires updating include paths in
 *  kernel sources and is deferred to a later ABI phase. Until then,
 *  any edit to one file MUST be mirrored to the other.
 */

#define BOOTINFO_MAGIC    0x5245415053454E47ULL /* "REAPGENG" — Reaper Genesis (unchanged from v1) */
/* BOOTINFO_VERSION remains 1 in Phase 1. The version bump to 2 is
 * a Phase 2 kernel change that lands together with the kernel-side
 * population of the v2 fields and the userspace-side update of the
 * Genesis bridge probe check. Bumping the constant now would break
 * the Day 15 marker [TEST] Day 15 Bootinfo Bridge: SUCCESS. and the
 * PARADIGM: Genesis bridge probe PASS. conformance marker, both of
 * which check version == 1. */
#define BOOTINFO_VERSION  1                     /* unchanged in Phase 1; bumps to 2 in Phase 2 */

/* Integrity hash size in bytes. The hash is BLAKE3 of the entire
 * struct with these 32 bytes replaced by zero before the call. */
#define BOOTINFO_INTEGRITY_SIZE  32

/* Number of module table entries. The table is BOOTINFO_MODULE_COUNT
 * * sizeof(boot_module_t) bytes. Populated count is in
 * `bootinfo.module_count`; slots past that count are empty. */
#define BOOTINFO_MODULE_COUNT  16

/* ----- Module type tags (boot_module_t.type) -----
 * 0 is reserved for "empty slot" so an uninitialized module entry is
 * interpretable as empty without needing to be explicitly cleared. */
#define BOOT_MODULE_TYPE_NONE        0u
#define BOOT_MODULE_TYPE_PARADIGM    1u   /* userspace daemon image */
#define BOOT_MODULE_TYPE_GENESIS     2u   /* bootstrap userspace image */
#define BOOT_MODULE_TYPE_DATA        3u   /* opaque data blob */
#define BOOT_MODULE_TYPE_RAMDISK     4u   /* initial root filesystem */
#define BOOT_MODULE_TYPE_FONT        5u   /* font asset for recovery UI */

/* ----- Module flag bits (boot_module_t.flags) ----- */
#define BOOT_MODULE_FLAG_NONE         0u
#define BOOT_MODULE_FLAG_EXECUTABLE   (1u << 0)  /* is an ELF */
#define BOOT_MODULE_FLAG_REQUIRED     (1u << 1)  /* boot fails if integrity
                                                    check fails */
#define BOOT_MODULE_FLAG_TRUSTED      (1u << 2)  /* content_hash is signed
                                                    by build key (future) */
#define BOOT_MODULE_FLAG_LOADED       (1u << 3)  /* kernel has loaded it */

/* ----- Bootinfo flag bits (bootinfo.flags) -----
 * There is intentionally no BOOTINFO_FLAG_INTEGRITY_OK. The hash's
 * presence and validity is communicated by the all-zero hash sentinel,
 * not by a flag bit. One source of truth per fact. */
#define BOOTINFO_FLAG_NONE            0u
#define BOOTINFO_FLAG_FRAMEBUFFER     (1u << 0)  /* framebuffer metadata is
                                                    present and valid */
#define BOOTINFO_FLAG_MODULE_TABLE    (1u << 1)  /* module_count > 0 */

/* ----- Framebuffer pixel format tags (bootinfo.fb_pixel_format) ----- */
#define BOOTINFO_PIXEL_FORMAT_UNKNOWN  0u
#define BOOTINFO_PIXEL_FORMAT_RGB888   1u   /* packed 24 bpp, RGB byte order */
#define BOOTINFO_PIXEL_FORMAT_BGR888   2u   /* packed 24 bpp, BGR byte order */
#define BOOTINFO_PIXEL_FORMAT_RGBA8888 3u   /* 32 bpp */
#define BOOTINFO_PIXEL_FORMAT_BGRA8888 4u   /* 32 bpp */
#define BOOTINFO_PIXEL_FORMAT_XRGB8888 5u   /* 32 bpp, X ignored */

/* ----- Module descriptor (boot_module_t) -----
 * 72 bytes, packed. Field order, types, and offsets are part of the
 * v2 ABI and verified at build time by kernel/verify_bootinfo.c and
 * shared/verify_bootinfo.c. */
typedef struct __attribute__((packed)) boot_module {
    uint64_t phys_base;              /* offset  0 — physical base address */
    uint64_t size;                   /* offset  8 — size in bytes */
    uint64_t entry;                  /* offset 16 — ELF entry, 0 if N/A */
    uint8_t  content_hash[32];       /* offset 24 — BLAKE3 of module bytes */
    uint32_t index;                  /* offset 56 — module index, 0..15 */
    uint32_t type;                   /* offset 60 — BOOT_MODULE_TYPE_* */
    uint32_t flags;                  /* offset 64 — BOOT_MODULE_FLAG_* */
    uint32_t reserved;               /* offset 68 — explicit, future use */
} boot_module_t;

/* ----- Boot info struct (boot_info_t) -----
 * The struct is __attribute__((packed)) to preserve the v1 binary
 * layout. See the file-level comment above for the rationale. */
typedef struct __attribute__((packed)) boot_info {
    /* --- v1 fields, byte-for-byte preserved --- */
    uint64_t magic;                 /* offset   0 */
    uint64_t version;               /* offset   8 — set to BOOTINFO_VERSION (2) */
    uint64_t hhdm_offset;           /* offset  16 */
    uint32_t genesis_cap_slot;      /* offset  24 */
    uint64_t memmap_addr;           /* offset  28 */
    uint64_t memmap_entries;        /* offset  36 */
    uint64_t kernel_start;          /* offset  44 */
    uint64_t kernel_end;            /* offset  52 */
    uint64_t reserved[8];           /* offset  60..123 — preserved for v1 */

    /* --- v2-only fields, appended after the v1 view --- */
    uint64_t flags;                 /* offset 124 — BOOTINFO_FLAG_* */
    uint32_t module_count;          /* offset 132 — populated entries
                                                in modules[0..16) */
    uint32_t reserved_v2_0;         /* offset 136 — explicit reserved */
    uint64_t fb_base;               /* offset 140 — physical base */
    uint32_t fb_width;              /* offset 148 — pixels */
    uint32_t fb_height;             /* offset 152 — pixels */
    uint32_t fb_pitch;              /* offset 156 — bytes per scanline */
    uint32_t fb_pixel_format;       /* offset 160 — BOOTINFO_PIXEL_* */
    uint32_t reserved_v2_1;         /* offset 164 — explicit reserved */
    uint32_t reserved_v2_2;         /* offset 168 — explicit reserved */
    uint32_t reserved_v2_3;         /* offset 172 — explicit reserved */
    uint64_t integrity_hash[4];     /* offset 176 — BOOTINFO_INTEGRITY_SIZE
                                                bytes; BLAKE3 of the struct
                                                with these 32 bytes treated
                                                as zero. */
    boot_module_t modules[BOOTINFO_MODULE_COUNT]; /* offset 208 — module table */

} boot_info_t;

#endif /* REAPER_BOOTINFO_H */
