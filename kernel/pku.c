#include "include/pku.h"
#include "include/cpu.h"
#include "include/klog.h"
#include "include/mode.h"

void pkru_init(void) {
    if (!cpu_has_pku()) {
        klog(0, "PKU: Not supported by hardware\n");
        return;
    }

    /* Enable PKU in CR4 */
    uint64_t cr4 = read_cr4();
    cr4 |= (1 << 22);
    write_cr4(cr4);

    klog(0, "PKU: Initialized\n");
}

uint32_t pkru_for_reality(uint8_t reality_mode) {
    /* 
     * PKRU register format: 2 bits per key. 
     * 00: Access allowed, 01: Write disable, 10: Access disable, 11: Access+Write disable.
     * We grant access to the specific key for the mode, and disable all others.
     */
    uint32_t key = 0;
    switch (reality_mode) {
        case MODE_CASUAL:  key = PKU_KEY_CASUAL; break;
        case MODE_SECURE:  key = PKU_KEY_SECURE; break;
        case MODE_LOCKDOWN: key = PKU_KEY_LOCKDOWN; break;
        case MODE_GHOST:   key = PKU_KEY_GHOST; break;
        default: return 0xFFFFFFFF; /* Disable everything */
    }

    uint32_t pkru = 0;
    /* Disable all keys by default (set to 11 = 0x3) */
    for (int i = 0; i < 16; i++) {
        pkru |= (0x3 << (2 * i));
    }
    /* Enable only the reality key (set to 00 = 0x0) */
    pkru &= ~(0x3 << (2 * key));
    
    return pkru;
}

void pkru_set_reality(uint8_t reality_mode) {
    if (!cpu_has_pku()) return;
    pkru_write(pkru_for_reality(reality_mode));
}
