#include "include/syscall.h"
#include "include/cpu.h"
#include "include/gdt.h"
#include "include/console.h"
#include "include/mode.h"
#include "include/capability.h"
#include "include/ipc.h"
#include "include/klog.h"
#include "include/scheduler.h"
#include "include/vmm.h"
#include "include/utils.h"
#include "include/ocular.h"
#include "include/genesis.h"

#include "include/pmm.h"
#include "include/kmalloc.h"

/* The assembly entry point defined in interrupts.s */
extern void syscall_entry(void);

/**
 * GS-based scratchpad for syscalls.
 * 0: User RSP
 * 8: Kernel RSP
 */
typedef struct {
    uint64_t user_rsp;
    uint64_t kernel_rsp;
    uint64_t user_rip;
    uint64_t user_rflags;
} __attribute__((packed)) syscall_gs_t;

static syscall_gs_t cpu_syscall_gs;
static const uint64_t USER_VA_LIMIT = 0x0000800000000000ULL;

static syscall_metrics_t g_sys_metrics = {0};
static bool g_sys_audit_stub_marker_emitted = false;
static uint64_t g_sys_percall[64] = {0};
static uint64_t g_day29_reason_mask = 0;

#define LAW2_ATTEST_EVIDENCE_VERSION 2U
#define LAW2_DAY28_MIN_STRICT_MAP_CALLS 8ULL
#define LAW2_DAY29_MIN_STRICT_UNMAP_CALLS 3ULL
#define LAW2_DAY29_UNMAP_CYCLES_BUDGET 2000000ULL
#define LAW2_DAY30_SCAN_CYCLES_BUDGET 5000000ULL

/* Strict map/unmap now require ctrl bit0 set; no legacy-zero path. */
#define SYS_MAP_STRICT_FLAG   (1ULL << 0)
#define SYS_UNMAP_STRICT_FLAG (1ULL << 0)
/* Strict user-allowed PTE bits. */
#define USER_PTE_ALLOWED_MASK (VMM_PRESENT | VMM_USER | VMM_WRITABLE | VMM_NX)

/*
 * Internal legacy syscall ids.
 * These are no longer user-visible ABI numbers; userspace enters through
 * SYS_GATE_CALL and operation ids are translated to these values.
 */
enum {
    SYS_VOID_LOG = 0,
    SYS_CAP_INVOKE = 1,
    SYS_CAP_MINT = 2,
    SYS_CAP_COPY = 3,
    SYS_CAP_DELETE = 4,
    SYS_MODE_QUERY = 5,
    SYS_YIELD = 6,
    SYS_EXIT = 7,
    SYS_WAIT = 8,
    SYS_MAP = 9,
    SYS_UNMAP = 10,
    SYS_CAP_REVOKE = 11,
    SYS_LATTICE_CREATE = 12,
    SYS_LATTICE_ATTACH = 13,
    SYS_FATE_READ = 14,
    SYS_FRAME_ALLOC = 15,
    SYS_CAP_RETYPE = 16,
    SYS_ATTUNE = 17,
    SYS_OCULAR_SET = 18,
    SYS_LATTICE_DETACH = 19,
    SYS_AUDIT = 20,
    SYS_SCHED_METRICS = 21,
    SYS_SCHED_AUTH_ROOT_MINT = 22,
    SYS_SCHED_AUTH_THREAD_DERIVE = 23,
    SYS_MODE_TRANSITION = 24,
    SYS_LAW2_ATTEST = 25,
    SYS_GENESIS_INVOKE = 26
};

static int gate_op_to_legacy_sys(uint64_t op) {
    switch (op) {
        case GATE_OP_VOID_LOG: return SYS_VOID_LOG;
        case GATE_OP_CAP_INVOKE: return SYS_CAP_INVOKE;
        case GATE_OP_CAP_MINT: return SYS_CAP_MINT;
        case GATE_OP_CAP_COPY: return SYS_CAP_COPY;
        case GATE_OP_CAP_DELETE: return SYS_CAP_DELETE;
        case GATE_OP_MODE_QUERY: return SYS_MODE_QUERY;
        case GATE_OP_YIELD: return SYS_YIELD;
        case GATE_OP_EXIT: return SYS_EXIT;
        case GATE_OP_WAIT: return SYS_WAIT;
        case GATE_OP_MAP: return SYS_MAP;
        case GATE_OP_UNMAP: return SYS_UNMAP;
        case GATE_OP_CAP_REVOKE: return SYS_CAP_REVOKE;
        case GATE_OP_LATTICE_CREATE: return SYS_LATTICE_CREATE;
        case GATE_OP_LATTICE_ATTACH: return SYS_LATTICE_ATTACH;
        case GATE_OP_FATE_READ: return SYS_FATE_READ;
        case GATE_OP_FRAME_ALLOC: return SYS_FRAME_ALLOC;
        case GATE_OP_CAP_RETYPE: return SYS_CAP_RETYPE;
        case GATE_OP_ATTUNE: return SYS_ATTUNE;
        case GATE_OP_OCULAR_SET: return SYS_OCULAR_SET;
        case GATE_OP_LATTICE_DETACH: return SYS_LATTICE_DETACH;
        case GATE_OP_AUDIT: return SYS_AUDIT;
        case GATE_OP_SCHED_METRICS: return SYS_SCHED_METRICS;
        case GATE_OP_SCHED_AUTH_ROOT_MINT: return SYS_SCHED_AUTH_ROOT_MINT;
        case GATE_OP_SCHED_AUTH_THREAD_DERIVE: return SYS_SCHED_AUTH_THREAD_DERIVE;
        case GATE_OP_MODE_TRANSITION: return SYS_MODE_TRANSITION;
        case GATE_OP_LAW2_ATTEST: return SYS_LAW2_ATTEST;
        case GATE_OP_GENESIS_INVOKE: return SYS_GENESIS_INVOKE;
        default: return -1;
    }
}

static void syscall_diag_emit_periodic(void) {
    if ((g_sys_metrics.total_calls & 0x3FFULL) != 0) return; /* Every 1024 calls */

    kprintf("[SYSCALL-DIAG] total=%lu fault=%lu invalid=%lu perm=%lu map=%lu strict=%lu unmap=%lu strict_u=%lu unmap_ok=%lu uctrl=%lu uparent=%lu urights=%lu uindex=%lu umax=%lu tlb_flush=%lu\n",
            g_sys_metrics.total_calls,
            g_sys_metrics.fault_rejects,
            g_sys_metrics.invalid_rejects,
            g_sys_metrics.perm_rejects,
            g_sys_metrics.map_calls,
            g_sys_metrics.map_strict_calls,
            g_sys_metrics.unmap_calls,
            g_sys_metrics.unmap_strict_calls,
            g_sys_metrics.unmap_success_calls,
            g_sys_metrics.unmap_fail_ctrl,
            g_sys_metrics.unmap_fail_parent,
            g_sys_metrics.unmap_fail_rights,
            g_sys_metrics.unmap_fail_index,
            g_sys_metrics.unmap_strict_cycles_max,
            g_sys_metrics.tlb_flushes);
}

