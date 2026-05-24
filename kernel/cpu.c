#include "include/cpu.h"
#include "include/console.h"
#include "include/klog.h"
#include "include/limine.h"
#include "include/pmm.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/scheduler.h"
#include <stddef.h>

#define CPU_MAX_TOPOLOGY 64

/* Per-CPU Context */
typedef struct {
    uint8_t gdt_data[64]; /* Space for GDT entries */
    gdtr_t  gdtr;
    tss_t   tss;
    uint8_t stack_rsp0[4096] __attribute__((aligned(16)));
    uint8_t stack_ist1[4096] __attribute__((aligned(16)));
} cpu_context_t;

cpu_context_t g_cpu_contexts[CPU_MAX_TOPOLOGY];
#define IA32_APIC_BASE_MSR 0x1B
#define APIC_BASE_X2APIC_BIT (1ULL << 10)
#define APIC_BASE_ENABLE_BIT (1ULL << 11)
#define APIC_BASE_ADDR_MASK 0xFFFFF000ULL
#define APIC_MMIO_EOI 0x0B0
#define APIC_MMIO_ICR_LOW 0x300
#define APIC_MMIO_ICR_HIGH 0x310
#define APIC_ICR_DELIVERY_PENDING (1u << 12)
#define X2APIC_MSR_EOI 0x80B
#define X2APIC_MSR_ICR 0x830

extern struct limine_mp_request mp_request;

static uint32_t g_cpu_count = 1;
static uint32_t g_bsp_lapic_id = 0;
static uint32_t g_lapic_ids[CPU_MAX_TOPOLOGY] = {0};
static struct limine_mp_info* g_mp_info[CPU_MAX_TOPOLOGY] = {0};
static volatile uint32_t g_ap_started = 0;
static volatile uint64_t g_sched_online_mask = 1ULL; /* BSP online by default */
static volatile uint64_t g_ap_start_request_mask = 0;
static volatile uint64_t g_ap_runtime_ready_mask = 0;
static volatile uint32_t g_ap_staged = 0;
static volatile uint64_t g_resched_ipi_attempts = 0;
static volatile uint64_t g_resched_ipi_deferred = 0;
static volatile uint64_t g_resched_ipi_sent = 0;
static volatile uint64_t g_resched_ipi_failed = 0;
static volatile uint64_t g_tlb_ipi_sent = 0;
static volatile uint64_t g_tlb_ipi_failed = 0;
static volatile uint64_t g_tlb_shootdown_timeouts = 0;
static volatile uint64_t g_tlb_req_gen = 0;
static volatile uint64_t g_tlb_req_addr = 0;
static volatile uint64_t g_tlb_req_targets = 0;
static volatile uint64_t g_tlb_req_acks = 0;
static uint64_t g_tlb_seen_gen[CPU_MAX_TOPOLOGY] = {0};
static bool g_ipi_transport_ready = false;
static bool g_ipi_x2apic = false;
static volatile uint32_t* g_apic_mmio = NULL;

static uint32_t cpu_read_initial_apic_id(void) {
    struct cpuid_result res;
    cpuid(0x01, 0, &res);
    return (res.ebx >> 24) & 0xFF;
}

