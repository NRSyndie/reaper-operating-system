/*
 * Bootinfo v2 layout verification (kernel side).
 *
 * This translation unit exists for one purpose: to fail the kernel
 * build at compile time if the Bootinfo v2 ABI layout drifts from
 * the contract documented in shared/include/bootinfo.h and mirrored
 * in kernel/include/bootinfo.h.
 *
 * The asserts verify:
 *   - sizeof / alignof of boot_info_t and boot_module_t
 *   - the byte offset of every named field in both structs
 *   - that the structs are __attribute__((packed)) (alignof == 1)
 *   - the constant values of BOOTINFO_MAGIC, BOOTINFO_VERSION, and
 *     BOOTINFO_MODULE_COUNT
 *   - the integrity hash size matches BOOTINFO_INTEGRITY_SIZE
 *
 * If a future change to bootinfo.h causes any of these to fail, the
 * build aborts immediately. That is the desired behavior: a silent
 * ABI drift is far worse than a noisy build failure.
 *
 * The file deliberately contains no code. Every static_assert is the
 * entire body of the file.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "bootinfo.h"

/* ------------------------------------------------------------------
 *  Struct shape
 * ------------------------------------------------------------------ */

/* boot_info_t total size: 124 (v1 view: 9 named fields + reserved[8])
 * + 52 (v2 prelude: flags + module_count + reserved_v2_0 + fb_base +
 * fb_width + fb_height + fb_pitch + fb_pixel_format + reserved_v2_1/2/3)
 * + 32 (integrity_hash) + 16 * 72 (modules) = 1360. The struct is
 * packed, so sizeof is the exact byte count. */
_Static_assert(sizeof(boot_info_t) == 1360,
               "boot_info_t size must be 1360 bytes");

/* boot_module_t total size: 8 + 8 + 8 + 32 + 4 + 4 + 4 + 4 = 72.
 * Packed, so sizeof is exact. */
_Static_assert(sizeof(boot_module_t) == 72,
               "boot_module_t size must be 72 bytes");

/* Both structs MUST be __attribute__((packed)). If a future engineer
 * removes the attribute, alignof becomes the natural alignment of
 * the largest scalar member, and the v1 byte offsets after
 * genesis_cap_slot will shift. The Day 15 closure marker
 * [TEST] Day 15 Bootinfo Bridge: SUCCESS. validates the packed
 * layout, so unpacking breaks the established contract. These two
 * asserts turn "remove the packed attribute" into a loud build
 * failure rather than a silent ABI break. */
_Static_assert(_Alignof(boot_info_t) == 1,
               "boot_info_t must be __attribute__((packed)) "
               "(alignof == 1). Do not remove the packed attribute; "
               "it preserves the v1 byte offsets validated by the "
               "Day 15 closure marker.");
_Static_assert(_Alignof(boot_module_t) == 1,
               "boot_module_t must be __attribute__((packed)) "
               "(alignof == 1) for the same reason as boot_info_t.");

/* ------------------------------------------------------------------
 *  boot_info_t field offsets — v1 view (must be byte-for-byte stable)
 * ------------------------------------------------------------------ */

_Static_assert(offsetof(boot_info_t, magic)              ==   0,
               "magic offset");
_Static_assert(offsetof(boot_info_t, version)            ==   8,
               "version offset");
_Static_assert(offsetof(boot_info_t, hhdm_offset)        ==  16,
               "hhdm_offset offset");
_Static_assert(offsetof(boot_info_t, genesis_cap_slot)   ==  24,
               "genesis_cap_slot offset");
_Static_assert(offsetof(boot_info_t, memmap_addr)        ==  28,
               "memmap_addr offset");
_Static_assert(offsetof(boot_info_t, memmap_entries)     ==  36,
               "memmap_entries offset");
_Static_assert(offsetof(boot_info_t, kernel_start)       ==  44,
               "kernel_start offset");
_Static_assert(offsetof(boot_info_t, kernel_end)         ==  52,
               "kernel_end offset");
_Static_assert(offsetof(boot_info_t, reserved)           ==  60,
               "reserved[8] offset (v1 64-byte reserved block)");

/* ------------------------------------------------------------------
 *  boot_info_t field offsets — v2-only fields (appended after v1 view)
 * ------------------------------------------------------------------ */

