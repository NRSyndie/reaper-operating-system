#include "include/cet.h"
#include "include/cpu.h"
#include "include/klog.h"

void cet_init(void) {
    bool shstk = cpu_has_cet_ss();
    bool ibt = cpu_has_cet_ibt();

    if (!shstk && !ibt) {
        klog(0, "CET: Not supported by hardware\n");
        return;
    }

    uint64_t cet_ctrl = 0;
    if (shstk) cet_ctrl |= CET_SHSTK_EN;
    if (ibt)   cet_ctrl |= CET_IBT_EN;

    wrmsr(MSR_IA32_S_CET, cet_ctrl);

    /* Enable CET in CR4 */
    uint64_t cr4 = read_cr4();
    cr4 |= (1 << 23); // CET bit
    write_cr4(cr4);

    klog(0, "CET: Initialized (SHSTK: %s, IBT: %s)\n", 
         shstk ? "YES" : "NO", ibt ? "YES" : "NO");
}
