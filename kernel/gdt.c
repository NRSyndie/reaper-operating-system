#include "gdt.h"
#include "console.h"
#include "utils.h" /* for memset */
#include "klog.h"

/*
 * The Global Descriptor Table (GDT)
 * 
 * Entry 0: Null Descriptor
 * Entry 1: Kernel Code (Ring 0)
 * Entry 2: Kernel Data (Ring 0)
 * Entry 3: User Data   (Ring 3) - Order matters for SYSCALL/SYSRET!
 * Entry 4: User Code   (Ring 3)
 * Entry 5: TSS (System Segment) - Takes 2 slots (16 bytes)
 */
static struct {
    gdt_entry_t entries[5];
    gdt_system_entry_t tss_entry;
} __attribute__((packed)) gdt;

static gdtr_t gdtr;
static tss_t tss;
static uint8_t bootstrap_rsp0_stack[4096] __attribute__((aligned(16)));
static uint8_t bootstrap_ist1_stack[4096] __attribute__((aligned(16)));

/*
 * Helper to encode standard GDT entries
 */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt.entries[num].base_low    = (base & 0xFFFF);
    gdt.entries[num].base_middle = (base >> 16) & 0xFF;
    gdt.entries[num].base_high   = (base >> 24) & 0xFF;

    gdt.entries[num].limit_low   = (limit & 0xFFFF);
    gdt.entries[num].granularity = (limit >> 16) & 0x0F;

    gdt.entries[num].granularity |= (gran & 0xF0);
    gdt.entries[num].access      = access;
}

/*
 * Helper to encode the TSS System Entry (16 bytes)
 */
static void gdt_set_tss_gate(uint64_t base, uint32_t limit) {
    gdt.tss_entry.base_low    = (base & 0xFFFF);
    gdt.tss_entry.base_middle = (base >> 16) & 0xFF;
    gdt.tss_entry.base_high   = (base >> 24) & 0xFF;
    gdt.tss_entry.base_upper  = (base >> 32) & 0xFFFFFFFF;

    gdt.tss_entry.limit_low   = (limit & 0xFFFF);
    
    // Type 9 = 64-bit TSS (Available), P=1, DPL=0
    // Access Byte: 1 (Present) | 00 (Ring 0) | 0 (System) | 1001 (Type) -> 10001001 -> 0x89
    gdt.tss_entry.access      = 0x89; 

    // Granularity: G=0 (Byte), Avail=0, L=0, AVL=0 -> 0x00
    // But Limit High is in low nibble...
    gdt.tss_entry.granularity = 0x00; 
    
    gdt.tss_entry.reserved    = 0;
}

/*
 * Load GDT assembly wrapper
 */
extern void gdt_flush(uint64_t gdtr_ptr);

/*
 * Load TR (Task Register) assembly wrapper
 */
extern void tss_flush(void);

void gdt_init(void) {
    // 1. Zero out the TSS
    memset(&tss, 0, sizeof(tss_t));
    tss.rsp0 = (uint64_t)(bootstrap_rsp0_stack + sizeof(bootstrap_rsp0_stack));
    tss.ist1 = (uint64_t)(bootstrap_ist1_stack + sizeof(bootstrap_ist1_stack));
    
    // 2. Setup GDT Entries
    
    // 0: Null Descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code: Access=0x9A (P=1, DPL=0, S=1, E=1, RW=1), Gran=0xAF (L=1)
    gdt_set_gate(1, 0, 0, 0x9A, 0xAF);

    // 2: Kernel Data: Access=0x92 (P=1, DPL=0, S=1, E=0, RW=1), Gran=0xCF
    gdt_set_gate(2, 0, 0, 0x92, 0xCF);

    // 3: User Data:   Access=0xF2 (P=1, DPL=3, S=1, E=0, RW=1), Gran=0xCF
    gdt_set_gate(3, 0, 0, 0xF2, 0xCF);

    // 4: User Code:   Access=0xFA (P=1, DPL=3, S=1, E=1, RW=1), Gran=0xAF
    gdt_set_gate(4, 0, 0, 0xFA, 0xAF);

    // 5: TSS
    gdt_set_tss_gate((uint64_t)&tss, sizeof(tss_t) - 1);

    // 3. Load GDT
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)&gdt;

        gdt_flush((uint64_t)&gdtr);
        tss_flush();

    }

void tss_set_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

void tss_set_ist(uint8_t index, uint64_t rsp) {
    switch (index) {
        case 1: tss.ist1 = rsp; break;
        case 2: tss.ist2 = rsp; break;
        case 3: tss.ist3 = rsp; break;
        case 4: tss.ist4 = rsp; break;
        case 5: tss.ist5 = rsp; break;
        case 6: tss.ist6 = rsp; break;
        case 7: tss.ist7 = rsp; break;
        default: break;
    }
}