bool syscall_get_metrics(syscall_metrics_t* out_metrics) {
    if (!out_metrics) return false;
    memcpy(out_metrics, &g_sys_metrics, sizeof(syscall_metrics_t));
    return true;
}

void syscall_reset_metrics(void) {
    fast_zero(&g_sys_metrics, sizeof(g_sys_metrics));
    fast_zero(g_sys_percall, sizeof(g_sys_percall));
    g_sys_audit_stub_marker_emitted = false;
    g_day29_reason_mask = 0;
}

bool syscall_self_test(void) {
    if (cpu_syscall_gs.kernel_rsp == 0) return false;
    if (rdmsr(MSR_LSTAR) != (uint64_t)syscall_entry) return false;
    if ((rdmsr(MSR_EFER) & 1ULL) == 0) return false; /* SCE */
    if ((rdmsr(MSR_EFER) & (1ULL << 11)) == 0) return false; /* NXE */
    if ((rdmsr(MSR_SFMASK) & (0x200 | 0x400)) != (0x200 | 0x400)) return false;
    return true;
}

static bool user_page_access_ok(uint64_t addr, bool require_write) {
    thread_t* current = scheduler_get_current();
    if (!current || !current->owner) return false;

    pt_entry_t* pml4 = (pt_entry_t*)pmm_phys_to_virt(current->owner->pml4_phys);

    uint64_t pml4e = pml4[(addr >> 39) & 0x1FF];
    if (!(pml4e & VMM_PRESENT) || !(pml4e & VMM_USER)) return false;
    if (require_write && !(pml4e & VMM_WRITABLE)) return false;

    pt_entry_t* pdpt = (pt_entry_t*)pmm_phys_to_virt(pml4e & ~0xFFFULL);
    uint64_t pdpte = pdpt[(addr >> 30) & 0x1FF];
    if (!(pdpte & VMM_PRESENT) || !(pdpte & VMM_USER)) return false;
    if (require_write && !(pdpte & VMM_WRITABLE)) return false;
    if (pdpte & VMM_HUGE) return true;

    pt_entry_t* pd = (pt_entry_t*)pmm_phys_to_virt(pdpte & ~0xFFFULL);
    uint64_t pde = pd[(addr >> 21) & 0x1FF];
    if (!(pde & VMM_PRESENT) || !(pde & VMM_USER)) return false;
    if (require_write && !(pde & VMM_WRITABLE)) return false;
    if (pde & VMM_HUGE) return true;

    pt_entry_t* pt = (pt_entry_t*)pmm_phys_to_virt(pde & ~0xFFFULL);
    uint64_t pte = pt[(addr >> 12) & 0x1FF];
    if (!(pte & VMM_PRESENT) || !(pte & VMM_USER)) return false;
    if (require_write && !(pte & VMM_WRITABLE)) return false;

    return true;
}

static bool user_range_valid_and_mapped(uint64_t addr, size_t n, bool require_write) {
    if (n == 0) return true;
    if (addr >= USER_VA_LIMIT) return false;
    if (n - 1 > (USER_VA_LIMIT - 1 - addr)) return false; /* overflow + upper bound */
    uint64_t start = addr & ~0xFFFULL;
    uint64_t end = (addr + n - 1) & ~0xFFFULL;

    for (uint64_t va = start;; va += PAGE_SIZE) {
        if (!user_page_access_ok(va, require_write)) return false;
        if (va == end) break;
    }
    return true;
}

static bool validate_user_range_read(uint64_t addr, size_t n) {
    if (!user_range_valid_and_mapped(addr, n, false)) {
        g_sys_metrics.fault_rejects++;
        return false;
    }
    return true;
}

static bool validate_user_range_write(uint64_t addr, size_t n) {
    if (!user_range_valid_and_mapped(addr, n, true)) {
        g_sys_metrics.fault_rejects++;
        return false;
    }
    return true;
}

/**
 * Safely copy from user space.
 * Returns true on success, false if the address is in kernel space.
 */
static bool copy_from_user(void* dest, const void* src, size_t n) {
    if (!validate_user_range_read((uint64_t)src, n)) {
        return false;
    }
    memcpy(dest, src, n);
    return true;
}

/**
 * Safely copy to user space.
 */
static bool copy_to_user(void* dest, const void* src, size_t n) {
    if (!validate_user_range_write((uint64_t)dest, n)) {
        return false;
    }
    memcpy(dest, src, n);
    return true;
}

/**
 * Safely copy a string from user space.
 */
static bool copy_string_from_user(char* dest, const char* src, size_t max_len) {
    if ((uint64_t)src >= USER_VA_LIMIT) {
        g_sys_metrics.fault_rejects++;
        return false;
    }
    
    size_t i = 0;
    while (i < max_len - 1) {
        if (!copy_from_user(&dest[i], &src[i], 1)) return false;
        if (dest[i] == '\0') return true;
        i++;
    }
    dest[i] = '\0';
    return true;
}

typedef enum {
    MAP_EXEC_MAP = 0,
    MAP_EXEC_UNMAP = 1
} map_exec_op_t;

typedef struct {
    cap_type_t child_type;
    uint64_t required_flags;
} map_child_rule_t;

static const map_child_rule_t k_map_child_rules[] = {
    { CAP_TYPE_RAM, 0 },
    { CAP_TYPE_PAGETABLE, VMM_USER | VMM_WRITABLE },
};

static const map_child_rule_t* map_find_child_rule(cap_type_t child_type) {
    size_t i;
    for (i = 0; i < (sizeof(k_map_child_rules) / sizeof(k_map_child_rules[0])); i++) {
        if (k_map_child_rules[i].child_type == child_type) return &k_map_child_rules[i];
    }
    return NULL;
}

static uint64_t syscall_map_validate_parent(process_t* owner,
                                            uint32_t parent_slot,
                                            uint64_t index,
                                            cap_identity_t** out_parent_ident) {
    cap_identity_t* parent_ident = cap_lookup(owner->cspace, parent_slot);

    if (!parent_ident || parent_ident->type != CAP_TYPE_PAGETABLE) {
        g_sys_metrics.map_fail_parent++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }
    if (!(parent_ident->rights & CAP_RIGHT_WRITE)) {
        g_sys_metrics.map_fail_rights++;
        g_sys_metrics.perm_rejects++;
        return (uint64_t)-1;
    }
    if (index >= 512) {
        g_sys_metrics.map_fail_index++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }

    *out_parent_ident = parent_ident;
    return 0;
}

