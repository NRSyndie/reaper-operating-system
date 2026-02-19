#ifndef REAPER_KLOG_H
#define REAPER_KLOG_H

#include <stdint.h>
#include <stdbool.h>
#include "console.h"

/* --- Axis 1: Semantic Categories --- */
#define KLOG_VOID    (1 << 0)
#define KLOG_FATE    (1 << 1)
#define KLOG_FORGE   (1 << 2)
#define KLOG_GATE    (1 << 3)
#define KLOG_PULSE   (1 << 4)
#define KLOG_ERROR   (1 << 5)

/* --- Axis 2: Temporal Certainty --- */
#define KLOG_ASSERTED    (1 << 8)
#define KLOG_ASSUMED     (1 << 9)
#define KLOG_SPECULATIVE (1 << 10)
#define KLOG_DERIVED     (1 << 11)

/* --- Axis 3: Vibrancy (Silence Tracking) --- */
#define KLOG_ORPHAN      (1 << 16)
#define KLOG_DORMANT     (1 << 17)
#define KLOG_DEAD_PATH   (1 << 18)

/* --- Ghost Assertions --- */
#define GASSERT_HARD(cond, msg) \
    do { \
        if (!(cond)) kpanic_at(__FILE__, __LINE__, "[GASSERT:HARD] " msg); \
    } while(0)

#define GASSERT_SOFT(cond, msg) \
    do { \
        if (!(cond)) {} \
    } while(0)

#define GASSERT_WATCH(cond, msg) \
    do { \
        if (!(cond)) {} \
    } while(0)

/* --- Infrastructure --- */

uint64_t klog_new_chain(void);
uint64_t klog_get_chain(void);

/* The Core Logger */
void klog(uint32_t tags, const char* fmt, ...);

// --- Convenience Logging Macros ---
#define klog_info(fmt, ...)     klog(KLOG_VOID, "INFO: " fmt, ##__VA_ARGS__)
#define klog_warn(fmt, ...)     klog(KLOG_VOID, "WARN: " fmt, ##__VA_ARGS__)
#define klog_error(fmt, ...)    klog(KLOG_ERROR, "ERROR: " fmt, ##__VA_ARGS__)
#define klog_critical(fmt, ...) do { klog(KLOG_ERROR, "CRITICAL: " fmt, ##__VA_ARGS__); kpanic_at(__FILE__, __LINE__, "CRITICAL ERROR"); } while(0)
#define klog_debug(fmt, ...)    klog(KLOG_VOID, "DEBUG: " fmt, ##__VA_ARGS__)

/* Performance Tracking */
void pulse_start(void);
uint64_t pulse_end(uint32_t tags, const char* name);

/* Silence Report */
void klog_emit_silence_report(void);

/* Observer Effect Tracking */
uint64_t klog_get_observer_cost(void);

#endif /* REAPER_KLOG_H */
