#ifndef REAPER_SYSCALL_H
#define REAPER_SYSCALL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../shared/include/syscall.h"
#include "capability.h"

uint64_t ipc_invoke_endpoint(cap_identity_t* ident, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t options);

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
    uint64_t unmap_success_calls;
    uint64_t unmap_fail_ctrl;
    uint64_t unmap_fail_parent;
    uint64_t unmap_fail_rights;
    uint64_t unmap_fail_index;
    uint64_t unmap_strict_cycles_total;
    uint64_t unmap_strict_cycles_max;
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

#include "mode.h"
#include "process.h"

uint64_t sys_mode_query_handler(process_t* owner, uint32_t cap_slot);
uint64_t sys_sched_auth_root_mint_handler(process_t* owner,
                                           uint32_t auth_cap_slot,
                                           mode_id_t mode_binding,
                                           uint64_t max_total_budget,
                                           uint64_t refill_period_ticks,
                                           uint32_t dst_slot);

#endif /* REAPER_SYSCALL_H */

