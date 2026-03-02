#ifndef REAPER_IDT_H
#define REAPER_IDT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * IDT Entry (Gate Descriptor) for x86_64
 * 16 bytes per entry.
 */
typedef struct {
    uint16_t isr_low;       // Lower 16 bits of ISR address
    uint16_t kernel_cs;     // Kernel Code Segment Selector
    uint8_t  ist;           // Interrupt Stack Table offset (0 = disabled)
    uint8_t  attributes;    // Type and attributes (P, DPL, Type)
    uint16_t isr_mid;       // Middle 16 bits of ISR address
    uint32_t isr_high;      // Upper 32 bits of ISR address
    uint32_t reserved;      // Reserved, must be 0
} __attribute__((packed)) idt_entry_t;

/*
 * IDT Register (IDTR)
 * Passed to 'lidt' instruction.
 */
typedef struct {
    uint16_t limit;         // Size of IDT - 1
    uint64_t base;          // Base address of IDT
} __attribute__((packed)) idtr_t;

/*
 * CPU State Snapshot (Forensic Context)
 * This structure matches the stack layout created by the common stub
 * in 'interrupts.s'. It captures the exact state of the universe
 * at the moment of the exception.
 */
typedef struct {
    // General Purpose Registers (pushed by common stub)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // Interrupt Info (pushed by ISR stub)
    uint64_t int_no;
    uint64_t err_code;

    // CPU Frame (pushed by hardware)
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) registers_t;

typedef struct {
    uint64_t total_interrupts;
    uint64_t timer_interrupts;
    uint64_t user_faults;
    uint64_t kernel_faults;
    uint64_t page_faults;
    uint64_t gp_faults;
    uint64_t spurious_irq7;
    uint64_t spurious_irq15;
} idt_metrics_t;

// Attribute definitions
#define IDT_TA_INTERRUPT_GATE    0x8E  // P=1, DPL=00, Type=E (Interrupt Gate)
#define IDT_TA_TRAP_GATE         0x8F  // P=1, DPL=00, Type=F (Trap Gate)
#define IDT_TA_USER_IG           0xEE  // P=1, DPL=11, Type=E (User Interrupt Gate)
#define IDT_TA_USER_TG           0xEF  // P=1, DPL=11, Type=F (User Trap Gate)

// Public API
void idt_init(void);
bool idt_self_test(void);
bool idt_get_metrics(idt_metrics_t* out_metrics);
void idt_note_spurious39(void);
void idt_note_spurious47(void);

#endif // REAPER_IDT_H
