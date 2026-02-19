/* kernel/boot.s */

.set MULTIBOOT2_MAGIC,     0xE85250D6
.set MULTIBOOT2_ARCH,      0     /* i386 (protected mode) - used for both 32 and 64 bit kernels */
.set MULTIBOOT2_HEADER_LEN, multiboot_header_end - multiboot_header

.section .multiboot, "a"
.align 8

multiboot_header:
    /* Multiboot2 header tag (mandatory) */
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT2_ARCH
    .long MULTIBOOT2_HEADER_LEN
    .long -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT2_HEADER_LEN)

    /* End tag */
    .word 0
    .word 0
    .long 8
multiboot_header_end:

.section .data
.align 16
gdt:
    .quad 0x0000000000000000  /* null descriptor */
    .quad 0x0020980000000000  /* 64-bit code descriptor */
gdt_ptr:
    .word . - gdt - 1
    .quad gdt

.section .data
.align 8
multiboot_ptr: .quad 0

.section .bss
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096
pt:
    .skip 4096
stack_bottom:
    .skip 16384
stack_top:

.code32
.section .text
.global _start
.type _start, @function

_start:
    // Early serial output '!' to confirm we started
    mov $0x3F8, %dx
    mov $'!', %al
    out %al, %dx

    // Save the multiboot info pointer from EBX.

    mov $stack_top, %esp

    /* Set up page tables for identity mapping */
    /* PML4[0] -> PDPT */
    mov $pdpt, %eax
    or $3, %eax  /* present, writable */
    mov %eax, pml4

    /* PDPT[0] -> PD */
    mov $pd, %eax
    or $3, %eax
    mov %eax, pdpt

    /* PD[0] -> PT */
    mov $pt, %eax
    or $3, %eax
    mov %eax, pd

    /* PT[0..511] -> 0x0 to 0x200000 (2MB) */
    mov $0, %ecx
    mov $pt, %edi
pt_loop:
    mov %ecx, %eax
    shl $12, %eax  /* *4096 */
    or $3, %eax
    stosl
    inc %ecx
    cmp $512, %ecx
    jl pt_loop

    /* Load CR3 with PML4 */
    mov $pml4, %eax
    mov %eax, %cr3

    /* Enable PAE */
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    /* Enable long mode in EFER */
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    /* Enable paging */
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0

    /* Load GDT */
    lgdt gdt_ptr

    /* Jump to 64-bit code */
    ljmp $0x8, $start64

.code64
start64:
    // Set up 64-bit stack
    mov $stack_top, %rsp

    // Clear BSS section
    mov $bss_start, %rdi
    mov $bss_end, %rcx
    sub %rdi, %rcx
    xor %al, %al
    cld
    rep stosb

    // Pass multiboot info to kernel_main
    mov (multiboot_ptr), %rdi
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang
