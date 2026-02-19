#ifndef REAPER_CPU_H
#define REAPER_CPU_H

#include <stdint.h>
#include <stdbool.h>

struct cpuid_result {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

/*
 * Execute CPUID instruction
 * leaf: The value for EAX
 * subleaf: The value for ECX (used for extended leaves)
 * result: Pointer to structure to store output registers
 */
void cpuid(uint32_t leaf, uint32_t subleaf, struct cpuid_result *result);

/* Check if Process-Context Identifiers (PCID) are supported */
bool cpu_has_pcid(void);

/* Check if INVPCID instruction is supported */
bool cpu_has_invpcid(void);

/* Read/Write Control Registers */
static inline uint64_t read_cr0(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(val) : "memory");
}

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val) : "memory");
}

static inline uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr4" : : "r"(val) : "memory");
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static inline void sti(void) {
    __asm__ volatile ("sti");
}

static inline void cli(void) {
    __asm__ volatile ("cli");
}

static inline uint64_t read_rflags(void) {
    uint64_t rflags;
    __asm__ volatile (
        "pushfq\n\t"
        "popq %0"
        : "=r"(rflags)
        :
        : "memory"
    );
    return rflags;
}

static inline void write_rflags(uint64_t rflags) {
    __asm__ volatile (
        "pushq %0\n\t"
        "popfq"
        :
        : "r"(rflags)
        : "memory", "cc"
    );
}

/* Enable PCID feature if supported */
void cpu_enable_pcid(void);

/* Initialize SSE/FPU/XSAVE extended state */
void cpu_init_extended_state(void);

#define FPU_MODE_NONE    0
#define FPU_MODE_FXSAVE  1
#define FPU_MODE_XSAVE   2

uint8_t cpu_get_fpu_mode(void);

/* INVPCID Descriptor */
struct invpcid_desc {
    uint64_t pcid : 12;
    uint64_t reserved : 52;
    uint64_t addr;
} __attribute__((packed));

#define INVPCID_TYPE_INDIVIDUAL_ADDR 0
#define INVPCID_TYPE_SINGLE_CONTEXT   1
#define INVPCID_TYPE_ALL_CONTEXTS     2
#define INVPCID_TYPE_ALL_NON_GLOBAL   3

/* Execute INVPCID instruction */
void invpcid(uint64_t type, uint16_t pcid, uint64_t addr);

/* XCR0 bits */
#define XCR0_X87         (1 << 0)
#define XCR0_SSE         (1 << 1)
#define XCR0_AVX         (1 << 2)

static inline uint64_t read_xcr0(void) {
    uint32_t lo, hi;
    __asm__ volatile ("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((uint64_t)hi << 32) | lo;
}

static inline void write_xcr0(uint64_t val) {
    uint32_t lo = val & 0xFFFFFFFF;
    uint32_t hi = val >> 32;
    __asm__ volatile ("xsetbv" : : "a"(lo), "d"(hi), "c"(0) : "memory");
}

/**
 * user_mode_jump: The final leap to Ring 3.
 * RIP: Target instruction pointer.
 * RSP: Target stack pointer.
 * NOTE: This function NEVER returns.
 */
void user_mode_jump(uint64_t rip, uint64_t rsp) __attribute__((noreturn));

/* Model Specific Registers (MSRs) */
#define MSR_EFER        0xC0000080
#define MSR_STAR        0xC0000081
#define MSR_LSTAR       0xC0000082
#define MSR_SFMASK      0xC0000084
#define MSR_GS_BASE     0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = val & 0xFFFFFFFF;
    uint32_t hi = val >> 32;
    __asm__ volatile ("wrmsr" : : "a"(lo), "d"(hi), "c"(msr) : "memory");
}

#endif /* REAPER_CPU_H */