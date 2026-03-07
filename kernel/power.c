#include "include/power.h"
#include "include/port_io.h"

#include <stdint.h>

static void kbc_wait_input_clear(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(0x64) & 0x02u) == 0) {
            return;
        }
    }
}

__attribute__((noreturn)) void system_reboot_now(void) {
    __asm__ volatile ("cli");

    /* Legacy keyboard controller reset pulse (works in QEMU/BIOS paths). */
    kbc_wait_input_clear();
    outb(0x64, 0xFE);

    /* Fast reset control register fallback. */
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);

    /* Final fallback: trigger triple fault via null IDT. */
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idtr = {0, 0};
    __asm__ volatile ("lidt %0" : : "m"(null_idtr));
    __asm__ volatile ("int3");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}

__attribute__((noreturn)) void system_halt_now(void) {
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