static uint64_t syscall_unmap_validate_parent(process_t* owner,
                                              uint32_t parent_slot,
                                              uint64_t index,
                                              cap_identity_t** out_parent_ident,
                                              uint64_t* out_day29_reason_mask) {
    cap_identity_t* parent_ident = cap_lookup(owner->cspace, parent_slot);

    if (!parent_ident || parent_ident->type != CAP_TYPE_PAGETABLE) {
        g_sys_metrics.map_fail_parent++;
        g_sys_metrics.unmap_fail_parent++;
        g_sys_metrics.invalid_rejects++;
        if (out_day29_reason_mask) *out_day29_reason_mask |= LAW2_DAY29_REASON_PARENT_DENY;
        return (uint64_t)-1;
    }
    if (!(parent_ident->rights & CAP_RIGHT_WRITE)) {
        g_sys_metrics.map_fail_rights++;
        g_sys_metrics.unmap_fail_rights++;
        g_sys_metrics.perm_rejects++;
        if (out_day29_reason_mask) *out_day29_reason_mask |= LAW2_DAY29_REASON_RIGHTS_DENY;
        return (uint64_t)-1;
    }
    if (index >= 512) {
        g_sys_metrics.map_fail_index++;
        g_sys_metrics.unmap_fail_index++;
        g_sys_metrics.invalid_rejects++;
        if (out_day29_reason_mask) *out_day29_reason_mask |= LAW2_DAY29_REASON_INDEX_DENY;
        return (uint64_t)-1;
    }

    *out_parent_ident = parent_ident;
    return 0;
}

static uint64_t syscall_map_validate_child(process_t* owner,
                                           uint32_t child_slot,
                                           uint64_t flags,
                                           cap_identity_t** out_child_ident) {
    cap_identity_t* child_ident = cap_lookup(owner->cspace, child_slot);
    const map_child_rule_t* child_rule;

    if (!child_ident) {
        g_sys_metrics.map_fail_child++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }

    child_rule = map_find_child_rule(child_ident->type);
    if (!child_rule) {
        g_sys_metrics.map_fail_child++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }

    if ((flags & child_rule->required_flags) != child_rule->required_flags) {
        g_sys_metrics.map_fail_flags++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }

    *out_child_ident = child_ident;
    return 0;
}

static uint64_t syscall_map_execute(process_t* owner,
                                    map_exec_op_t op,
                                    uint32_t parent_slot,
                                    uint64_t index,
                                    uint32_t child_slot,
                                    uint64_t flags) {
    cap_identity_t* parent_ident = NULL;

    if (syscall_map_validate_parent(owner, parent_slot, index, &parent_ident) != 0) {
        return (uint64_t)-1;
    }

    if (op == MAP_EXEC_UNMAP) {
        vmm_set_entry(parent_ident->object_ptr, index, 0);
        invpcid_flush_all();
        g_sys_metrics.tlb_flushes++;
        return 0;
    }

    if ((flags & ~USER_PTE_ALLOWED_MASK) != 0) {
        g_sys_metrics.map_fail_flags++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }

    if (child_slot == 0) {
        /* Strict-only contract: unmap is explicit via SYS_UNMAP. */
        g_sys_metrics.map_fail_child++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }

    cap_identity_t* child_ident = NULL;
    if (syscall_map_validate_child(owner, child_slot, flags, &child_ident) != 0) {
        return (uint64_t)-1;
    }

    uint64_t safe_flags = VMM_PRESENT | VMM_USER;
    if (child_ident->rights & CAP_RIGHT_WRITE) safe_flags |= VMM_WRITABLE;
    if (!(child_ident->rights & CAP_RIGHT_EXECUTE)) safe_flags |= VMM_NX;

    uint64_t entry_val = (child_ident->object_ptr & ~0xFFFULL) | (flags & safe_flags);
    vmm_set_entry(parent_ident->object_ptr, index, entry_val);
    invpcid_flush_all();
    g_sys_metrics.tlb_flushes++;
    return 0;
}

static uint64_t syscall_map_entry(process_t* owner,
                                  uint32_t parent_slot,
                                  uint64_t index,
                                  uint32_t child_slot,
                                  uint64_t flags,
                                  uint64_t ctrl) {
    if (ctrl != SYS_MAP_STRICT_FLAG) {
        g_sys_metrics.map_fail_flags++;
        g_sys_metrics.invalid_rejects++;
        return (uint64_t)-1;
    }
    g_sys_metrics.map_calls++;
    g_sys_metrics.map_strict_calls++;
    return syscall_map_execute(owner, MAP_EXEC_MAP, parent_slot, index, child_slot, flags);
}

static uint64_t syscall_unmap_entry(process_t* owner,
                                    uint32_t parent_slot,
                                    uint64_t index,
                                    uint64_t ctrl) {
    uint64_t tsc_start = rdtsc();
    uint64_t tsc_end;
    uint64_t duration;

    g_sys_metrics.unmap_calls++;
    if (ctrl != SYS_UNMAP_STRICT_FLAG) {
        g_sys_metrics.invalid_rejects++;
        g_sys_metrics.unmap_fail_ctrl++;
        g_day29_reason_mask |= LAW2_DAY29_REASON_CTRL_DENY;
        return (uint64_t)-1;
    }
    g_sys_metrics.unmap_strict_calls++;

    cap_identity_t* parent_ident = NULL;
    if (syscall_unmap_validate_parent(owner, parent_slot, index, &parent_ident, &g_day29_reason_mask) != 0) {
        return (uint64_t)-1;
    }

    vmm_set_entry(parent_ident->object_ptr, index, 0);
    invpcid_flush_all();
    g_sys_metrics.tlb_flushes++;
    g_sys_metrics.unmap_success_calls++;
    g_day29_reason_mask |= LAW2_DAY29_REASON_AUTH_OK;

    tsc_end = rdtsc();
    duration = tsc_end - tsc_start;
    g_sys_metrics.unmap_strict_cycles_total += duration;
    if (duration > g_sys_metrics.unmap_strict_cycles_max) {
        g_sys_metrics.unmap_strict_cycles_max = duration;
    }
    return 0;
}

static uint16_t law2_day28_reason(const gate_law2_attest_t* out) {
    if (out->map_calls != out->map_strict_calls) return 1;
    if (out->map_strict_calls < LAW2_DAY28_MIN_STRICT_MAP_CALLS) return 2;
    if (out->map_fail_index < 1) return 3;
    if (out->map_fail_child < 1) return 4;
    if (out->map_fail_flags < 2) return 5;
    return 0;
}

static uint16_t law2_day29_reason(const gate_law2_attest_t* out) {
    if (out->unmap_calls < out->unmap_strict_calls) return 1;
    if (out->unmap_strict_calls < LAW2_DAY29_MIN_STRICT_UNMAP_CALLS) return 2;
    if (out->map_calls != out->map_strict_calls) return 3;
    if ((out->day29_reason_mask & LAW2_DAY29_REASON_MASK_REQUIRED) != LAW2_DAY29_REASON_MASK_REQUIRED) return 4;
    if (out->day29_unmap_cycles_max > out->day29_perf_budget_cycles) return 5;
    return 0;
}

