#ifndef REAPER_SYSCALL_H
#define REAPER_SYSCALL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../shared/include/syscall.h"

/*
 * REAPER-OS SYSCALL ABI (THE VOID GATE)
 *
 * Registers:
 * rax: Syscall number
 * rdi: Arg 0
 * rsi: Arg 1
 * rdx: Arg 2
 * r10: Arg 3 (rcx is destroyed by syscall)
 * r8:  Arg 4
 * r9:  Arg 5
 *
 * Return:
 * rax: Return value
 */

/*
 * Legacy syscall numeric ids are intentionally removed from the public ABI.
 * Kernel dispatch now expects SYS_GATE_CALL with operation ids from shared ABI.
 */

typedef struct {
    uint64_t total_calls;
    uint64_t unknown_calls;
    uint64_t ownerless_rejects;
    uint64_t fault_rejects;
    uint64_t invalid_rejects;
    uint64_t perm_rejects;
    uint64_t map_calls;
    uint64_t map_strict_calls;
    uint64_t map_fail_parent;
    uint64_t map_fail_rights;
    uint64_t map_fail_index;
    uint64_t map_fail_child;
    uint64_t map_fail_flags;
    uint64_t unmap_calls;
    uint64_t unmap_strict_calls;
    uint64_t tlb_flushes;
} syscall_metrics_t;

/**
 * syscall_init: Configures MSRs and entry points for SYSCALL/SYSRET.
 */
void syscall_init(void);

/**
 * syscall_set_kernel_stack: Updates the kernel stack used by the next syscall.
 */
void syscall_set_kernel_stack(uint64_t stack_virt);
bool syscall_self_test(void);
bool syscall_get_metrics(syscall_metrics_t* out_metrics);
void syscall_reset_metrics(void);

#endif /* REAPER_SYSCALL_H */
