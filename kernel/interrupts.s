.section .text
.extern isr_handler
.extern syscall_handler
.extern sys_stack_top
.extern user_rsp_save

/*
 * Macro for exceptions with NO error code.
 * Pushes a dummy 0 so the stack frame is consistent.
 */
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    pushq $0                # Dummy error code
    pushq $\num             # Interrupt number
    jmp idt_common_stub
.endm

/*
 * Macro for exceptions WITH an error code.
 * CPU has already pushed the error code.
 */
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    pushq $\num             # Interrupt number
    jmp idt_common_stub
.endm

/*
 * The Common Gatekeeper Stub
 * 1. Saves all general purpose registers (matching registers_t).
 * 2. Calls the C dispatcher (isr_handler).
 * 3. Restores state (though likely we won't return from a kernel panic).
 */
idt_common_stub:
    # Save GPRs
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Pass pointer to stack (registers_t*) as first argument
    movq %rsp, %rdi

    # Call the C Forensic Dispatcher
    call isr_handler

    # Restore GPRs
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Clean up error code and ISR number (2 * 8 bytes)
    addq $16, %rsp

    # Return from interrupt
    iretq

/*
 * Define the first 32 ISRs
 */
ISR_NOERRCODE 0   # Divide by Zero
ISR_NOERRCODE 1   # Debug
ISR_NOERRCODE 2   # NMI
ISR_NOERRCODE 3   # Breakpoint
ISR_NOERRCODE 4   # Overflow
ISR_NOERRCODE 5   # Bound Range Exceeded
ISR_NOERRCODE 6   # Invalid Opcode
ISR_NOERRCODE 7   # Device Not Available
ISR_ERRCODE   8   # Double Fault
ISR_NOERRCODE 9   # Coprocessor Segment Overrun
ISR_ERRCODE   10  # Invalid TSS
ISR_ERRCODE   11  # Segment Not Present
ISR_ERRCODE   12  # Stack-Segment Fault
ISR_ERRCODE   13  # General Protection Fault
ISR_ERRCODE   14  # Page Fault
ISR_NOERRCODE 15  # Reserved
ISR_NOERRCODE 16  # x87 Floating-Point Exception
ISR_ERRCODE   17  # Alignment Check
ISR_NOERRCODE 18  # Machine Check
ISR_NOERRCODE 19  # SIMD Floating-Point Exception
ISR_NOERRCODE 20  # Virtualization Exception
ISR_ERRCODE   21  # Control Protection Exception
ISR_NOERRCODE 22  # Reserved
ISR_NOERRCODE 23  # Reserved
ISR_NOERRCODE 24  # Reserved
ISR_NOERRCODE 25  # Reserved
ISR_NOERRCODE 26  # Reserved
ISR_NOERRCODE 27  # Reserved
ISR_NOERRCODE 28  # Hypervisor Injection Exception
ISR_ERRCODE   29  # VMM Communication Exception
ISR_ERRCODE   30  # Security Exception
ISR_NOERRCODE 31  # Reserved

/*
 * Global Table of ISR Stubs
 * This allows C code to loop through and install them easily.
 */
.section .rodata
.global isr_stub_table
isr_stub_table:
    .quad isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    .quad isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    .quad isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    .quad isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

.section .text

/*
 * gdt_flush(uint64_t gdtr_ptr)
 * Reloads GDT and segments.
 */
.global gdt_flush
gdt_flush:
    lgdt (%rdi)              # Load GDT from pointer passed in RDI

    # Reload Segment Registers
    mov $0x10, %ax           # 0x10 is Kernel Data Segment
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Far Jump to reload CS (0x08 is Kernel Code Segment)
    pushq $0x08              # Push CS
    leaq .reload_cs(%rip), %rax
    pushq %rax               # Push Return Address
    lretq                    # Far Return (pops RIP and CS)

.reload_cs:
    ret

/*
 * tss_flush()
 * Loads the Task Register (TR) with selector 0x28.
 */
