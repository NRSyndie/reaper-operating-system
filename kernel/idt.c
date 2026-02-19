#include <stdint.h>
#include <idt.h>
#include <gdt.h>
#include <console.h>
#include <cpu.h>
#include <mode.h>
#include <utils.h>

/*
 * The IDT is an array of 256 16-byte entries.
 * It must be aligned to 16 bytes for performance/correctness.
 */
__attribute__((aligned(0x10)))
static idt_entry_t idt[256];

static idtr_t idtr;
static spinlock_t idt_metrics_lock = 0;
static idt_metrics_t idt_metrics = {0};

/* Defined in interrupts.s */
extern uint64_t isr_stub_table[32];

static const char *exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

/* 
 * 64-bit Kernel Code Segment. 
 */
#define KERNEL_CS_SELECTOR GDT_KERNEL_CODE

/*
 * Sets a single IDT entry.
 */
static void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags, uint8_t ist) {
    idt[num].isr_low    = (base & 0xFFFF);
    idt[num].kernel_cs  = sel;
    idt[num].ist        = ist & 0x7;
    idt[num].attributes = flags;
    idt[num].isr_mid    = (base >> 16) & 0xFFFF;
    idt[num].isr_high   = (base >> 32) & 0xFFFFFFFF;
    idt[num].reserved   = 0;
}

static inline uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(val));
    return val;
}

#include <scheduler.h>
#include <thread.h>
#include <port_io.h>
#include <klog.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define ICW1_INIT    0x11
#define ICW4_8086    0x01

static void pic_remap(void) {
    uint8_t a1, a2;

    
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT);
    outb(PIC2_COMMAND, ICW1_INIT);
    outb(PIC1_DATA, 0x20); // Remap Master PIC to 0x20-0x27
    outb(PIC2_DATA, 0x28); // Remap Slave PIC to 0x28-0x2F
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);

    /* Unmask IRQ 0 (Timer) */
    uint8_t mask = inb(PIC1_DATA) & ~0x01;
    outb(PIC1_DATA, mask);
    }

extern void timer_stub(void);
extern void isr_spurious_39(void);
extern void isr_spurious_47(void);
void idt_note_spurious39(void);
void idt_note_spurious47(void);

void idt_init(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    // Install the first 32 ISRs
    for (int i = 0; i < 32; i++) {
        uint8_t attrs = IDT_TA_INTERRUPT_GATE;
        uint8_t ist = 0;

        if (i == 1 || i == 4) attrs = IDT_TA_TRAP_GATE;
        if (i == 3) attrs = IDT_TA_USER_TG;
        if (i == 2 || i == 8 || i == 18) ist = 1;

        idt_set_gate((uint8_t)i, isr_stub_table[i], KERNEL_CS_SELECTOR, attrs, ist);
    }

    // Remap PIC and install Timer (IRQ 0 -> Vector 32)
    pic_remap();
    idt_set_gate(32, (uint64_t)timer_stub, KERNEL_CS_SELECTOR, IDT_TA_INTERRUPT_GATE, 0);

    /* Install Spurious Handlers */
    idt_set_gate(39, (uint64_t)isr_spurious_39, KERNEL_CS_SELECTOR, IDT_TA_INTERRUPT_GATE, 0);
    idt_set_gate(47, (uint64_t)isr_spurious_47, KERNEL_CS_SELECTOR, IDT_TA_INTERRUPT_GATE, 0);

    __asm__ volatile ("lidt %0" : : "m"(idtr));
    
    }

bool idt_get_metrics(idt_metrics_t* out_metrics) {
    if (!out_metrics) return false;
    uint64_t flags = spinlock_irqsave(&idt_metrics_lock);
    memcpy(out_metrics, &idt_metrics, sizeof(idt_metrics_t));
    spinlock_irqrestore(&idt_metrics_lock, flags);
    return true;
}

bool idt_self_test(void) {
    if (idtr.base != (uint64_t)&idt) return false;
    if (idtr.limit != (sizeof(idt) - 1)) return false;
    if ((idt[8].ist & 0x7) != 1) return false;
    if (idt[1].attributes != IDT_TA_TRAP_GATE) return false;
    if (idt[3].attributes != IDT_TA_USER_TG) return false;
    if (idt[32].attributes != IDT_TA_INTERRUPT_GATE) return false;
    return true;
}

/*
 * The Central Forensic Dispatcher.
 * Called by idt_common_stub in interrupts.s
 */
