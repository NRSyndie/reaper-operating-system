#ifndef REAPER_GDT_H
#define REAPER_GDT_H

#include <stdint.h>

/*
 * The Task State Segment (TSS)
 * Required for x86-64 hardware task switching (Stack Switching).
 * Specifically, RSP0 is loaded into RSP when a privilege level change 
 * (Ring 3 -> Ring 0) occurs via Interrupts.
 */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;      // Kernel Stack Pointer (Ring 0)
    uint64_t rsp1;      // Ring 1 Stack (Unused)
    uint64_t rsp2;      // Ring 2 Stack (Unused)
    uint64_t reserved1;
    uint64_t ist1;      // Interrupt Stack Table 1 (Double Faults)
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

/*
 * GDT Descriptor (Segment Descriptor)
 * 64-bit Mode GDT Entries are 8 bytes mostly, but System Segments (TSS) are 16 bytes.
 */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

/*
 * System Segment Descriptor (TSS) - 16 Bytes
 */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;      // Present, Ring 0, Type 9 (Available TSS)
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) gdt_system_entry_t;

/*
 * The GDT Pointer structure loaded by LGDT
 */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

/*
 * GDT Selectors (Offsets)
 */
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28

/*
 * Initialization
 */
void gdt_init(void);

/*
 * Set the Kernel Stack (RSP0) for the current CPU's TSS.
 * This must be called during Context Switches.
 */
void tss_set_stack(uint64_t rsp0);
void tss_set_ist(uint8_t index, uint64_t rsp);

#endif /* REAPER_GDT_H */