static uint64_t law2_day30_reason_bit(uint32_t reject_reason) {
    switch (reject_reason) {
        case MODE_REJECT_EDGE_ILLEGAL:
            return LAW2_DAY30_REASON_EDGE_ILLEGAL;
        case MODE_REJECT_AUTH_REQUIRED:
            return LAW2_DAY30_REASON_AUTH_REQUIRED;
        case MODE_REJECT_SPECIAL_KEY_REQUIRED:
            return LAW2_DAY30_REASON_SPECIAL_KEY_REQ;
        case MODE_REJECT_COOLDOWN_ACTIVE:
            return LAW2_DAY30_REASON_COOLDOWN_ACTIVE;
        default:
            return 0;
    }
}

static uint16_t law2_day30_reason(gate_law2_attest_t* out) {
    struct mode_transition* recs;
    int count;
    uint64_t reject_with_reason = 0;
    uint64_t reason_mask = 0;
    int i;
    uint64_t tsc_start = rdtsc();
    uint64_t tsc_end;

    recs = (struct mode_transition*)kzalloc(sizeof(struct mode_transition) * MODE_HISTORY_SIZE);
    if (!recs) return 1;

    count = mode_get_history_filtered(recs, MODE_HISTORY_SIZE, FATE_READ_TRANSITIONS);
    if (count <= 0) {
        kfree(recs);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (recs[i].record_type != FATE_RECORD_TRANSITION) continue;
        if (recs[i].result_code != FATE_RESULT_REJECTED) continue;
        if (recs[i].fault_error_code == MODE_REJECT_NONE) continue;
        reject_with_reason++;
        reason_mask |= law2_day30_reason_bit(recs[i].fault_error_code);
    }

    tsc_end = rdtsc();
    out->transition_reject_with_reason = reject_with_reason;
    out->day30_reason_mask = reason_mask;
    out->day30_reject_scan_cycles = tsc_end - tsc_start;
    out->day30_perf_budget_cycles = LAW2_DAY30_SCAN_CYCLES_BUDGET;
    kfree(recs);
    if (reject_with_reason == 0) return 2;
    if ((reason_mask & LAW2_DAY30_REASON_MASK_REQUIRED) != LAW2_DAY30_REASON_MASK_REQUIRED) return 3;
    if (out->day30_reject_scan_cycles > out->day30_perf_budget_cycles) return 4;
    return 0;
}

static void law2_collect_attestation(gate_law2_attest_t* out, uint16_t* day28_reason, uint16_t* day29_reason, uint16_t* day30_reason) {
    memset(out, 0, sizeof(*out));
    out->evidence_version = LAW2_ATTEST_EVIDENCE_VERSION;
    out->map_calls = g_sys_metrics.map_calls;
    out->map_strict_calls = g_sys_metrics.map_strict_calls;
    out->unmap_calls = g_sys_metrics.unmap_calls;
    out->unmap_strict_calls = g_sys_metrics.unmap_strict_calls;
    out->map_fail_index = g_sys_metrics.map_fail_index;
    out->map_fail_child = g_sys_metrics.map_fail_child;
    out->map_fail_flags = g_sys_metrics.map_fail_flags;
    out->day29_reason_mask = g_day29_reason_mask;
    out->day29_unmap_cycles_max = g_sys_metrics.unmap_strict_cycles_max;
    if (g_sys_metrics.unmap_success_calls > 0) {
        out->day29_unmap_cycles_avg = g_sys_metrics.unmap_strict_cycles_total / g_sys_metrics.unmap_success_calls;
    } else {
        out->day29_unmap_cycles_avg = 0;
    }
    out->day29_perf_budget_cycles = LAW2_DAY29_UNMAP_CYCLES_BUDGET;

    *day28_reason = law2_day28_reason(out);
    *day29_reason = law2_day29_reason(out);
    *day30_reason = law2_day30_reason(out);

    out->day28_status = (*day28_reason == 0) ? 1U : 0U;
    out->day29_status = (*day29_reason == 0) ? 1U : 0U;
    out->day30_status = (*day30_reason == 0) ? 1U : 0U;
}

void syscall_init(void) {
    /* 1. Allocate a dedicated syscall stack for the kernel */
    uint64_t stack_phys = pmm_alloc(COLOR_VOID, 0);
    uint64_t stack_virt = (uint64_t)pmm_phys_to_virt(stack_phys) + 4096;
    
    cpu_syscall_gs.kernel_rsp = stack_virt;
    cpu_syscall_gs.user_rsp = 0;

    /* 2. Configure GS bases
     * KERNEL_GS_BASE is what SWAPGS pulls into GS
     * GS_BASE is what the CPU uses during user mode (currently unused)
     */
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)&cpu_syscall_gs);
    wrmsr(MSR_GS_BASE, 0);

    /* 3. Enable System Call Extensions (SCE) and No-Execute (NXE) in EFER */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1;       /* SCE Bit */
    efer |= (1<<11); /* NXE Bit */
    wrmsr(MSR_EFER, efer);

    /* 
     * 2. Configure STAR MSR (Segment Selectors)
     * Bits 47:32 -> Kernel CS (and SS = CS + 8)
     * Bits 63:48 -> User CS (and SS = CS + 8)
     * Note: SYSRET expects User CS to be +16 from the base set here, and SS to be +8.
     * This is slightly confusing in x86_64:
     * SYSCALL: CS = STAR[47:32], SS = STAR[47:32] + 8
     * SYSRET:  CS = STAR[63:48] + 16, SS = STAR[63:48] + 8
     */
    uint64_t star = ((uint64_t)GDT_KERNEL_CODE << 32) | ((uint64_t)(GDT_USER_DATA - 8) << 48);
    wrmsr(MSR_STAR, star);

    /* 3. Configure LSTAR (Entry Point) */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 4. Configure SFMASK (RFLAGS mask) 
     * Mask Interrupts (bit 9) and Direction (bit 10) on syscall entry.
     */
    wrmsr(MSR_SFMASK, 0x200 | 0x400); 

    }

