#include "include/pku.h"
#include "include/cpu.h"
#include "include/klog.h"
#include "include/mode.h"
#include "include/vmm.h"
#include "include/pmm.h"
#include <console.h>

void pkru_init(void) {
    if (!cpu_has_pku()) {
        klog(0, "PKU: Not supported by hardware\n");
        return;
    }

    /* Enable PKU in CR4 */
    uint64_t cr4 = read_cr4();
    cr4 |= (1 << 22);
    write_cr4(cr4);

    /* Leave PKRU at default 0x0 (all keys enabled) until pages are tagged
     * with protection keys via pkru_assign_key. Writing a restrictive PKRU
     * before pages are tagged would block access to all default-keyed pages. */

    kprintf("PKU: Initialized (PKRU = 0x%x)\n", (unsigned)pkru_read());
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

/* PKU enforcement is only active after pages have been tagged with protection keys.
 * Before that, PKRU stays at 0x0 (all keys enabled) to avoid blocking default-keyed pages. */
static bool pkru_enforcement_active = false;

void pkru_set_reality(uint8_t reality_mode) {
    if (!cpu_has_pku()) return;
    if (!pkru_enforcement_active) return;
    pkru_write(pkru_for_reality(reality_mode));
}

bool pkru_assign_key(uint64_t pml4_phys, uint64_t virt_start, uint64_t length, uint8_t key) {
    if (!cpu_has_pku()) return false;
    if (key > 15) return false;

    uint64_t aligned_start = virt_start & ~(PAGE_SIZE - 1);
    uint64_t aligned_end = (virt_start + length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    for (uint64_t v = aligned_start; v < aligned_end; v += PAGE_SIZE) {
        uint64_t pml4_idx = (v >> 39) & 0x1FF;
        uint64_t pdpt_idx = (v >> 30) & 0x1FF;
        uint64_t pd_idx   = (v >> 21) & 0x1FF;
        uint64_t pt_idx   = (v >> 12) & 0x1FF;

        uint64_t pml4_entry = *(uint64_t*)pmm_phys_to_virt(pml4_phys + pml4_idx * 8);
        if (!(pml4_entry & VMM_PRESENT)) continue;

        uint64_t pdpt_entry = *(uint64_t*)pmm_phys_to_virt((pml4_entry & ~0xFFFULL) + pdpt_idx * 8);
        if (!(pdpt_entry & VMM_PRESENT)) continue;

        uint64_t pd_entry = *(uint64_t*)pmm_phys_to_virt((pdpt_entry & ~0xFFFULL) + pd_idx * 8);
        if (!(pd_entry & VMM_PRESENT)) continue;

        /* 2MB huge page */
        if (pd_entry & VMM_HUGE) {
            uint64_t pd_pa = (pdpt_entry & ~0xFFFULL) + pd_idx * 8;
            uint64_t* pd_ptr = (uint64_t*)pmm_phys_to_virt(pd_pa);
            *pd_ptr = (*pd_ptr & ~((0xFULL << VMM_PK_SHIFT))) | VMM_PK(key);
            cpu_tlb_shootdown_page(v);
            continue;
        }

        uint64_t pt_entry = *(uint64_t*)pmm_phys_to_virt((pd_entry & ~0xFFFULL) + pt_idx * 8);
        if (!(pt_entry & VMM_PRESENT)) continue;

        /* 4KB leaf page */
        uint64_t pt_pa = (pd_entry & ~0xFFFULL) + pt_idx * 8;
        uint64_t* pt_ptr = (uint64_t*)pmm_phys_to_virt(pt_pa);
        *pt_ptr = (*pt_ptr & ~((0xFULL << VMM_PK_SHIFT))) | VMM_PK(key);
        cpu_tlb_shootdown_page(v);
    }

    return true;
}

bool pku_self_test(void) {
    if (!cpu_has_pku()) {
        kprintf("[PKU] unavailable: skipped\n");
        return true;
    }

    /* 1. wrpkru/rdpkru round-trip: save, write, readback, restore */
    uint32_t saved = pkru_read();
    uint32_t test_val = 0x55555555;
    pkru_write(test_val);
    if (pkru_read() != test_val) {
        kprintf("[PKU-FAIL] wrpkru/rdpkru round-trip failed: wrote 0x%x read 0x%x\n",
                (unsigned)test_val, (unsigned)pkru_read());
        pkru_write(saved);
        return false;
    }
    pkru_write(saved);
    if (pkru_read() != saved) {
        kprintf("[PKU-FAIL] PKRU restore failed\n");
        return false;
    }

    /* 3. pkru_for_reality returns correct bit patterns for each mode */
    struct {
        uint8_t mode;
        uint8_t key;
        uint32_t expected;
    } cases[] = {
        { MODE_CASUAL,   PKU_KEY_CASUAL,   ~(0x3u << (2 * PKU_KEY_CASUAL))   },
        { MODE_SECURE,   PKU_KEY_SECURE,   ~(0x3u << (2 * PKU_KEY_SECURE))   },
        { MODE_LOCKDOWN, PKU_KEY_LOCKDOWN, ~(0x3u << (2 * PKU_KEY_LOCKDOWN)) },
        { MODE_GHOST,    PKU_KEY_GHOST,    ~(0x3u << (2 * PKU_KEY_GHOST))    },
    };
    for (int i = 0; i < 4; i++) {
        uint32_t got = pkru_for_reality(cases[i].mode);
        if (got != cases[i].expected) {
            kprintf("[PKU-FAIL] pkru_for_reality(%u) = 0x%x, expected 0x%x (key %u)\n",
                    (unsigned)cases[i].mode, (unsigned)got,
                    (unsigned)cases[i].expected, (unsigned)cases[i].key);
            return false;
        }
    }

    /* 4. Verify each mode's PKRU actually isolates the correct key */
    pkru_write(pkru_for_reality(MODE_CASUAL));
    uint32_t casual_pkru = pkru_read();
    if ((casual_pkru & (0x3u << (2 * PKU_KEY_CASUAL))) != 0) {
        kprintf("[PKU-FAIL] Casual PKRU does not enable key %u: 0x%x\n",
                (unsigned)PKU_KEY_CASUAL, (unsigned)casual_pkru);
        pkru_write(saved);
        return false;
    }

    pkru_write(pkru_for_reality(MODE_SECURE));
    uint32_t secure_pkru = pkru_read();
    if ((secure_pkru & (0x3u << (2 * PKU_KEY_SECURE))) != 0) {
        kprintf("[PKU-FAIL] Secure PKRU does not enable key %u: 0x%x\n",
                (unsigned)PKU_KEY_SECURE, (unsigned)secure_pkru);
        pkru_write(saved);
        return false;
    }

    pkru_write(saved);

    kprintf("[PKU] self-test: PASS\n");
    return true;
}
