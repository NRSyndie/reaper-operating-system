#include "gdt.h"
#include "console.h"
#include "utils.h" /* for memset */
#include "klog.h"
#include "cpu.h"

/*
 * Load GDT assembly wrapper
 */
extern void gdt_flush(uint64_t gdtr_ptr);

/*
 * Load TR (Task Register) assembly wrapper
 */
extern void tss_flush(void);

void gdt_init(void) {
    /* BSP (CPU 0) initialization using the new per-CPU logic in cpu.c */
    void cpu_init_per_cpu(uint32_t logical_id);
    cpu_init_per_cpu(0);
}

typedef struct {
    uint8_t gdt_data[64];
    gdtr_t  gdtr;
    tss_t   tss;
    uint8_t stack_rsp0[4096];
    uint8_t stack_ist1[4096];
} cpu_context_t;
extern cpu_context_t g_cpu_contexts[];

void tss_set_stack(uint64_t rsp0) {
    uint32_t id = cpu_get_id();
    if (id < 64) {
        g_cpu_contexts[id].tss.rsp0 = rsp0;
    }
}

void tss_set_ist(uint8_t index, uint64_t rsp) {
    uint32_t id = cpu_get_id();
    if (id >= 64) return;
    tss_t* tss = &g_cpu_contexts[id].tss;

    switch (index) {
        case 1: tss->ist1 = rsp; break;
        case 2: tss->ist2 = rsp; break;
        case 3: tss->ist3 = rsp; break;
        case 4: tss->ist4 = rsp; break;
        case 5: tss->ist5 = rsp; break;
        case 6: tss->ist6 = rsp; break;
        case 7: tss->ist7 = rsp; break;
        default: break;
    }
}