void isr_handler(registers_t *regs) {
    __atomic_fetch_add(&idt_metrics.total_interrupts, 1, __ATOMIC_RELAXED);

    // If it's a hardware interrupt (timer), handle it and return
    if (regs->int_no == 32) {
        __atomic_fetch_add(&idt_metrics.timer_interrupts, 1, __ATOMIC_RELAXED);
        // Handled via timer_stub -> timer_handler directly
        return;
    }

    // Check privilege level: CS bits 0-1 indicate CPL
    bool is_user = (regs->cs & 3) == 3;

    if (is_user) {
        __atomic_fetch_add(&idt_metrics.user_faults, 1, __ATOMIC_RELAXED);
        // USER MODE EXCEPTION: Graceful termination
        
        if (regs->int_no == 14) {
            __atomic_fetch_add(&idt_metrics.page_faults, 1, __ATOMIC_RELAXED);
            uint64_t cr2 = read_cr2();
            mode_log_fault_event((uint8_t)regs->int_no, regs->err_code, regs->rip,
                                 cr2, regs->rsp, regs->cs, regs->rflags, true);
            
            extern bool lattice_handle_fault(uint64_t vaddr, uint64_t error_code);
            if (lattice_handle_fault(cr2, regs->err_code)) {
                /* VOID WALL or PRISM MATERIALIZATION SUCCESSful. 
                 * Returning from the interrupt will retry the faulting instruction.
                 */
                return;
            }

            kprintf("[USER-FAULT] Page Fault at 0x%lx (RIP: 0x%lx, Error: 0x%lx)\n", cr2, regs->rip, regs->err_code);
        } else if (regs->int_no == 13) {
            __atomic_fetch_add(&idt_metrics.gp_faults, 1, __ATOMIC_RELAXED);
            mode_log_fault_event((uint8_t)regs->int_no, regs->err_code, regs->rip,
                                 0, regs->rsp, regs->cs, regs->rflags, true);
            kprintf("[USER-FAULT] Exception %d (RIP: 0x%lx, Error: 0x%lx)\n", regs->int_no, regs->rip, regs->err_code);
        } else {
            kprintf("[USER-FAULT] Exception %d (RIP: 0x%lx, Error: 0x%lx)\n", regs->int_no, regs->rip, regs->err_code);
        }

        thread_exit(); // Never returns
    }

    // KERNEL MODE EXCEPTION: Fatal Panic
    __atomic_fetch_add(&idt_metrics.kernel_faults, 1, __ATOMIC_RELAXED);
    __asm__ volatile ("cli");

    kprintf("\n");
    kprintf("================================================================================\n");
    kprintf("                            FATAL KERNEL EXCEPTION                              \n");
    kprintf("================================================================================\n");

    if (regs->int_no < 32) {
        if (regs->int_no == 13 || regs->int_no == 14) {
            uint64_t cr2 = (regs->int_no == 14) ? read_cr2() : 0;
            mode_log_fault_event((uint8_t)regs->int_no, regs->err_code, regs->rip,
                                 cr2, regs->rsp, regs->cs, regs->rflags, false);
        }
        kprintf("EXCEPTION: %s (Vector: %d)\n", exception_messages[regs->int_no], regs->int_no);
    } else {
        kprintf("EXCEPTION: Unknown (Vector: %d)\n", regs->int_no);
    }

    kprintf("ERROR CODE: 0x%lx\n", regs->err_code);

    // Special handling for Page Faults
    if (regs->int_no == 14) {
        __atomic_fetch_add(&idt_metrics.page_faults, 1, __ATOMIC_RELAXED);
        uint64_t cr2 = read_cr2();
        kprintf("FAULT ADDR: 0x%lx (CR2)\n", cr2);
        
        kprintf("PF FLAGS: ");
        if (regs->err_code & 1) kprintf("PRESENT "); else kprintf("NOT-PRESENT ");
        if (regs->err_code & 2) kprintf("WRITE "); else kprintf("READ ");
        if (regs->err_code & 4) kprintf("USER "); else kprintf("KERNEL ");
        if (regs->err_code & 8) kprintf("RESERVED-BIT ");
        if (regs->err_code & 16) kprintf("INSTRUCTION-FETCH ");
        kprintf("\n");
    }

    kprintf("\n--- FORENSIC CONTEXT ---\n");
    mode_id_t current_mode = mode_get_current();
    kprintf("REALITY:    %s\n", mode_get_name(current_mode));
    kprintf("RIP:        0x%lx\n", regs->rip);
    kprintf("CS:         0x%lx\n", regs->cs);
    kprintf("RFLAGS:     0x%lx\n", regs->rflags);
    kprintf("RSP:        0x%lx\n", regs->rsp);
    kprintf("SS:         0x%lx\n", regs->ss);

    kprintf("\n--- GENERAL REGISTERS ---\n");
    kprintf("RAX: 0x%lx  RBX: 0x%lx  RCX: 0x%lx\n", regs->rax, regs->rbx, regs->rcx);
    kprintf("RDX: 0x%lx  RSI: 0x%lx  RDI: 0x%lx\n", regs->rdx, regs->rsi, regs->rdi);
    kprintf("RBP: 0x%lx  R8:  0x%lx  R9:  0x%lx\n", regs->rbp, regs->r8, regs->r9);
    kprintf("R10: 0x%lx  R11: 0x%lx  R12: 0x%lx\n", regs->r10, regs->r11, regs->r12);
    kprintf("R13: 0x%lx  R14: 0x%lx  R15: 0x%lx\n", regs->r13, regs->r14, regs->r15);

    kprintf("================================================================================\n");
    kprintf("System Halted. Fate has been sealed.\n");

    // Infinite Halt Loop
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void idt_note_spurious39(void) {
    __atomic_fetch_add(&idt_metrics.spurious_irq7, 1, __ATOMIC_RELAXED);
}

void idt_note_spurious47(void) {
    __atomic_fetch_add(&idt_metrics.spurious_irq15, 1, __ATOMIC_RELAXED);
}