static void cpu_ap_entry_stub(struct limine_mp_info* info) {
    uint32_t logical_id = info ? (uint32_t)info->extra_argument : 0;
    uint64_t my_bit = (logical_id < 64) ? (1ULL << logical_id) : 0;

    __atomic_add_fetch(&g_ap_started, 1, __ATOMIC_ACQ_REL);

    if (my_bit == 0) {
        for (;;) {
            __asm__ volatile ("pause");
        }
    }

    /* Controlled AP runtime start: BSP must explicitly request activation. */
    for (;;) {
        uint64_t req = __atomic_load_n(&g_ap_start_request_mask, __ATOMIC_ACQUIRE);
        if (req & my_bit) {
            break;
        }
        __asm__ volatile ("pause");
    }

    /* 1. Initialize Per-CPU Structures (GDT, TSS, IDT) */
    void cpu_init_per_cpu(uint32_t logical_id);
    cpu_init_per_cpu(logical_id);

    /* 2. Enable PCID and Extended State if supported */
    if (cpu_has_pcid()) {
        cpu_enable_pcid();
    }
    cpu_init_extended_state();

    /* 3. Mark as runtime-ready */
    __atomic_fetch_or(&g_ap_runtime_ready_mask, my_bit, __ATOMIC_ACQ_REL);

    /* 4. Wait for scheduler activation */
    for (;;) {
        uint64_t online = __atomic_load_n(&g_sched_online_mask, __ATOMIC_ACQUIRE);
        if (online & my_bit) {
            break;
        }
        __asm__ volatile ("pause");
    }

    /* 5. Enter Scheduler Dispatch Loop */
    klog_info("CPU %u: Entering scheduler dispatch loop.", logical_id);
    sti();
    schedule();

    /* Should never reach here */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void cpu_ipi_transport_init(void) {
    uint64_t apic_base;
    uint64_t apic_phys;

    if (g_ipi_transport_ready) {
        return;
    }

    apic_base = rdmsr(IA32_APIC_BASE_MSR);
    if ((apic_base & APIC_BASE_ENABLE_BIT) == 0) {
        g_ipi_transport_ready = false;
        return;
    }

    g_ipi_x2apic = (apic_base & APIC_BASE_X2APIC_BIT) != 0;
    if (g_ipi_x2apic) {
        g_ipi_transport_ready = true;
        klog_info("CPU: IPI transport ready mode=x2apic");
        return;
    }

    apic_phys = apic_base & APIC_BASE_ADDR_MASK;
    g_apic_mmio = (volatile uint32_t*)pmm_phys_to_virt(apic_phys);
    if (!g_apic_mmio) {
        g_ipi_transport_ready = false;
        return;
    }

    g_ipi_transport_ready = true;
    klog_info("CPU: IPI transport ready mode=xapic phys=0x%lx", apic_phys);
}

static inline void cpu_apic_eoi(void) {
    if (!g_ipi_transport_ready) return;

    if (g_ipi_x2apic) {
        wrmsr(X2APIC_MSR_EOI, 0);
        return;
    }
    if (g_apic_mmio) {
        g_apic_mmio[APIC_MMIO_EOI / 4] = 0;
    }
}

static bool cpu_send_ipi(uint32_t target_lapic_id, uint8_t vector) {
    uint32_t spins = 100000;

    if (g_ipi_x2apic) {
        uint64_t icr = ((uint64_t)target_lapic_id << 32) | (uint64_t)vector;
        wrmsr(X2APIC_MSR_ICR, icr);
        return true;
    }

    if (!g_apic_mmio) return false;

    while ((g_apic_mmio[APIC_MMIO_ICR_LOW / 4] & APIC_ICR_DELIVERY_PENDING) && spins--) {
        __asm__ volatile ("pause");
    }
    if (spins == 0) return false;

    g_apic_mmio[APIC_MMIO_ICR_HIGH / 4] = (target_lapic_id << 24);
    g_apic_mmio[APIC_MMIO_ICR_LOW / 4] = vector;

    spins = 100000;
    while ((g_apic_mmio[APIC_MMIO_ICR_LOW / 4] & APIC_ICR_DELIVERY_PENDING) && spins--) {
        __asm__ volatile ("pause");
    }
    return spins != 0;
}

static inline uint64_t cpu_bit_for(uint32_t cpu_id) {
    if (cpu_id >= 64) return 0;
    return 1ULL << cpu_id;
}

static uint32_t cpu_popcount64(uint64_t v) {
    uint32_t c = 0;
    while (v) {
        c += (uint32_t)(v & 1ULL);
        v >>= 1;
    }
    return c;
}

void cpuid(uint32_t leaf, uint32_t subleaf, struct cpuid_result *result) {
    __asm__ volatile (
        "cpuid"
        : "=a"(result->eax), "=b"(result->ebx), "=c"(result->ecx), "=d"(result->edx)
        : "a"(leaf), "c"(subleaf)
        : "memory"
    );
}

bool cpu_has_pcid(void) {
    struct cpuid_result res;
    cpuid(0x01, 0, &res);
    // PCID is bit 17 of ECX
    return (res.ecx & (1 << 17)) != 0;
}

bool cpu_has_cet_ss(void) {
    struct cpuid_result res;
    cpuid(0x07, 0, &res);
    // CET_SS is bit 7 of ECX
    return (res.ecx & (1 << 7)) != 0;
}

bool cpu_has_cet_ibt(void) {
    struct cpuid_result res;
    cpuid(0x07, 0, &res);
    // CET_IBT is bit 20 of ECX
    return (res.ecx & (1 << 20)) != 0;
}

bool cpu_has_pku(void) {
    struct cpuid_result res;
    cpuid(0x07, 0, &res);
    // PKU is bit 3 of ECX
    return (res.ecx & (1 << 3)) != 0;
}

bool cpu_has_invpcid(void) {
    struct cpuid_result res;
    cpuid(0x07, 0, &res);
    // INVPCID is bit 10 of EBX
    return (res.ebx & (1 << 10)) != 0;
}

bool cpu_has_rdrand(void) {
    struct cpuid_result res;
    cpuid(0x01, 0, &res);
    // RDRAND is bit 30 of ECX
    return (res.ecx & (1 << 30)) != 0;
}

void cpu_enable_pcid(void) {
    if (!cpu_has_pcid()) {
                return;
    }

    // Verify CR3 bits 11:0 are zero
    uint64_t cr3 = read_cr3();
    if (cr3 & 0xFFF) {
                return;
    }

    uint64_t cr4 = read_cr4();
    cr4 |= (1 << 17); // Set PCIDE bit
    write_cr4(cr4);

    // Verify bit is set
    if (read_cr4() & (1 << 17)) {
            } else {
            }
}

static uint8_t fpu_mode = FPU_MODE_NONE;

uint8_t cpu_get_fpu_mode(void) {
    return fpu_mode;
}

uint32_t cpu_get_id(void) {
    uint32_t lapic_id = cpu_read_initial_apic_id();
    uint32_t count = g_cpu_count;

    if (count == 0 || count > CPU_MAX_TOPOLOGY) {
        return 0;
    }

    for (uint32_t i = 0; i < count; i++) {
        if (g_lapic_ids[i] == lapic_id) {
            return i;
        }
    }
    return 0;
}

uint32_t cpu_get_count(void) {
    return g_cpu_count ? g_cpu_count : 1;
}

uint32_t cpu_get_online_count(void) {
    uint64_t mask = __atomic_load_n(&g_sched_online_mask, __ATOMIC_ACQUIRE);
    return cpu_popcount64(mask);
}

uint32_t cpu_get_started_count(void) {
    return __atomic_load_n(&g_ap_started, __ATOMIC_ACQUIRE);
}

uint32_t cpu_get_runtime_ready_count(void) {
    uint64_t mask = __atomic_load_n(&g_ap_runtime_ready_mask, __ATOMIC_ACQUIRE);
    return cpu_popcount64(mask);
}

void cpu_request_ap_runtime_start(uint32_t cpu_id) {
    uint64_t bit = cpu_bit_for(cpu_id);
    if (cpu_id == 0) return;
    if (cpu_id >= g_cpu_count) return;
    if (bit == 0) return;
    __atomic_fetch_or(&g_ap_start_request_mask, bit, __ATOMIC_ACQ_REL);
}

void cpu_mark_sched_online(uint32_t cpu_id) {
    uint64_t bit = cpu_bit_for(cpu_id);
    if (bit == 0) return;
    __atomic_fetch_or(&g_sched_online_mask, bit, __ATOMIC_ACQ_REL);
}

void cpu_mark_sched_offline(uint32_t cpu_id) {
    uint64_t bit = cpu_bit_for(cpu_id);
    if (bit == 0) return;
    __atomic_fetch_and(&g_sched_online_mask, ~bit, __ATOMIC_ACQ_REL);
}

bool cpu_is_bsp(void) {
    return cpu_read_initial_apic_id() == g_bsp_lapic_id;
}

void cpu_topology_init(void) {
    uint32_t detected_lapic = cpu_read_initial_apic_id();

    g_cpu_count = 1;
    g_bsp_lapic_id = detected_lapic;
    g_lapic_ids[0] = detected_lapic;
    g_mp_info[0] = NULL;
    __atomic_store_n(&g_ap_started, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_sched_online_mask, 1ULL, __ATOMIC_RELEASE);
    __atomic_store_n(&g_ap_start_request_mask, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_ap_runtime_ready_mask, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_ap_staged, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_tlb_req_gen, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_tlb_req_addr, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_tlb_req_targets, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_tlb_req_acks, 0, __ATOMIC_RELEASE);

    if (mp_request.response && mp_request.response->cpu_count > 0 && mp_request.response->cpus) {
        uint64_t max = mp_request.response->cpu_count;
        if (max > CPU_MAX_TOPOLOGY) {
            max = CPU_MAX_TOPOLOGY;
        }

        g_cpu_count = (uint32_t)max;
        g_bsp_lapic_id = mp_request.response->bsp_lapic_id;

        for (uint32_t i = 0; i < g_cpu_count; i++) {
            struct limine_mp_info* info = mp_request.response->cpus[i];
            g_mp_info[i] = info;
            g_lapic_ids[i] = info ? info->lapic_id : 0;
        }
    }

    klog_info("CPU: Topology initialized count=%u bsp_lapic_id=%u current_lapic_id=%u",
              g_cpu_count,
              g_bsp_lapic_id,
              detected_lapic);
}

void cpu_smp_stage_ap_stub_start(void) {
    uint32_t staged = 0;

    if (g_cpu_count <= 1) {
        return;
    }

    for (uint32_t i = 0; i < g_cpu_count; i++) {
        struct limine_mp_info* info = g_mp_info[i];
        if (!info) continue;
        if (info->lapic_id == g_bsp_lapic_id) continue;

        info->extra_argument = (uint64_t)i;
        info->goto_address = cpu_ap_entry_stub;
        staged++;
    }

    __atomic_store_n(&g_ap_staged, staged, __ATOMIC_RELEASE);
    klog_info("CPU: AP stub stage armed staged_aps=%u total_cpus=%u", staged, g_cpu_count);
}

void cpu_handle_resched_ipi(void) {
    cpu_apic_eoi();
}

void cpu_handle_tlb_ipi(void) {
    uint32_t cpu_id = cpu_get_id();
    uint64_t gen = __atomic_load_n(&g_tlb_req_gen, __ATOMIC_ACQUIRE);
    uint64_t my_bit = cpu_bit_for(cpu_id);

    if (cpu_id < CPU_MAX_TOPOLOGY && gen != 0 && my_bit != 0) {
        if (g_tlb_seen_gen[cpu_id] != gen) {
            uint64_t addr = __atomic_load_n(&g_tlb_req_addr, __ATOMIC_ACQUIRE);
            __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
            g_tlb_seen_gen[cpu_id] = gen;
        }
        __atomic_fetch_or(&g_tlb_req_acks, my_bit, __ATOMIC_ACQ_REL);
    }

    cpu_apic_eoi();
}

void cpu_request_resched_ipi(uint32_t target_cpu) {
    uint32_t target_lapic_id;
    uint32_t online = cpu_get_online_count();

    if (online == 0) online = 1;
    if (target_cpu >= online) return;
    if (target_cpu == cpu_get_id()) return;

    __atomic_add_fetch(&g_resched_ipi_attempts, 1, __ATOMIC_ACQ_REL);
    cpu_ipi_transport_init();

    if (!g_ipi_transport_ready) {
        __atomic_add_fetch(&g_resched_ipi_deferred, 1, __ATOMIC_ACQ_REL);
        return;
    }

    target_lapic_id = g_lapic_ids[target_cpu];
    if (target_lapic_id == 0 && target_cpu != 0) {
        __atomic_add_fetch(&g_resched_ipi_failed, 1, __ATOMIC_ACQ_REL);
        return;
    }

    if (cpu_send_ipi(target_lapic_id, CPU_IPI_RESCHED_VECTOR)) {
        __atomic_add_fetch(&g_resched_ipi_sent, 1, __ATOMIC_ACQ_REL);
    } else {
        __atomic_add_fetch(&g_resched_ipi_failed, 1, __ATOMIC_ACQ_REL);
    }
}

bool cpu_tlb_shootdown_page(uint64_t addr) {
    uint32_t online = cpu_get_online_count();
    uint32_t self = cpu_get_id();
    uint64_t targets = 0;
    uint64_t gen;
    uint32_t spins = 1000000;

    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
    if (online <= 1) return true;
    if (online > 64) online = 64;

    cpu_ipi_transport_init();
    if (!g_ipi_transport_ready) return false;

    __atomic_store_n(&g_tlb_req_addr, addr, __ATOMIC_RELEASE);
    __atomic_store_n(&g_tlb_req_targets, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_tlb_req_acks, 0, __ATOMIC_RELEASE);
    gen = __atomic_add_fetch(&g_tlb_req_gen, 1, __ATOMIC_ACQ_REL);
    if (self < CPU_MAX_TOPOLOGY) {
        g_tlb_seen_gen[self] = gen;
    }

    for (uint32_t c = 0; c < online; c++) {
        uint64_t bit;
        uint32_t lapic;
        if (c == self) continue;
        bit = cpu_bit_for(c);
        if (bit == 0) continue;
        lapic = g_lapic_ids[c];
        if (lapic == 0 && c != 0) continue;
        targets |= bit;
        if (!cpu_send_ipi(lapic, CPU_IPI_TLB_VECTOR)) {
            __atomic_add_fetch(&g_tlb_ipi_failed, 1, __ATOMIC_ACQ_REL);
            return false;
        }
        __atomic_add_fetch(&g_tlb_ipi_sent, 1, __ATOMIC_ACQ_REL);
    }

    __atomic_store_n(&g_tlb_req_targets, targets, __ATOMIC_RELEASE);
    if (targets == 0) return true;

    while (spins--) {
        uint64_t acks = __atomic_load_n(&g_tlb_req_acks, __ATOMIC_ACQUIRE);
        if ((acks & targets) == targets) {
            return true;
        }
        __asm__ volatile ("pause");
    }

    __atomic_add_fetch(&g_tlb_shootdown_timeouts, 1, __ATOMIC_ACQ_REL);
    return false;
}

void cpu_init_extended_state(void) {
    struct cpuid_result res;
    
    // 1. Audit Features
    cpuid(0x01, 0, &res);
    bool has_sse = (res.edx & (1 << 25)) != 0;
    bool has_fxsave = (res.edx & (1 << 24)) != 0;
    bool has_xsave = (res.ecx & (1 << 26)) != 0;
    bool has_avx = (res.ecx & (1 << 28)) != 0;

    
    if (!has_sse) {
                return;
    }

    if (has_xsave) {
        fpu_mode = FPU_MODE_XSAVE;
    } else if (has_fxsave) {
        fpu_mode = FPU_MODE_FXSAVE;
    }

    // 2. Activate SSE/FPU in CR0
    uint64_t cr0 = read_cr0();
    cr0 &= ~(1 << 2); // Clear EM (Emulation)
    cr0 |= (1 << 1);  // Set MP (Monitor Coprocessor)
    cr0 |= (1 << 5);  // Set NE (Numeric Error)
    write_cr0(cr0);
    
    // 3. Activate FXSAVE/XSAVE in CR4
    uint64_t cr4 = read_cr4();
    cr4 |= (1 << 9);  // Set OSFXSR
    cr4 |= (1 << 10); // Set OSXMMEXCPT
    
    if (has_xsave) {
        cr4 |= (1 << 18); // Set OSXSAVE
    }
    write_cr4(cr4);
    
    // 4. Configure XCR0 if XSAVE is enabled
    if (has_xsave) {
        uint64_t xcr0 = XCR0_X87 | XCR0_SSE;
        if (has_avx) {
            xcr0 |= XCR0_AVX;
        }
        write_xcr0(xcr0);

        cpuid(0x0D, 0, &res);
            }
}

void invpcid(uint64_t type, uint16_t pcid, uint64_t addr) {
    if (cpu_has_invpcid()) {
        struct invpcid_desc desc = {
            .pcid = pcid,
            .reserved = 0,
            .addr = addr
        };
        __asm__ volatile (
            "invpcid %0, %1"
            :
            : "m"(desc), "r"(type)
            : "memory"
        );
    } else {
        // Fallback logic
        switch (type) {
            case INVPCID_TYPE_INDIVIDUAL_ADDR:
                // If it's for current PCID, we can use invlpg
                if (pcid == (read_cr3() & 0xFFF)) {
                    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
                } else {
                    // Otherwise, we have to reload CR3 to flush
                    write_cr3(read_cr3());
                }
                break;
            default:
                // For all other types, reload CR3 is the safest fallback
                write_cr3(read_cr3());
                break;
        }
    }
}

extern void gdt_flush(uint64_t gdtr_ptr);
extern void tss_flush(void);

void cpu_init_per_cpu(uint32_t logical_id) {
    if (logical_id >= CPU_MAX_TOPOLOGY) return;

    cpu_context_t* ctx = &g_cpu_contexts[logical_id];
    fast_zero(ctx, sizeof(cpu_context_t));

    /* 1. Setup GDT for this CPU */
    uint64_t* entries = (uint64_t*)ctx->gdt_data;
    
    // 0: Null
    entries[0] = 0;
    // 1: Kernel Code: Access=0x9A, Gran=0xAF
    entries[1] = 0x00AF9A000000FFFFULL;
    // 2: Kernel Data: Access=0x92, Gran=0xCF
    entries[2] = 0x00CF92000000FFFFULL;
    // 3: User Data:   Access=0xF2, Gran=0xCF
    entries[3] = 0x00CFF2000000FFFFULL;
    // 4: User Code:   Access=0xFA, Gran=0xAF
    entries[4] = 0x00AFFA000000FFFFULL;
    
    // 5: TSS (16 bytes)
    uint64_t tss_base = (uint64_t)&ctx->tss;
    uint32_t tss_limit = sizeof(tss_t) - 1;
    entries[5] = (tss_limit & 0xFFFF) |
                 ((tss_base & 0xFFFFFFULL) << 16) |
                 (0x89ULL << 40) |
                 ((((uint64_t)tss_limit >> 16) & 0xFULL) << 48) |
                 (((tss_base >> 24) & 0xFFULL) << 56);
    entries[6] = tss_base >> 32;

    ctx->gdtr.limit = (7 * 8) - 1;
    ctx->gdtr.base = (uint64_t)entries;

    /* 2. Setup TSS for this CPU */
    ctx->tss.rsp0 = (uint64_t)ctx->stack_rsp0 + sizeof(ctx->stack_rsp0);
    ctx->tss.ist1 = (uint64_t)ctx->stack_ist1 + sizeof(ctx->stack_ist1);

    /* 3. Load GDT and TR */
    gdt_flush((uint64_t)&ctx->gdtr);
    tss_flush();

    /* 4. Load IDT (Shared) */
    extern idtr_t idtr_shared;
    __asm__ volatile ("lidt %0" : : "m"(idtr_shared));

    /* 5. Initialize LAPIC for IPIs */
    cpu_ipi_transport_init();

    /* 6. Initialize Per-CPU Scheduler State */
    scheduler_cpu_init();
}
