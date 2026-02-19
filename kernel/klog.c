#include "include/klog.h"
#include "include/cpu.h"
#include "include/utils.h"
#include <stdarg.h>

static uint64_t current_chain_id = 0;
static uint64_t observer_cost_accumulator = 0;

uint64_t klog_new_chain(void) {
    current_chain_id++;
    return current_chain_id;
}

uint64_t klog_get_chain(void) {
    return current_chain_id;
}

/* Stubbed out logging function */
void klog(uint32_t tags, const char* fmt, ...) {
    (void)tags;
    (void)fmt;
    /* Do nothing */
}

static uint64_t pulse_tmp_start = 0;

void pulse_start(void) {
    pulse_tmp_start = rdtsc();
}

uint64_t pulse_end(uint32_t tags, const char* name) {
    (void)tags;
    (void)name;
    uint64_t diff = rdtsc() - pulse_tmp_start;
    return diff;
}

uint64_t klog_get_observer_cost(void) {
    return observer_cost_accumulator;
}

void klog_emit_silence_report(void) {
    /* Silent */
}