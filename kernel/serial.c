#include "include/serial.h"
#include "include/port_io.h" // For inb and outb
#include <stdbool.h> // For bool

#define COM1_PORT 0x3F8 // COM1 base port address

// Checks if the transmit buffer is empty
static bool serial_is_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

// Initializes the serial port (COM1)
void serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Disable interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1_PORT + 1, 0x00); //                  (hi byte)
    outb(COM1_PORT + 3, 0x03); // Disable DLAB, set 8 data bits, 1 stop bit, no parity
    outb(COM1_PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set

    // Loopback test removed for QEMU compatibility
    // If test passed, disable loopback and enable DTR/RTS
    outb(COM1_PORT + 4, 0x0F);
}

// Writes a character to the serial port
void serial_write_char(char c) {
    while (!serial_is_transmit_empty()); // Wait for transmit buffer to be empty
    outb(COM1_PORT, c);
}
