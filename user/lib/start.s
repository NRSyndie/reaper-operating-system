.intel_syntax noprefix
.global _start
.extern main
.extern sys_exit

.section .text
_start:
    /* Stack is already set up by the kernel at 0x7FFFF0 or similar */
    /* Call main(argc, argv) - currently 0, NULL */
    xor rdi, rdi
    xor rsi, rsi
    call main

    /* Call sys_exit with return value */
    mov rdi, rax
    call sys_exit

    /* Should not return */
    jmp .

.section .note.GNU-stack,"",@progbits