.global tss_flush
tss_flush:
    mov $0x28, %ax           # TSS Selector index 5 (5 * 8 = 40 = 0x28)
    ltr %ax
    ret

/*
 * syscall_entry
 * Target of MSR_LSTAR.
 */
.global syscall_entry
.extern syscall_dispatcher
syscall_entry:
    # 1. Switch to kernel stack
    swapgs                  # GS base now points to kernel state
    movq %rsp, %gs:0        # Save user stack
    movq %gs:8, %rsp        # Load kernel stack

    # 2. Save user state (needed for SYSRET)
    movq %rcx, %gs:16       # Save user RIP (RCX)
    movq %r11, %gs:24       # Save user RFLAGS (R11)
    
    # 3. Align registers for C Dispatcher (Syscall Convention -> C ABI)
    # Syscall: RAX=num, RDI=a0, RSI=a1, RDX=a2, R10=a3, R8=a4, R9=a5
    # C ABI:   RDI=num, RSI=a0, RDX=a1, RCX=a2, R8=a3, R9=a4
    # Note: We currently support up to a4 (5 args) in registers.
    
    movq %r8, %r9           # a4 -> arg5 (R9)
    movq %r10, %r8          # a3 -> arg4 (R8)
    movq %rdx, %rcx         # a2 -> arg3 (RCX)
    movq %rsi, %rdx         # a1 -> arg2 (RDX)
    movq %rdi, %rsi         # a0 -> arg1 (RSI)
    movq %rax, %rdi         # num -> arg0 (RDI)

    call syscall_dispatcher

    # 4. Zero-Residue: Clear scratch registers to prevent leakage
    # We DO NOT clear callee-saved registers (RBX, RBP, R12-R15) 
    # because the user-space program relies on them.
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    xorq %rdi, %rdi
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10

    # Scrub XMM registers (0-15)
    pxor %xmm0, %xmm0
    pxor %xmm1, %xmm1
    pxor %xmm2, %xmm2
    pxor %xmm3, %xmm3
    pxor %xmm4, %xmm4
    pxor %xmm5, %xmm5
    pxor %xmm6, %xmm6
    pxor %xmm7, %xmm7
    pxor %xmm8, %xmm8
    pxor %xmm9, %xmm9
    pxor %xmm10, %xmm10
    pxor %xmm11, %xmm11
    pxor %xmm12, %xmm12
    pxor %xmm13, %xmm13
    pxor %xmm14, %xmm14
    pxor %xmm15, %xmm15

    # Restore RCX/R11 from GS
    movq %gs:16, %rcx
    movq %gs:24, %r11

    # 5. Restore user stack and return
    movq %gs:0, %rsp        # Restore user stack
    swapgs                  # GS base back to user
    sysretq

/*
 * context_switch(uint64_t* old_rsp, uint64_t new_rsp, void* old_ext, void* new_ext, uint8_t fpu_mode)
 * RDI: Pointer to the RSP of the old thread.
 * RSI: The RSP of the new thread.
 * RDX: Pointer to the extended state of the old thread.
 * RCX: Pointer to the extended state of the new thread.
 * R8:  FPU mode (0=NONE, 1=FXSAVE, 2=XSAVE)
 */
.global context_switch
context_switch:
    # 1. Save old context
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushfq                  # Save RFLAGS

    # 2. Save FPU/SSE State
    cmpb $1, %r8b            # FPU_MODE_FXSAVE
    jne .check_xsave_save
    fxsave64 (%rdx)
    jmp .swap_stacks

.check_xsave_save:
    cmpb $2, %r8b            # FPU_MODE_XSAVE
    jne .swap_stacks
    movq %rdx, %r11          # Preserve pointer
    mov $0xFFFFFFFF, %eax   # Save all features
    mov $0xFFFFFFFF, %edx
    xsave64 (%r11)

