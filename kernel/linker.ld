ENTRY(kernel_main)

PHDRS
{
    text    PT_LOAD    FLAGS(5); /* RX */
    rodata  PT_LOAD    FLAGS(4); /* R */
    data    PT_LOAD    FLAGS(6); /* RW */
}

SECTIONS
{
    /* We want to be placed in the high half of memory (VMA) */
    . = 0xffffffff80000000;
    kernel_start = .;
    
    /* Physical offset (LMA starts at 2MB) */
    kernel_phys_offset = . - 0x200000;

    .text : AT(ADDR(.text) - kernel_phys_offset) {
        *(.text .text.*)
    } :text

    . = ALIGN(0x1000);
    .rodata : AT(ADDR(.rodata) - kernel_phys_offset) {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(0x1000);
    .data : AT(ADDR(.data) - kernel_phys_offset) {
        *(.data .data.*)
    } :data

    . = ALIGN(0x1000);
    /* Limine requests section */
    .requests : AT(ADDR(.requests) - kernel_phys_offset) {
        KEEP(*(.requests))
    } :data

    . = ALIGN(0x1000);
    .bss : AT(ADDR(.bss) - kernel_phys_offset) {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    . = ALIGN(0x1000);
    /* INITCALL SECTION */
    .initcall : AT(ADDR(.initcall) - kernel_phys_offset)
    {
        initcall_start = .;
        KEEP(*(.initcall))
        initcall_end = .;
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }

    . = ALIGN(4096);
    kernel_end = .;
}