/* Helper for IPC Endpoint invocation */
uint64_t ipc_invoke_endpoint(cap_identity_t* ident, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t options) {
    ipc_endpoint_t* ep = (ipc_endpoint_t*)ident->object_ptr;
    thread_t* caller = scheduler_get_current();

    if (!ep) return -1;

    uint64_t flags = spinlock_irqsave(&ep->lock);

    /* Determine Operation: SEND or RECEIVE based on options and rights */
    bool is_send = false;
    if (options == CAP_INVOKE_OPT_SEND) {
        is_send = true;
    } else if (options == CAP_INVOKE_OPT_RECV) {
        is_send = false;
    } else {
        /* AUTO: SEND if WRITE right present, else RECEIVE if READ right present */
        if (ident->rights & CAP_RIGHT_WRITE) {
            is_send = true;
        } else if (ident->rights & CAP_RIGHT_READ) {
            is_send = false;
        } else {
            /* No appropriate rights for AUTO */
            spinlock_irqrestore(&ep->lock, flags);
            return -1;
        }
    }

    if (is_send) {
        /* Check rights for forced SEND */
        if (!(ident->rights & CAP_RIGHT_WRITE)) {
            spinlock_irqrestore(&ep->lock, flags);
            return -1;
        }

        /* --- SEND OPERATION --- */
        
        /* 1. Check for a waiting receiver (Rendezvous) */
        if (ep->recv_head) {
            thread_t* receiver = ep->recv_head;
            ep->recv_head = receiver->wait_next;
            if (!ep->recv_head) ep->recv_tail = NULL;

            /* Transfer Data (Register-Only) */
            receiver->ipc_payload[0] = a0;
            receiver->ipc_payload[1] = a1;
            receiver->ipc_payload[2] = a2;
            receiver->ipc_payload[3] = ident->badge;

            /* Wake Receiver */
            scheduler_wake(receiver);
            spinlock_irqrestore(&ep->lock, flags);
            return 0;
        }

        /* 2. Buffer the message if space available (Asynchronous) */
        if (ep->count < IPC_BUFFER_SIZE) {
            uint32_t idx = ep->tail;
            ep->buffer[idx].data[0] = a0;
            ep->buffer[idx].data[1] = a1;
            ep->buffer[idx].data[2] = a2;
            ep->buffer[idx].data[3] = ident->badge;
            
            ep->tail = (ep->tail + 1) % IPC_BUFFER_SIZE;
            ep->count++;
            
            spinlock_irqrestore(&ep->lock, flags);
            return 0;
        }

        /* 3. Buffer full: Block the sender (Synchronous fallback) */
        scheduler_block(caller);
        caller->ipc_payload[0] = a0;
        caller->ipc_payload[1] = a1;
        caller->ipc_payload[2] = a2;
        caller->ipc_payload[3] = ident->badge;
        caller->wait_next = NULL;

        if (!ep->send_head) {
            ep->send_head = caller;
            ep->send_tail = caller;
        } else {
            ep->send_tail->wait_next = caller;
            ep->send_tail = caller;
        }

        spinlock_irqrestore(&ep->lock, flags);
        schedule();
        return 0;

    } else {
        /* Check rights for forced RECEIVE */
        if (!(ident->rights & CAP_RIGHT_READ)) {
            spinlock_irqrestore(&ep->lock, flags);
            return -1;
        }

        /* --- RECEIVE OPERATION --- */

        /* 1. Check buffer first */
        if (ep->count > 0) {
            uint32_t idx = ep->head;
            caller->ipc_payload[0] = ep->buffer[idx].data[0];
            caller->ipc_payload[1] = ep->buffer[idx].data[1];
            caller->ipc_payload[2] = ep->buffer[idx].data[2];
            caller->ipc_payload[3] = ep->buffer[idx].data[3];

            ep->head = (ep->head + 1) % IPC_BUFFER_SIZE;
            ep->count--;

            /* 2. Wake a blocked sender if any */
            if (ep->send_head) {
                thread_t* sender = ep->send_head;
                ep->send_head = sender->wait_next;
                if (!ep->send_head) ep->send_tail = NULL;

                /* Push blocked sender's message into the now-vacant buffer slot */
                uint32_t nidx = ep->tail;
                ep->buffer[nidx].data[0] = sender->ipc_payload[0];
                ep->buffer[nidx].data[1] = sender->ipc_payload[1];
                ep->buffer[nidx].data[2] = sender->ipc_payload[2];
                ep->buffer[nidx].data[3] = sender->ipc_payload[3];

                ep->tail = (ep->tail + 1) % IPC_BUFFER_SIZE;
                ep->count++;

                scheduler_wake(sender);
            }

            spinlock_irqrestore(&ep->lock, flags);
            return 0;
        }

        /* 3. Buffer empty: Block the receiver */
        scheduler_block(caller);
        caller->wait_next = NULL;

        if (!ep->recv_head) {
            ep->recv_head = caller;
            ep->recv_tail = caller;
        } else {
            ep->recv_tail->wait_next = caller;
            ep->recv_tail = caller;
        }

        spinlock_irqrestore(&ep->lock, flags);
        schedule();
        return 0;
    }
}

/*
 * SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER:
 * Fixed kernel policy constant for SYS_SCHED_AUTH_ROOT_MINT.
 * Defines the maximum accumulated budget ceiling as 4x max_total_budget
 * to bound refill accumulation window, matching Genesis initialization policy.
 */
#define SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER 4ULL

uint64_t sys_mode_query_handler(process_t* owner, uint32_t cap_slot) {
    if (process_has_capability_at(owner, cap_slot, CAP_TYPE_REALITY_CTRL, CAP_RIGHT_READ)) {
        return (uint64_t)mode_get_current();
    }
    /* Redacted for occupants: "The Architect is pleased." */
    return (uint64_t)MODE_CASUAL;
}

uint64_t sys_sched_auth_root_mint_handler(process_t* owner,
                                           uint32_t auth_cap_slot,
                                           mode_id_t mode_binding,
                                           uint64_t max_total_budget,
                                           uint64_t refill_period_ticks,
                                           uint32_t dst_slot) {
    if (!owner || !owner->cspace) {
        g_sys_metrics.ownerless_rejects++;
        return (uint64_t)-1;
    }

    /* Privileged minting: Gated on CAP_TYPE_REALITY_CTRL with CAP_RIGHT_GRANT at presented slot. */
    if (!process_has_capability_at(owner, auth_cap_slot, CAP_TYPE_REALITY_CTRL, CAP_RIGHT_GRANT)) {
        g_sys_metrics.perm_rejects++;
        return (uint64_t)-1;
    }

    uint64_t max_accumulated = max_total_budget * SCHED_AUTH_MAX_ACCUMULATED_MULTIPLIER;
    return (scheduler_mint_root_auth(owner,
                                    dst_slot,
                                    mode_binding,
                                    max_total_budget,
                                    refill_period_ticks,
                                    max_accumulated) == 0) ? 0 : (uint64_t)-1;
}

/**
 * syscall_dispatcher: The C-side handler for the Void Gate.
 * Called from assembly with user registers.
 */
