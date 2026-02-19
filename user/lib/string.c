#include "../include/reaper.h"

void* memset(void* dest, int c, unsigned long n) {
    unsigned char* ptr = dest;
    while (n-- > 0) *ptr++ = c;
    return dest;
}

void* memcpy(void* dest, const void* src, unsigned long n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n-- > 0) *d++ = *s++;
    return dest;
}

unsigned long strlen(const char* str) {
    unsigned long len = 0;
    while (str[len]) len++;
    return len;
}

/* Minimal itoa for logging */
char* itoa(long value, char* str, int base) {
    char *rc;
    char *ptr;
    char *low;
    // Check for supported base.
    if ( base < 2 || base > 36 ) {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    // Set '-' for negative decimals.
    if ( value < 0 && base == 10 ) {
        *ptr++ = '-';
    }
    // Remember where the numbers start.
    low = ptr;
    // The actual conversion.
    do {
        // Modulo is negative for negative value. This trick makes abs() unnecessary.
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } while ( value );
    // Terminating the string.
    *ptr-- = '\0';
    // Invert the numbers.
    while ( low < ptr ) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}
