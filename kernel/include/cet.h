#ifndef CET_H
#define CET_H

#include <stdint.h>

#define MSR_IA32_S_CET     0x6A2
#define MSR_IA32_PL0_SSP   0x6A4
#define MSR_IA32_U_CET     0x6A0

#define CET_SHSTK_EN       (1ULL << 0)
#define CET_IBT_EN         (1ULL << 1)

void cet_init(void);

#endif