uint64_t syscall_dispatcher(uint64_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {

    g_sys_metrics.total_calls++;
    if (num < (sizeof(g_sys_percall) / sizeof(g_sys_percall[0]))) {
        g_sys_percall[num]++;
    }
    
    // Get current context for logging
    thread_t* current = scheduler_get_current();
    if (!current) {
        kprintf("[SYSCALL] CRITICAL: No current thread context!\n");
        return -1;
    }

    // kprintf("[SYSCALL] num=%ld a0=%lx a1=%lx\n", num, a0, a1);
    
    uint64_t ret = -1;
    process_t* owner = current->owner;

    if (num != SYS_GATE_CALL) {
        g_sys_metrics.unknown_calls++;
        g_sys_metrics.invalid_rejects++;
        syscall_diag_emit_periodic();
        return (uint64_t)-1;
    }

    uint64_t gate_op = a0;
    gate_call_msg_t gate_msg;
    fast_zero(&gate_msg, sizeof(gate_msg));

    if (a1 != 0 || a2 != 0) {
        if (a1 == 0 || a2 != sizeof(gate_call_msg_t)) {
            g_sys_metrics.invalid_rejects++;
            syscall_diag_emit_periodic();
            return (uint64_t)-1;
        }
        if (!copy_from_user(&gate_msg, (const void*)a1, sizeof(gate_msg))) {
            syscall_diag_emit_periodic();
            return (uint64_t)-1;
        }
    }

    int legacy_num = gate_op_to_legacy_sys(gate_op);
    if (legacy_num < 0) {
        g_sys_metrics.unknown_calls++;
        g_sys_metrics.invalid_rejects++;
        syscall_diag_emit_periodic();
        return (uint64_t)-1;
    }

    num = (uint64_t)legacy_num;
    a0 = gate_msg.args[0];
    a1 = gate_msg.args[1];
    a2 = gate_msg.args[2];
    a3 = gate_msg.args[3];
    a4 = gate_msg.args[4];

    switch (num) {
        case SYS_VOID_LOG: {
            char log_buf[256];
            if (copy_string_from_user(log_buf, (const char*)a0, sizeof(log_buf))) {
                kprintf("[USER-LOG] %s\n", log_buf);
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_MODE_QUERY:
            ret = sys_mode_query_handler(owner, (uint32_t)a0);
            break;


        case SYS_MODE_TRANSITION:
            if (a0 < MODE_CASUAL || a0 > MODE_GHOST) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }
            if ((a1 & ~((uint64_t)(MODE_AUTH_PASSWORD |
                                   MODE_AUTH_SPECIAL_KEY |
                                   MODE_AUTH_SYSTEM_PROMPT |
                                   MODE_AUTH_COOLDOWN_ELAPSED |
                                   MODE_AUTH_DEESC_ELAPSED |
                                   MODE_AUTH_MANUAL))) != 0) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }
            ret = (uint64_t)mode_request_transition_ex((mode_id_t)a0, TRANSITION_SOURCE_USER, (uint32_t)a1);
            break;

        case SYS_CAP_INVOKE: {
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            cap_identity_t* ident = cap_lookup(owner->cspace, (uint32_t)a0);
            if (!ident) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            if (ident->type == CAP_TYPE_ENDPOINT) {
                ret = ipc_invoke_endpoint(ident, a1, a2, a3, a4);
            } else {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
            }
            break;
        }

        case SYS_CAP_MINT:
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            if ((((uint8_t)a4) & (uint8_t)~CAP_MODE_VALID_MASK) != 0 || ((uint8_t)a4) == 0) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }
            ret = cap_mint(owner->cspace, (uint32_t)a0, (uint32_t)a1, (uint16_t)a2, (uint32_t)a3, (uint8_t)a4);
            break;

        case SYS_CAP_COPY:
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            ret = cap_copy(owner->cspace, (uint32_t)a0, (uint32_t)a1);
            break;

        case SYS_CAP_DELETE:
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            cap_delete(owner->cspace, (uint32_t)a0);
            ret = 0;
            break;

        case SYS_CAP_REVOKE:
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            cap_revoke(owner->cspace, (uint32_t)a0);
            ret = 0;
            break;

        case SYS_YIELD:
            thread_yield();
            ret = 0;
            break;

        case SYS_EXIT:
            thread_exit();
            break;

        case SYS_WAIT:
        {
            /* a0 bit0: non-blocking mode */
            uint64_t flags = a0;
            if (flags & ~1ULL) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            if (!owner) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            /* Consume a pending peer-exit event immediately. */
            if (owner->exit_events > 0) {
                owner->exit_events--;
                ret = 0;
                break;
            }

            /* Non-blocking: nothing available yet. */
            if (flags & 1ULL) {
                ret = 1;
                break;
            }

            /* No peers to wait for and no pending event. */
            if (owner->thread_count <= 1) {
                ret = 1;
                break;
            }

            if (owner->waiter_thread && owner->waiter_thread != current) {
                g_sys_metrics.perm_rejects++;
                ret = -1;
                break;
            }

            owner->waiter_thread = current;
            scheduler_block(current);
            schedule();

            /* Wake path: consume one event if present. */
            if (owner->exit_events > 0) {
                owner->exit_events--;
                ret = 0;
            } else {
                ret = -1;
            }
            break;
        }

        case SYS_MAP: {
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            ret = syscall_map_entry(owner, (uint32_t)a0, a1, (uint32_t)a2, a3, a4);
            break;
        }

        case SYS_ATTUNE: {
            uint32_t lattice_cap = (uint32_t)a0;
            uint32_t new_crystal_index = (uint32_t)a1;
            
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            cap_identity_t* ident = cap_lookup(owner->cspace, lattice_cap);
            if (!ident || ident->type != CAP_TYPE_LATTICE) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            lattice_t* lattice = (lattice_t*)ident->object_ptr;
            
            /* SECURITY CHECK: Only the Source (Owner/Writer) can attune the crystal */
            if (!(ident->rights & CAP_RIGHT_WRITE)) {
                g_sys_metrics.perm_rejects++;
                ret = -1;
                break;
            }

            ret = (uint64_t)lattice_attune(lattice, new_crystal_index);
            break;
        }

        case SYS_CAP_RETYPE: {
            uint32_t src_slot = (uint32_t)a0;
            uint32_t dst_slot = (uint32_t)a1;
            uint32_t new_type = (uint32_t)a2;
            uint32_t badge    = (uint32_t)a3;
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            ret = cap_retype(owner->cspace, src_slot, dst_slot, (uint16_t)new_type, badge);
            break;
        }

        case SYS_UNMAP: {
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            ret = syscall_unmap_entry(owner, (uint32_t)a0, a1, a2);
            break;
        }

        case SYS_LATTICE_ATTACH: {
            uint32_t lattice_cap = (uint32_t)a0;
            uint64_t vaddr = a1;
            
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            cap_identity_t* ident = cap_lookup(owner->cspace, lattice_cap);
            if (!ident || ident->type != CAP_TYPE_LATTICE) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }
            if (!(ident->rights & CAP_RIGHT_READ)) {
                g_sys_metrics.perm_rejects++;
                ret = -1;
                break;
            }

            lattice_t* lattice = (lattice_t*)ident->object_ptr;
            bool is_source = (ident->rights & CAP_RIGHT_WRITE);
            uint64_t span = (uint64_t)lattice->page_count * PAGE_SIZE;
            if ((vaddr & (PAGE_SIZE - 1)) != 0 || vaddr >= USER_VA_LIMIT || span == 0 || (span - 1 > (USER_VA_LIMIT - 1 - vaddr))) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            ret = (uint64_t)process_attach_lattice(owner, lattice, vaddr, is_source);
            break;
        }

        case SYS_LATTICE_DETACH: {
            uint32_t lattice_cap = (uint32_t)a0;
            uint64_t vaddr = a1;

            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            cap_identity_t* ident = cap_lookup(owner->cspace, lattice_cap);
            if (!ident || ident->type != CAP_TYPE_LATTICE) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }
            if (!(ident->rights & CAP_RIGHT_READ)) {
                g_sys_metrics.perm_rejects++;
                ret = -1;
                break;
            }
            if ((vaddr & (PAGE_SIZE - 1)) != 0 || vaddr >= USER_VA_LIMIT) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            lattice_t* lattice = (lattice_t*)ident->object_ptr;
            ret = (uint64_t)process_detach_lattice(owner, lattice, vaddr);
            break;
        }

        case SYS_LATTICE_CREATE: {
            uint32_t page_count = (uint32_t)a0;
            uint32_t source_slot = (uint32_t)a1;
            uint32_t listener_count = (uint32_t)a2;
            uint32_t listener_slot0 = (uint32_t)a3;
            uint32_t listener_slot1 = (uint32_t)a4;

            if (listener_count > 2) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }
            if (page_count == 0 || page_count > 1024) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            if (listener_count >= 1 && listener_slot0 == source_slot) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }
            if (listener_count >= 1 && listener_slot0 == 0) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            if (listener_count >= 2) {
                if (listener_slot1 == source_slot || listener_slot1 == listener_slot0) {
                    g_sys_metrics.invalid_rejects++;
                    ret = -1;
                    break;
                }
                if (listener_slot1 == 0) {
                    g_sys_metrics.invalid_rejects++;
                    ret = -1;
                    break;
                }
            }
            
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            if (source_slot == 0) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            extern lattice_t* lattice_create(uint32_t page_count, uint64_t owner_token);
            lattice_t* lattice = lattice_create(page_count, owner->pid);
            if (!lattice) {
                ret = -1;
                break;
            }

            cap_identity_t* ident = cap_identity_create((uint64_t)lattice, CAP_TYPE_LATTICE, CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT, 0, CAP_MODE_ALL);
            if (!ident) {
                lattice_destroy(lattice);
                ret = -1;
                break;
            }

            if (cap_insert(owner->cspace, source_slot, ident) != 0) {
                cap_identity_free(ident);
                ret = -1;
                break;
            }

            if (listener_count >= 1) {
                if (cap_mint(owner->cspace, source_slot, listener_slot0, CAP_RIGHT_READ, 0, CAP_MODE_ALL) != 0) {
                    cap_delete(owner->cspace, source_slot);
                    ret = -1;
                    break;
                }
            }

            if (listener_count >= 2) {
                if (cap_mint(owner->cspace, source_slot, listener_slot1, CAP_RIGHT_READ, 0, CAP_MODE_ALL) != 0) {
                    cap_delete(owner->cspace, listener_slot0);
                    cap_delete(owner->cspace, source_slot);
                    ret = -1;
                    break;
                }
            }

            ret = 0;
            break;
        }

        case SYS_OCULAR_SET: {
            uint32_t lattice_cap = (uint32_t)a0;
            uint32_t x = (uint32_t)a1;
            uint32_t y = (uint32_t)a2;
            uint32_t w = (uint32_t)a3;
            uint32_t h = (uint32_t)a4;
            uint8_t reality_mask = (uint8_t)current->ipc_payload[0]; // Extra arg in payload

            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = -1;
                break;
            }
            cap_identity_t* ident = cap_lookup(owner->cspace, lattice_cap);
            if (!ident || ident->type != CAP_TYPE_LATTICE) {
                g_sys_metrics.invalid_rejects++;
                ret = -1;
                break;
            }

            ret = (uint64_t)ocular_set_projection((lattice_t*)ident->object_ptr, x, y, w, h, reality_mask);
            break;
        }

        case SYS_FATE_READ: {
            struct mode_transition* user_buf = (struct mode_transition*)a0;
            uint64_t raw_count = a1;
            size_t count;
            uint32_t audit_cap = (uint32_t)a2;
            uint32_t read_mode = (uint32_t)a3;
            
            /* SECURITY CHECK: Must hold CAP_TYPE_AUDITOR */
            if (!owner || !owner->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = (uint64_t)-1;
                break;
            }
            cap_identity_t* ident = cap_lookup(owner->cspace, audit_cap);
            if (!ident || ident->type != CAP_TYPE_AUDITOR) {
                g_sys_metrics.perm_rejects++;
                ret = (uint64_t)-1;
                break;
            }
            if (!(ident->rights & CAP_RIGHT_READ)) {
                g_sys_metrics.perm_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            if (read_mode > FATE_READ_ATTEST) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            /* Reject signed/overflowed counts from user ABI (int count). */
            if (raw_count > 0x7FFFFFFFULL) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            count = (size_t)raw_count;
            if (count == 0) { ret = 0; break; }
            if (((uint64_t)user_buf % 8) != 0) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            // Cap count to keep per-call allocation bounded.
            if (count > 128) count = 128;
            if (!validate_user_range_write((uint64_t)user_buf, count * sizeof(struct mode_transition))) {
                ret = (uint64_t)-1;
                break;
            }
            
            size_t scratch_size = count * sizeof(struct mode_transition);
            struct mode_transition* kernel_buf = (struct mode_transition*)kzalloc(scratch_size);
            if (!kernel_buf) {
                ret = (uint64_t)-1; 
                break; 
            }

            // Retrieve history
            int copied = mode_get_history_filtered(kernel_buf, count, (fate_read_mode_t)read_mode);
            if (copied < 0 || (size_t)copied > count) {
                kfree(kernel_buf);
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }
            
            if (copied > 0) {
                if (!copy_to_user(user_buf, kernel_buf, copied * sizeof(struct mode_transition))) {
                    copied = -1;
                }
            }
            
            kfree(kernel_buf);
            ret = (uint64_t)copied;
            break;
        }

        case SYS_LAW2_ATTEST: {
            gate_law2_attest_t out;
            gate_law2_attest_t* user_out = (gate_law2_attest_t*)a0;
            uint64_t user_size = a1;
            uint16_t day28_reason = 0;
            uint16_t day29_reason = 0;
            uint16_t day30_reason = 0;

            if (user_out == NULL || user_size < sizeof(gate_law2_attest_t)) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            if (!validate_user_range_write((uint64_t)user_out, sizeof(gate_law2_attest_t))) {
                ret = (uint64_t)-1;
                break;
            }

            law2_collect_attestation(&out, &day28_reason, &day29_reason, &day30_reason);

            mode_log_law2_attestation(28, out.day28_status ? FATE_RESULT_ACCEPTED : FATE_RESULT_REJECTED, day28_reason, (uint32_t)out.map_strict_calls);
            mode_log_law2_attestation(29, out.day29_status ? FATE_RESULT_ACCEPTED : FATE_RESULT_REJECTED, day29_reason, (uint32_t)out.unmap_strict_calls);
            mode_log_law2_attestation(30, out.day30_status ? FATE_RESULT_ACCEPTED : FATE_RESULT_REJECTED, day30_reason, (uint32_t)out.transition_reject_with_reason);

            kprintf("[LAW2_ATTEST] day=28 result=%s reason=%u strict_map=%lu fail_index=%lu fail_child=%lu fail_flags=%lu\n",
                    out.day28_status ? "PASS" : "FAIL",
                    (unsigned)day28_reason,
                    out.map_strict_calls,
                    out.map_fail_index,
                    out.map_fail_child,
                    out.map_fail_flags);
            kprintf("[LAW2_ATTEST] day=29 result=%s reason=%u strict_unmap=%lu strict_map=%lu reason_mask=0x%lx unmap_max=%lu unmap_avg=%lu budget=%lu\n",
                    out.day29_status ? "PASS" : "FAIL",
                    (unsigned)day29_reason,
                    out.unmap_strict_calls,
                    out.map_strict_calls,
                    out.day29_reason_mask,
                    out.day29_unmap_cycles_max,
                    out.day29_unmap_cycles_avg,
                    out.day29_perf_budget_cycles);
            kprintf("[LAW2_ATTEST] day=30 result=%s reason=%u reject_reason_events=%lu reason_mask=0x%lx scan_cycles=%lu budget=%lu\n",
                    out.day30_status ? "PASS" : "FAIL",
                    (unsigned)day30_reason,
                    out.transition_reject_with_reason,
                    out.day30_reason_mask,
                    out.day30_reject_scan_cycles,
                    out.day30_perf_budget_cycles);

            if (!copy_to_user(user_out, &out, sizeof(out))) {
                ret = (uint64_t)-1;
                break;
            }

            ret = 0;
            break;
        }

        case SYS_AUDIT: {
            /*
             * Epoch III Day 43 contract freeze:
             * SYS_AUDIT is now reserved in the ABI but remains fail-closed
             * until target identity/delegation semantics are implemented.
             */
            if (!g_sys_audit_stub_marker_emitted) {
                kprintf("[SYS_AUDIT] Contract frozen in ABI; implementation pending. fail-closed=-1\n");
                g_sys_audit_stub_marker_emitted = true;
            }
            g_sys_metrics.perm_rejects++;
            ret = (uint64_t)-1;
            break;
        }

        case SYS_SCHED_METRICS: {
            gate_sched_metrics_t user_metrics;
            sched_metrics_t kernel_metrics;
            void* user_buf = (void*)a0;
            uint64_t user_buf_size = a1;

            if (!user_buf || user_buf_size < sizeof(gate_sched_metrics_t)) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            if (!validate_user_range_write((uint64_t)user_buf, sizeof(gate_sched_metrics_t))) {
                ret = (uint64_t)-1;
                break;
            }

            scheduler_get_metrics(&kernel_metrics);
            fast_zero(&user_metrics, sizeof(user_metrics));
            user_metrics.schedule_count = kernel_metrics.schedule_count;
            user_metrics.switch_count = kernel_metrics.switch_count;
            user_metrics.remote_enqueue = kernel_metrics.remote_enqueue;
            user_metrics.migrations = kernel_metrics.migrations;
            user_metrics.denied_enqueue = kernel_metrics.denied_enqueue;
            user_metrics.denied_wake = kernel_metrics.denied_wake;
            user_metrics.denied_dispatch = kernel_metrics.denied_dispatch;
            user_metrics.denied_no_auth = kernel_metrics.denied_no_auth;
            user_metrics.denied_mode_mismatch = kernel_metrics.denied_mode_mismatch;
            user_metrics.budget_exhaustions = kernel_metrics.budget_exhaustions;
            user_metrics.envelope_switches = kernel_metrics.envelope_switches;
            user_metrics.active_security_epoch = kernel_metrics.active_security_epoch;
            user_metrics.cpu_id = kernel_metrics.cpu_id;
            user_metrics.active_mode = kernel_metrics.active_mode;
            user_metrics.ready_depth = kernel_metrics.ready_depth;
            user_metrics.zombie_depth = kernel_metrics.zombie_depth;

            ret = copy_to_user(user_buf, &user_metrics, sizeof(user_metrics)) ? 0 : (uint64_t)-1;
            break;
        }

        case SYS_SCHED_AUTH_ROOT_MINT:
            ret = sys_sched_auth_root_mint_handler(owner,
                                                    (uint32_t)a0,
                                                    (mode_id_t)a1,
                                                    a2,
                                                    a3,
                                                    (uint32_t)a4);
            break;


        case SYS_SCHED_AUTH_THREAD_DERIVE: {
            uint32_t root_slot = (uint32_t)a0;
            uint32_t dst_slot = (uint32_t)a1;
            uint32_t max_slice = (uint32_t)a2;
            uint32_t weight = (uint32_t)a3;
            uint64_t local_max_accumulated = a4;
            thread_t* current = scheduler_get_current();

            if (!owner || !owner->cspace || !current || current->owner != owner) {
                g_sys_metrics.ownerless_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            ret = (scheduler_derive_thread_auth(owner,
                                                current,
                                                root_slot,
                                                dst_slot,
                                                max_slice,
                                                weight,
                                                local_max_accumulated) == 0) ? 0 : (uint64_t)-1;
            break;
        }

        case SYS_GENESIS_INVOKE:
            ret = genesis_syscall_dispatch(owner, (uint32_t)a0, (uint32_t)a1, a2, false);
            break;

        case SYS_FRAME_ALLOC: {
            uint32_t slot = (uint32_t)a0;
            process_t* proc = owner;
            if (!proc || !proc->cspace) {
                g_sys_metrics.ownerless_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            /* Allocate a frame from PMM */
            uint64_t phys = pmm_alloc(COLOR_CASUAL, proc->pid);
            if (!phys) {
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            /* Create RAM Capability */
            cap_identity_t* ident = cap_identity_create(phys, CAP_TYPE_RAM, CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_EXECUTE | CAP_RIGHT_GRANT, 0, CAP_MODE_ALL);
            if (!ident) {
                pmm_free(phys);
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
                break;
            }

            /* Insert into CSpace */
            if (cap_insert(proc->cspace, slot, ident) != 0) {
                cap_identity_free(ident);
                pmm_free(phys);
                g_sys_metrics.invalid_rejects++;
                ret = (uint64_t)-1;
            } else {
                ret = 0;
            }
            break;
        }

        default:
            g_sys_metrics.unknown_calls++;
            g_sys_metrics.invalid_rejects++;
            ret = (uint64_t)-1;
            break;
    }

    syscall_diag_emit_periodic();
    return ret;
}

void syscall_set_kernel_stack(uint64_t stack_virt) {
    cpu_syscall_gs.kernel_rsp = stack_virt;
}