.swap_stacks:
    # 3. Swap stacks
    movq %rsp, (%rdi)       # Save old RSP
    movq %rsi, %rsp         # Load new RSP

    # 4. Restore FPU/SSE State
    cmpb $1, %r8b
    jne .check_xsave_restore
    fxrstor64 (%rcx)
    jmp .restore_gprs

.check_xsave_restore:
    cmpb $2, %r8b
    jne .restore_gprs
    movq %rcx, %r11          # Preserve pointer
    mov $0xFFFFFFFF, %eax
    mov $0xFFFFFFFF, %edx
    xrstor64 (%r11)

.restore_gprs:
    # 5. Restore new context
    popfq                   # Restore RFLAGS
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx
    
    # 6. Return
    ret

/*
 * user_mode_jump(uint64_t rip, uint64_t rsp)
 * RDI: The target Instruction Pointer (User Mode).
 * RSI: The target Stack Pointer (User Mode).
 */
.global user_mode_jump
user_mode_jump:
    # 1. Prepare the IRETQ frame
    # Stack layout: SS, RSP, RFLAGS, CS, RIP
    pushq $0x1B             # SS: User Data Segment (Index 3, RPL 3)
    pushq %rsi              # RSP: User Stack
    pushq $0x202            # RFLAGS: IF=1, Reserved=1
    pushq $0x23             # CS: User Code Segment (Index 4, RPL 3)
    pushq %rdi              # RIP: User Entry Point

    # 2. Zero out general purpose registers (Zero-Residue)
    # This prevents kernel data from leaking into user-space.
    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    xorq %rdi, %rdi
    xorq %rbp, %rbp
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r11, %r11
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15

    # Scrub XMM registers (0-15)
    pxor %xmm0, %xmm0
    pxor %xmm1, %xmm1
    pxor %xmm2, %xmm2
    pxor %xmm3, %xmm3
    pxor %xmm4, %xmm4
    pxor %xmm5, %xmm5
    pxor %xmm6, %xmm6
    pxor %xmm7, %xmm7
    pxor %xmm8, %xmm8
    pxor %xmm9, %xmm9
    pxor %xmm10, %xmm10
    pxor %xmm11, %xmm11
    pxor %xmm12, %xmm12
    pxor %xmm13, %xmm13
    pxor %xmm14, %xmm14
    pxor %xmm15, %xmm15

    # 3. Execute the leap to Ring 3
    iretq

/*
 * timer_stub
 * Handles IRQ 0 (Timer).
 */
.global timer_stub
.extern timer_handler
timer_stub:
    # We must save scratch registers because C ABI says they can change
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    # Acknowledge PIC IMMEDIATELY before we potentially switch stacks
    # (0x20 is EOI for master PIC)
    movb $0x20, %al
    outb %al, $0x20

    call timer_handler

    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    iretq

/*
 * isr_spurious_39 (IRQ 7)
 */
.global isr_spurious_39
isr_spurious_39:
    pushq %rax
    movb $0x0B, %al         # OCW3: Read ISR
    outb %al, $0x20
    inb $0x20, %al
    testb $0x80, %al        # Check bit 7
    jz .spurious_39_done    # If clear, it's spurious
    movb $0x20, %al         # Real: Send EOI
    outb %al, $0x20
.spurious_39_done:
    popq %rax
    iretq

/*
 * isr_spurious_47 (IRQ 15)
 */
.global isr_spurious_47
isr_spurious_47:
    pushq %rax
    movb $0x0B, %al         # OCW3: Read ISR
    outb %al, $0xA0
    inb $0xA0, %al
    testb $0x80, %al        # Check bit 7
    jz .spurious_47_master  # If clear, it's spurious, but must ACK master
    movb $0x20, %al         # Real: Send EOI to both
    outb %al, $0xA0
.spurious_47_master:
    movb $0x20, %al
    outb %al, $0x20
    popq %rax
    iretq

.section .note.GNU-stack,"",@progbits

