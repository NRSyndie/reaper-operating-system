/*
 * Bootinfo v2 layout verification (userspace / shared side).
 *
 * This translation unit exists for one purpose: to fail the
 * userspace build at compile time if the Bootinfo v2 ABI layout
 * drifts from the contract documented in shared/include/bootinfo.h
 * and mirrored in kernel/include/bootinfo.h.
 *
 * The userspace build uses -std=gnu99, which does not provide
 * _Static_assert. This file therefore uses the C89/C99 typedef-
 * array trick to encode the same checks as compile-time failures:
 *
 *     typedef char verify_NAME[CONDITION ? 1 : -1];
 *
 * If CONDITION is false, the array size becomes -1, which is not
 * a valid size for any C array type, and the compiler rejects the
 * translation unit. The behavior is identical to _Static_assert for
 * our purposes, just spelled in a C99-compatible way.
 *
 * The asserts are byte-for-byte identical to those in
 * kernel/verify_bootinfo.c. If a change to bootinfo.h passes one
 * side's build but not the other, the kernel and userspace structs
 * have diverged, which is the failure mode this file exists to
 * detect.
 *
 * The file deliberately contains no code. Every typedef is the
 * entire body of the file.
 */

#include <stddef.h>
#include <stdint.h>

#include "bootinfo.h"

/* ------------------------------------------------------------------
 *  Struct shape
 * ------------------------------------------------------------------ */

typedef char verify_boot_info_t_size
    [(sizeof(boot_info_t) == 1360) ? 1 : -1];
typedef char verify_boot_module_t_size
    [(sizeof(boot_module_t) == 72) ? 1 : -1];

/* Both structs MUST be __attribute__((packed)). If a future engineer
 * removes the attribute, alignof becomes the natural alignment of
 * the largest scalar member, and the v1 byte offsets after
 * genesis_cap_slot will shift. The Day 15 closure marker
 * [TEST] Day 15 Bootinfo Bridge: SUCCESS. validates the packed
 * layout, so unpacking breaks the established contract. These two
 * typedefs turn "remove the packed attribute" into a loud build
 * failure rather than a silent ABI break. */
typedef char verify_boot_info_t_packed
    [(_Alignof(boot_info_t) == 1) ? 1 : -1];
typedef char verify_boot_module_t_packed
    [(_Alignof(boot_module_t) == 1) ? 1 : -1];

/* ------------------------------------------------------------------
 *  boot_info_t field offsets — v1 view (must be byte-for-byte stable)
 * ------------------------------------------------------------------ */

typedef char verify_magic_offset
    [(offsetof(boot_info_t, magic)              ==   0) ? 1 : -1];
typedef char verify_version_offset
    [(offsetof(boot_info_t, version)            ==   8) ? 1 : -1];
typedef char verify_hhdm_offset_offset
    [(offsetof(boot_info_t, hhdm_offset)        ==  16) ? 1 : -1];
typedef char verify_genesis_cap_slot_offset
    [(offsetof(boot_info_t, genesis_cap_slot)   ==  24) ? 1 : -1];
typedef char verify_memmap_addr_offset
    [(offsetof(boot_info_t, memmap_addr)        ==  28) ? 1 : -1];
typedef char verify_memmap_entries_offset
    [(offsetof(boot_info_t, memmap_entries)     ==  36) ? 1 : -1];
typedef char verify_kernel_start_offset
    [(offsetof(boot_info_t, kernel_start)       ==  44) ? 1 : -1];
typedef char verify_kernel_end_offset
    [(offsetof(boot_info_t, kernel_end)         ==  52) ? 1 : -1];
typedef char verify_reserved_offset
    [(offsetof(boot_info_t, reserved)           ==  60) ? 1 : -1];

/* ------------------------------------------------------------------
 *  boot_info_t field offsets — v2-only fields (appended after v1 view)
 * ------------------------------------------------------------------ */

typedef char verify_flags_offset
    [(offsetof(boot_info_t, flags)              == 124) ? 1 : -1];
