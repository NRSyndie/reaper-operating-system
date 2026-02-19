#include "include/cpu.h"
#include "include/console.h"
#include "include/klog.h"

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

bool cpu_has_invpcid(void) {
    struct cpuid_result res;
    cpuid(0x07, 0, &res);
    // INVPCID is bit 10 of EBX
    return (res.ebx & (1 << 10)) != 0;
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
    /* SMP scaffolding: BSP only for now. */
    return 0;
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