_Static_assert(offsetof(boot_info_t, flags)              == 124,
               "flags offset");
_Static_assert(offsetof(boot_info_t, module_count)       == 132,
               "module_count offset");
_Static_assert(offsetof(boot_info_t, reserved_v2_0)      == 136,
               "reserved_v2_0 offset");
_Static_assert(offsetof(boot_info_t, fb_base)            == 140,
               "fb_base offset");
_Static_assert(offsetof(boot_info_t, fb_width)           == 148,
               "fb_width offset");
_Static_assert(offsetof(boot_info_t, fb_height)          == 152,
               "fb_height offset");
_Static_assert(offsetof(boot_info_t, fb_pitch)           == 156,
               "fb_pitch offset");
_Static_assert(offsetof(boot_info_t, fb_pixel_format)    == 160,
               "fb_pixel_format offset");
_Static_assert(offsetof(boot_info_t, reserved_v2_1)      == 164,
               "reserved_v2_1 offset");
_Static_assert(offsetof(boot_info_t, reserved_v2_2)      == 168,
               "reserved_v2_2 offset");
_Static_assert(offsetof(boot_info_t, reserved_v2_3)      == 172,
               "reserved_v2_3 offset");
_Static_assert(offsetof(boot_info_t, integrity_hash)     == 176,
               "integrity_hash offset");
_Static_assert(offsetof(boot_info_t, modules)            == 208,
               "modules[16] offset");

/* The integrity_hash field is exactly 32 bytes (BLAKE3 output). */
_Static_assert(sizeof(((boot_info_t *)0)->integrity_hash)
                   == BOOTINFO_INTEGRITY_SIZE,
               "integrity_hash size must equal BOOTINFO_INTEGRITY_SIZE");

/* ------------------------------------------------------------------
 *  boot_module_t field offsets
 * ------------------------------------------------------------------ */

_Static_assert(offsetof(boot_module_t, phys_base)        ==  0,
               "boot_module_t.phys_base offset");
_Static_assert(offsetof(boot_module_t, size)             ==  8,
               "boot_module_t.size offset");
_Static_assert(offsetof(boot_module_t, entry)            == 16,
               "boot_module_t.entry offset");
_Static_assert(offsetof(boot_module_t, content_hash)     == 24,
               "boot_module_t.content_hash offset");
_Static_assert(offsetof(boot_module_t, index)            == 56,
               "boot_module_t.index offset");
_Static_assert(offsetof(boot_module_t, type)             == 60,
               "boot_module_t.type offset");
_Static_assert(offsetof(boot_module_t, flags)            == 64,
               "boot_module_t.flags offset");
_Static_assert(offsetof(boot_module_t, reserved)         == 68,
               "boot_module_t.reserved offset");

_Static_assert(sizeof(((boot_module_t *)0)->content_hash) == 32,
               "boot_module_t.content_hash size must be 32 bytes (BLAKE3)");

/* ------------------------------------------------------------------
 *  Constants
 * ------------------------------------------------------------------ */

_Static_assert(BOOTINFO_MAGIC   == 0x5245415053454E47ULL,
               "BOOTINFO_MAGIC must remain 0x5245415053454E47 (\"REAPGENG\")");
_Static_assert(BOOTINFO_VERSION == 1,
               "BOOTINFO_VERSION must remain 1 in Phase 1; "
               "the bump to 2 is a Phase 2 kernel change");
_Static_assert(BOOTINFO_MODULE_COUNT == 16,
               "BOOTINFO_MODULE_COUNT must be 16");
_Static_assert(BOOTINFO_INTEGRITY_SIZE == 32,
               "BOOTINFO_INTEGRITY_SIZE must be 32 (BLAKE3 output)");

/* ------------------------------------------------------------------
 *  Module type tag invariant
 * ------------------------------------------------------------------ */

/* BOOT_MODULE_TYPE_NONE must be 0 so an uninitialized module entry
 * (zeroed by genesis's fast_zero of the bootinfo page) reads as
 * "empty slot" without an explicit clear. */
_Static_assert(BOOT_MODULE_TYPE_NONE == 0,
               "BOOT_MODULE_TYPE_NONE must be 0 (empty-slot sentinel)");
