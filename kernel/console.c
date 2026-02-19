#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "include/init.h"
#include "include/console.h"
#include "include/serial.h" // Include serial driver header
#include "include/scheduler.h" // For kpanic context
#include "include/thread.h"    // For thread/process structs

// --- Private VGA Driver State ---
// static volatile uint8_t* vga_buffer = (volatile uint8_t*)0xB8000; // Unused in serial-only mode
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

// --- Private Helper Functions ---

static void console_putentryat(char c, uint8_t color, size_t x, size_t y) {
    /* VGA buffer not available in Limine graphical mode */
    (void)c; (void)color; (void)x; (void)y;
}

static void console_scroll(void) {
    terminal_row = 24;
}

// --- Public Console API ---

void console_clear(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = 0x07; // Light grey on black
}

void console_putc(char c) {
    // 1. Write to Legacy VGA (Backup/stub)
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else {
        console_putentryat(c, terminal_color, terminal_column, terminal_row);
        terminal_column++;
    }

    if (terminal_column >= 80) {
        terminal_column = 0;
        terminal_row++;
    }

    if (terminal_row >= 25) {
        console_scroll();
    }

    // 2. Write to Serial Port (Logging/Debugging)
    serial_write_char(c);
}

void console_write_string(const char* str) {
    // Fallback loop for serial and VGA state tracking
    for (size_t i = 0; str[i] != '\0'; i++) {
        char c = str[i];
        
        // VGA State tracking logic (copied from putc)
        if (c == '\n') {
            terminal_column = 0;
            terminal_row++;
        } else {
            console_putentryat(c, terminal_color, terminal_column, terminal_row);
            terminal_column++;
        }
        if (terminal_column >= 80) { terminal_column = 0; terminal_row++; } 
        if (terminal_row >= 25) { console_scroll(); }
        
        serial_write_char(c);
    }
}

// --- kprintf Implementation ---

static void print_number(unsigned long long n, unsigned int base, int is_signed) {
    char buf[64];
    int i = 0;
    int is_neg = 0;

    if (is_signed && (long long)n < 0) {
        is_neg = 1;
        n = (unsigned long long)(-(long long)n);
    }

    if (n == 0) {
        buf[i++] = '0';
    } else {
        while (n > 0) {
            int digit = n % base;
            buf[i++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'a');
            n /= base;
        }
    }

    if (is_neg) {
        buf[i++] = '-';
    }

    // Print buffer in reverse
    for (int j = i - 1; j >= 0; j--) {
        console_putc(buf[j]);
    }
}

void kvprintf(const char *fmt, va_list args) {
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            console_putc(fmt[i]);
            continue;
        }

        i++; // Move past the '%'

        int is_long = 0;
        if (fmt[i] == 'l') {
            is_long = 1;
            i++;
        }

        switch (fmt[i]) {
            case 'c':
                console_putc((char)va_arg(args, int));
                break;
            case 's':
                console_write_string(va_arg(args, char*));
                break;
            case 'd':
            case 'i':
                if (is_long)
                    print_number(va_arg(args, long long), 10, 1);
                else
                    print_number((unsigned long long)(long long)va_arg(args, int), 10, 1);
                break;
            case 'u':
                if (is_long)
                    print_number(va_arg(args, unsigned long long), 10, 0);
                else
                    print_number((unsigned long long)va_arg(args, unsigned int), 10, 0);
                break;
            case 'x':
                if (is_long)
                    print_number(va_arg(args, unsigned long long), 16, 0);
                else
                    print_number((unsigned long long)va_arg(args, unsigned int), 16, 0);
                break;
            case 'p':
                console_write_string("0x");
                print_number(va_arg(args, unsigned long long), 16, 0);
                break;
            case '%':
                console_putc('%');
                break;
            default:
                console_putc('%');
                if (is_long) console_putc('l');
                console_putc(fmt[i]);
                break;
        }
    }
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);
}

void kpanic(const char *fmt, ...) {
    __asm__ volatile ("cli");

    kprintf("\n\033[31;1m!!! KERNEL PANIC !!!\033[0m\n");
    kprintf("REASON: ");
    
    va_list args;
    va_start(args, fmt);
    kvprintf(fmt, args);
    va_end(args);

    /* Attempt to dump context */
    thread_t* current = scheduler_get_current();
    if (current) {
        kprintf("\nCONTEXT: Thread %u (Process %u)\n", 
            current->tid, 
            (current->owner ? current->owner->pid : (uint32_t)-1));
        kprintf("State: %d, Quantum: %d\n", current->state, current->ticks_remaining);
    } else {
        kprintf("\nCONTEXT: Early Boot (No Scheduler)\n");
    }

    kprintf("\n\nSystem Halted. Please investigate the forensic logs.\n");

    while(1) {
        __asm__ volatile ("hlt");
    }
}

void kpanic_at(const char* file, int line, const char* msg) {
    __asm__ volatile ("cli");
    kprintf("\n\033[31;1m!!! KERNEL PANIC !!!\033[0m\n");
    kprintf("AT: %s:%d\n", file, line);
    kprintf("REASON: %s\n", msg);
    
    /* Attempt to dump context */
    thread_t* current = scheduler_get_current();
    if (current) {
        kprintf("CONTEXT: Thread %u (Process %u)\n", 
            current->tid, 
            (current->owner ? current->owner->pid : (uint32_t)-1));
    }

    kprintf("\nSystem Halted. Fate has been sealed.\n");
    while(1) __asm__ volatile ("hlt");
}

// --- Initcall Registration ---

void console_init(void) {
    serial_init(); // Initialize serial port first
    console_clear();
    kprintf("Reaper-OS: Console initialized.\n");
    kprintf("Reaper-OS: Serial port initialized (COM1).\n");
}
