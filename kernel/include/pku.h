#ifndef PKU_H
#define PKU_H

#include <stdint.h>

/* Reality PKU Keys (Colours) */
#define PKU_KEY_CASUAL    1
#define PKU_KEY_SECURE    2
#define PKU_KEY_LOCKDOWN  3
#define PKU_KEY_GHOST     4
#define PKU_KEY_GUARD     15 /* Restricted key for stack guards */

/* PKRU Register Helpers */
static inline uint32_t pkru_read(void) {
    uint32_t eax, edx;
    __asm__ volatile ("rdpkru" : "=a"(eax), "=d"(edx) : "c"(0));
    return eax;
}

static inline void pkru_write(uint32_t pkru) {
    __asm__ volatile ("wrpkru" : : "a"(pkru), "c"(0), "d"(0));
}

void pkru_init(void);
void pkru_set_reality(uint8_t reality_mode);
uint32_t pkru_for_reality(uint8_t reality_mode);

#endif