typedef char verify_module_count_offset
    [(offsetof(boot_info_t, module_count)       == 132) ? 1 : -1];
typedef char verify_reserved_v2_0_offset
    [(offsetof(boot_info_t, reserved_v2_0)      == 136) ? 1 : -1];
typedef char verify_fb_base_offset
    [(offsetof(boot_info_t, fb_base)            == 140) ? 1 : -1];
typedef char verify_fb_width_offset
    [(offsetof(boot_info_t, fb_width)           == 148) ? 1 : -1];
typedef char verify_fb_height_offset
    [(offsetof(boot_info_t, fb_height)          == 152) ? 1 : -1];
typedef char verify_fb_pitch_offset
    [(offsetof(boot_info_t, fb_pitch)           == 156) ? 1 : -1];
typedef char verify_fb_pixel_format_offset
    [(offsetof(boot_info_t, fb_pixel_format)    == 160) ? 1 : -1];
typedef char verify_reserved_v2_1_offset
    [(offsetof(boot_info_t, reserved_v2_1)      == 164) ? 1 : -1];
typedef char verify_reserved_v2_2_offset
    [(offsetof(boot_info_t, reserved_v2_2)      == 168) ? 1 : -1];
typedef char verify_reserved_v2_3_offset
    [(offsetof(boot_info_t, reserved_v2_3)      == 172) ? 1 : -1];
typedef char verify_integrity_hash_offset
    [(offsetof(boot_info_t, integrity_hash)     == 176) ? 1 : -1];
typedef char verify_modules_offset
    [(offsetof(boot_info_t, modules)            == 208) ? 1 : -1];

typedef char verify_integrity_hash_size
    [(sizeof(((boot_info_t *)0)->integrity_hash)
        == BOOTINFO_INTEGRITY_SIZE) ? 1 : -1];

/* ------------------------------------------------------------------
 *  boot_module_t field offsets
 * ------------------------------------------------------------------ */

typedef char verify_mod_phys_base_offset
    [(offsetof(boot_module_t, phys_base)        ==  0) ? 1 : -1];
typedef char verify_mod_size_offset
    [(offsetof(boot_module_t, size)             ==  8) ? 1 : -1];
typedef char verify_mod_entry_offset
    [(offsetof(boot_module_t, entry)            == 16) ? 1 : -1];
typedef char verify_mod_content_hash_offset
    [(offsetof(boot_module_t, content_hash)     == 24) ? 1 : -1];
typedef char verify_mod_index_offset
    [(offsetof(boot_module_t, index)            == 56) ? 1 : -1];
typedef char verify_mod_type_offset
    [(offsetof(boot_module_t, type)             == 60) ? 1 : -1];
typedef char verify_mod_flags_offset
    [(offsetof(boot_module_t, flags)            == 64) ? 1 : -1];
typedef char verify_mod_reserved_offset
    [(offsetof(boot_module_t, reserved)         == 68) ? 1 : -1];

typedef char verify_mod_content_hash_size
    [(sizeof(((boot_module_t *)0)->content_hash) == 32) ? 1 : -1];

/* ------------------------------------------------------------------
 *  Constants
 * ------------------------------------------------------------------ */

typedef char verify_bootinfo_magic
    [(BOOTINFO_MAGIC == 0x5245415053454E47ULL) ? 1 : -1];
typedef char verify_bootinfo_version
    [(BOOTINFO_VERSION == 1) ? 1 : -1];
typedef char verify_bootinfo_module_count
    [(BOOTINFO_MODULE_COUNT == 16) ? 1 : -1];
typedef char verify_bootinfo_integrity_size
    [(BOOTINFO_INTEGRITY_SIZE == 32) ? 1 : -1];

/* ------------------------------------------------------------------
 *  Module type tag invariant
 * ------------------------------------------------------------------ */

typedef char verify_boot_module_type_none
    [(BOOT_MODULE_TYPE_NONE == 0) ? 1 : -1];
