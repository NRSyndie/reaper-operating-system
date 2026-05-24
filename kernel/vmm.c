#include "include/vmm.h"
#include "include/pmm.h"
#include "include/mode.h"
#include "include/cpu.h"
#include "include/console.h"
#include "include/limine.h"
#include "include/utils.h"
#include "include/klog.h"
#include "include/scheduler.h"
#include "include/thread.h"
#include "include/ipc.h"
#include "include/capability.h"
#include "include/pcid.h" // Added for PCID colorization validation

extern struct limine_memmap_request memmap_request;
extern struct limine_hhdm_request hhdm_request;

extern char kernel_start[];
extern char kernel_end[];

static pt_entry_t* kernel_pml4 = NULL;
static uint64_t hhdm_offset;
static vmm_region_contract_t vmm_contract_log[128];
static uint32_t vmm_contract_log_head = 0;
static uint32_t vmm_contract_log_count = 0;
static vmm_contract_metrics_t vmm_contract_metrics = {0};

vmm_statistics_t vmm_stats = {0};

static inline void* phys_to_virt(uint64_t phys) {
    return (void*)(phys + hhdm_offset);
}

static inline pt_entry_t* proc_root(process_t* proc) {
    return proc ? (pt_entry_t*)phys_to_virt(proc->pml4_phys) : kernel_pml4;
}

static pt_entry_t* get_next_level(pt_entry_t* current, uint64_t index, process_t* proc) {
    if (current[index] & VMM_PRESENT) {
        return phys_to_virt(current[index] & ~0xFFFULL);
    }

    /* Auto-allocation for Kernel/Genesis/ELF operations */
    /* Note: Syscalls should NOT use vmm_map if they want to enforce Shadow Mapping manually. */
    // Use the process's mode for pmm_alloc if proc is available, otherwise use COLOR_VOID (default/kernel)
    uint64_t new_table_phys = pmm_alloc(proc ? mode_to_color(proc->mode) : COLOR_VOID, proc ? proc->pid : 0);
    if (!new_table_phys) return NULL;
    
    // We must zero the new table!
    pt_entry_t* virt = phys_to_virt(new_table_phys);
    fast_zero(virt, PAGE_SIZE);

    current[index] = new_table_phys | VMM_PRESENT | VMM_WRITABLE | (proc ? VMM_USER : 0);

    return virt;
}

void vmm_set_entry(uint64_t table_phys, uint64_t index, uint64_t value) {
    pt_entry_t* table = phys_to_virt(table_phys);
    table[index] = value;
}

static bool vmm_walk_leaf_noalloc(pt_entry_t* pml4, uint64_t virt, pt_entry_t** out_pt, uint64_t* out_pt_idx) {
    if (!pml4 || !out_pt || !out_pt_idx) return false;
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & VMM_PRESENT)) return false;
    pt_entry_t* pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) return false;
    pt_entry_t* pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & VMM_PRESENT)) return false;

    if (pd[pd_idx] & VMM_HUGE) {
        *out_pt = pd;
        *out_pt_idx = pd_idx;
        return true;
    }

    pt_entry_t* pt = phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    *out_pt = pt;
    *out_pt_idx = pt_idx;
    return true;
}

static bool vmm_unmap_leaf_noalloc(pt_entry_t* pml4, uint64_t virt) {
    pt_entry_t* pt;
    uint64_t pt_idx;
    if (!vmm_walk_leaf_noalloc(pml4, virt, &pt, &pt_idx)) return false;
    if (!(pt[pt_idx] & VMM_PRESENT)) return false;
    pt[pt_idx] = 0;
    (void)cpu_tlb_shootdown_page(virt);
    return true;
}

static void vmm_contract_log_push(const vmm_region_contract_t* contract) {
    if (!contract) return;
    vmm_contract_log[vmm_contract_log_head] = *contract;
    vmm_contract_log_head = (vmm_contract_log_head + 1) % (sizeof(vmm_contract_log) / sizeof(vmm_contract_log[0]));
    if (vmm_contract_log_count < (sizeof(vmm_contract_log) / sizeof(vmm_contract_log[0]))) {
        vmm_contract_log_count++;
    }
}

static bool vmm_map_raw(process_t* proc, uint64_t virt, uint64_t phys, uint64_t flags) {
    GASSERT_HARD(virt % PAGE_SIZE == 0, "Virtual address must be page-aligned");
    if (!(flags & VMM_DEMAND)) {
        GASSERT_HARD(phys % PAGE_SIZE == 0, "Physical address must be page-aligned");
    }

    pt_entry_t* pml4 = kernel_pml4;
    if (proc) pml4 = phys_to_virt(proc->pml4_phys);
    
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    /* Phase 1: Ensure path exists (consumes bricks) */
    pt_entry_t* pdpt = get_next_level(pml4, pml4_idx, proc);
    if (!pdpt) return false;

    pt_entry_t* pd = get_next_level(pdpt, pdpt_idx, proc);
    if (!pd) return false;

    if (flags & VMM_HUGE) {
        /* Ensure we only pass relevant bits to the PDE huge page entry */
        /* Only keep VMM_HUGE, VMM_PRESENT, VMM_WRITABLE, VMM_USER, VMM_NX, VMM_ACCESSED, VMM_DIRTY */
        uint64_t pde_flags = flags & (VMM_HUGE | VMM_PRESENT | VMM_WRITABLE | VMM_USER | VMM_NX | VMM_ACCESSED | VMM_DIRTY);
        pd[pd_idx] = (phys & ~0x1FFFFFULL) | pde_flags;
        klog_debug("VMM_MAP_RAW: vaddr=0x%lx pde=0x%lx flags=0x%lx\n", 
           virt, pd[pd_idx], flags);
        return true;
    }

    pt_entry_t* pt = get_next_level(pd, pd_idx, proc);
    if (!pt) return false;

    /* Phase 2: Set the leaf */
    if (flags & VMM_DEMAND) {
        /* Only store software-meaningful bits in the demand placeholder */
        pt[pt_idx] = flags & (VMM_DEMAND | VMM_COW | VMM_USER | VMM_WRITABLE);
    } else {
        /* Ensure we only pass relevant bits to the PTE entry */
        uint64_t pte_flags = flags & (VMM_PRESENT | VMM_WRITABLE | VMM_USER | VMM_NX | VMM_ACCESSED | VMM_DIRTY | VMM_COW | VMM_DEMAND);
        pt[pt_idx] = (phys & ~0xFFFULL) | pte_flags;
    }
    return true;
}

static bool vmm_contract_fail(vmm_compiled_mapping_t* compiled, vmm_contract_result_t result) {
    if (compiled) {
        compiled->result = result;
    }
    vmm_contract_metrics.compile_fail++;
    return false;
}

bool vmm_compile_region_contract(const vmm_region_contract_t* contract, vmm_compiled_mapping_t* compiled) {
    if (!contract || !compiled) return false;
    compiled->result = VMM_CONTRACT_RESULT_NONE;

    if (contract->op != VMM_CONTRACT_OP_MAP && contract->op != VMM_CONTRACT_OP_UNMAP) {
        return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_OP);
    }
    if (contract->length == 0 || (contract->length % PAGE_SIZE) != 0) {
        return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_LENGTH);
    }
    if ((contract->virt_start % PAGE_SIZE) != 0) {
        return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_ALIGNMENT);
    }
    uint64_t max_virt = contract->virt_start + contract->length;
    if (max_virt < contract->virt_start) {
        return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_OVERFLOW);
    }

    if (contract->trust != VMM_CONTRACT_TRUST_VERIFIED &&
        contract->trust != VMM_CONTRACT_TRUST_UNVERIFIED) {
        return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_POLICY);
    }

    bool owner_is_user = (contract->owner_mode != MODE_KERNEL);
    if (owner_is_user) {
        if (contract->virt_start >= KERNEL_VIRT_BASE || max_virt > KERNEL_VIRT_BASE) {
            return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_POLICY);
        }
    }

    if (contract->op == VMM_CONTRACT_OP_MAP) {
        if ((contract->phys_start % PAGE_SIZE) != 0) {
            return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_ALIGNMENT);
        }
        uint64_t max_phys = contract->phys_start + contract->length;
        if (max_phys < contract->phys_start) {
            return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_OVERFLOW);
        }

        /* Demand paging: Allow phys_start=0 if VMM_DEMAND is set */
        if (contract->phys_start == 0 && !(contract->pte_flags & VMM_DEMAND)) {
             // Normal map needs physical address
             // return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_FLAGS);
        }

        if (contract->pte_flags & VMM_DEMAND) {
            if (contract->pte_flags & (VMM_PRESENT | VMM_NX | VMM_ACCESSED | VMM_DIRTY)) {
                return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_FLAGS);
            }
        }

        /* Huge pages: Only 2MB supported for now, must be aligned */
        if (contract->pte_flags & VMM_HUGE) {
            if (contract->length != (2 * 1024 * 1024)) {
                return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_LENGTH);
            }
            if ((contract->virt_start % (2 * 1024 * 1024)) != 0 || 
                (contract->phys_start % (2 * 1024 * 1024)) != 0) {
                return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_ALIGNMENT);
            }
        }

        if (owner_is_user && !(contract->pte_flags & VMM_USER)) {
            return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_POLICY);
        }
    } else {
        if (contract->phys_start != 0 || contract->pte_flags != 0) {
            return vmm_contract_fail(compiled, VMM_CONTRACT_RESULT_ERR_FLAGS);
        }
    }

    compiled->op = contract->op;
    compiled->virt_start = contract->virt_start;
    compiled->phys_start = contract->phys_start;
    compiled->page_count = contract->length / PAGE_SIZE;
    compiled->pte_flags = contract->pte_flags;
    compiled->owner_pid = contract->owner_pid;
    compiled->owner_mode = contract->owner_mode;
    compiled->trust = contract->trust;
    compiled->result = VMM_CONTRACT_RESULT_OK;
    vmm_contract_metrics.compile_ok++;
    return true;
}

bool vmm_apply_compiled_mapping(process_t* proc, const vmm_compiled_mapping_t* compiled) {
    if (!compiled || compiled->page_count == 0) return false;

    vmm_compiled_mapping_t local = *compiled;
    pt_entry_t* pml4 = proc_root(proc);
    uint64_t applied_pages = 0;
    bool ok = true;

    if (!pml4) {
        local.result = VMM_CONTRACT_RESULT_ERR_WALK;
        ok = false;
    }

    if (ok && local.op == VMM_CONTRACT_OP_MAP) {
        for (uint64_t i = 0; i < local.page_count; i++) {
            pt_entry_t* pt;
            uint64_t pt_idx;
            uint64_t virt = local.virt_start + (i * PAGE_SIZE);
            if (vmm_walk_leaf_noalloc(pml4, virt, &pt, &pt_idx) && (pt[pt_idx] & VMM_PRESENT)) {
                local.result = VMM_CONTRACT_RESULT_ERR_ALREADY_MAPPED;
                ok = false;
                break;
            }
        }
    }

    if (ok && local.op == VMM_CONTRACT_OP_UNMAP) {
        for (uint64_t i = 0; i < local.page_count; i++) {
            pt_entry_t* pt;
            uint64_t pt_idx;
            uint64_t virt = local.virt_start + (i * PAGE_SIZE);
            if (!vmm_walk_leaf_noalloc(pml4, virt, &pt, &pt_idx) || !(pt[pt_idx] & VMM_PRESENT)) {
                local.result = VMM_CONTRACT_RESULT_ERR_NOT_MAPPED;
                ok = false;
                break;
            }
        }
    }

    if (ok && local.op == VMM_CONTRACT_OP_MAP) {
        for (uint64_t i = 0; i < local.page_count; i++) {
            uint64_t virt = local.virt_start + (i * PAGE_SIZE);
            uint64_t phys = local.phys_start + (i * PAGE_SIZE);
            if (!vmm_map_raw(proc, virt, phys, local.pte_flags)) {
                local.result = VMM_CONTRACT_RESULT_ERR_ALLOC;
                ok = false;
                break;
            }
            applied_pages++;
        }

        if (!ok && applied_pages > 0) {
            vmm_contract_metrics.rollback_attempts++;
            bool rollback_ok = true;
            for (uint64_t i = 0; i < applied_pages; i++) {
                uint64_t virt = local.virt_start + (i * PAGE_SIZE);
                if (!vmm_unmap_leaf_noalloc(pml4, virt)) {
                    rollback_ok = false;
                }
            }
            if (!rollback_ok) {
                vmm_contract_metrics.rollback_failures++;
                local.result = VMM_CONTRACT_RESULT_ERR_ROLLBACK;
            }
        }
    } else if (ok && local.op == VMM_CONTRACT_OP_UNMAP) {
        for (uint64_t i = 0; i < local.page_count; i++) {
            uint64_t virt = local.virt_start + (i * PAGE_SIZE);
            if (!vmm_unmap_leaf_noalloc(pml4, virt)) {
                local.result = VMM_CONTRACT_RESULT_ERR_WALK;
                ok = false;
                break;
            }
            applied_pages++;
        }
    } else if (ok) {
        local.result = VMM_CONTRACT_RESULT_ERR_OP;
        ok = false;
    }

    if (ok) local.result = VMM_CONTRACT_RESULT_OK;
    if (ok) vmm_contract_metrics.apply_ok++;
    else vmm_contract_metrics.apply_fail++;

    vmm_region_contract_t contract = {
        .op = local.op,
        .virt_start = local.virt_start,
        .phys_start = local.phys_start,
        .length = local.page_count * PAGE_SIZE,
        .pte_flags = local.pte_flags,
        .owner_pid = local.owner_pid,
        .owner_mode = local.owner_mode,
        .trust = local.trust,
        .result = local.result,
        .applied_pages = applied_pages
    };
    vmm_contract_log_push(&contract);
    return ok;
}

int vmm_read_recent_contracts(vmm_region_contract_t* out, size_t max_count) {
    if (!out || max_count == 0) return 0;
    uint32_t available = vmm_contract_log_count;
    uint32_t to_copy = (available < max_count) ? available : (uint32_t)max_count;

    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t idx = (vmm_contract_log_head + (sizeof(vmm_contract_log) / sizeof(vmm_contract_log[0])) - 1 - i) %
                       (sizeof(vmm_contract_log) / sizeof(vmm_contract_log[0]));
        out[i] = vmm_contract_log[idx];
    }
    return (int)to_copy;
}

bool vmm_map(process_t* proc, uint64_t virt, uint64_t phys, uint64_t flags) {
    vmm_region_contract_t contract = {
        .op = VMM_CONTRACT_OP_MAP,
        .virt_start = virt,
        .phys_start = phys,
        .length = PAGE_SIZE,
        .pte_flags = flags,
        .owner_pid = proc ? proc->pid : 0,
        .owner_mode = proc ? (uint8_t)proc->mode : (uint8_t)MODE_KERNEL,
        .trust = VMM_CONTRACT_TRUST_VERIFIED,
        .result = VMM_CONTRACT_RESULT_NONE,
        .applied_pages = 0
    };
    vmm_compiled_mapping_t compiled;
    if (!vmm_compile_region_contract(&contract, &compiled)) {
        contract.result = compiled.result;
        vmm_contract_log_push(&contract);
        return false;
    }
    return vmm_apply_compiled_mapping(proc, &compiled);
}

uint64_t vmm_virt_to_phys(pt_entry_t* pml4, uint64_t virt) {
    if (!pml4) pml4 = kernel_pml4;
    pt_entry_t* pt;
    uint64_t pt_idx;
    if (!vmm_walk_leaf_noalloc(pml4, virt, &pt, &pt_idx)) return 0;
    if (!(pt[pt_idx] & VMM_PRESENT)) return 0;
    return (pt[pt_idx] & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

bool vmm_unmap_region(process_t* proc, uint64_t virt, uint64_t length) {
    vmm_region_contract_t contract = {
        .op = VMM_CONTRACT_OP_UNMAP,
        .virt_start = virt,
        .phys_start = 0,
        .length = length,
        .pte_flags = 0,
        .owner_pid = proc ? proc->pid : 0,
        .owner_mode = proc ? (uint8_t)proc->mode : (uint8_t)MODE_KERNEL,
        .trust = VMM_CONTRACT_TRUST_VERIFIED,
        .result = VMM_CONTRACT_RESULT_NONE,
        .applied_pages = 0
    };
    vmm_compiled_mapping_t compiled;
    if (!vmm_compile_region_contract(&contract, &compiled)) {
        contract.result = compiled.result;
        vmm_contract_log_push(&contract);
        return false;
    }
    return vmm_apply_compiled_mapping(proc, &compiled);
}

bool vmm_get_contract_metrics(vmm_contract_metrics_t* out_metrics) {
    if (!out_metrics) return false;
    *out_metrics = vmm_contract_metrics;
    return true;
}

bool vmm_contract_self_test(void) {
    uint64_t pml4_phys = vmm_fork_pml4();
    if (!pml4_phys) {
        kprintf("[TEST] VMM contract self-test fail: fork_pml4\n");
        return false;
    }

    process_t* proc = process_create(pml4_phys, pcid_alloc(MODE_CASUAL), cnode_create(), MODE_CASUAL);
    if (!proc) {
        kprintf("[TEST] VMM contract self-test fail: process_create\n");
        vmm_destroy_pml4(pml4_phys);
        return false;
    }

    bool ok = true;
    bool mapped_a = false;
    bool mapped_b = false;
    uint64_t phys_a = pmm_alloc(mode_to_color(proc->mode), proc->pid);
    uint64_t phys_b = pmm_alloc(mode_to_color(proc->mode), proc->pid);
    if (!phys_a || !phys_b) {
        kprintf("[TEST] VMM contract self-test fail: pmm_alloc\n");
        ok = false;
        goto cleanup;
    }

    vmm_region_contract_t map_contract = {
        .op = VMM_CONTRACT_OP_MAP,
        .virt_start = 0x500000,
        .phys_start = phys_a,
        .length = PAGE_SIZE,
        .pte_flags = PAGE_USER_DATA,
        .owner_pid = proc->pid,
        .owner_mode = (uint8_t)proc->mode,
        .trust = VMM_CONTRACT_TRUST_VERIFIED,
        .result = VMM_CONTRACT_RESULT_NONE,
        .applied_pages = 0
    };
    vmm_compiled_mapping_t compiled;
    if (!vmm_compile_region_contract(&map_contract, &compiled)) {
        kprintf("[TEST] VMM contract self-test fail: compile_map (%d)\n", compiled.result);
        ok = false;
        goto cleanup;
    }

    compiled.phys_start = phys_a;
    if (!vmm_apply_compiled_mapping(proc, &compiled)) {
        kprintf("[TEST] VMM contract self-test fail: apply_map\n");
        ok = false;
        goto cleanup;
    }
    mapped_a = true;

    if ((vmm_virt_to_phys((pt_entry_t*)phys_to_virt(proc->pml4_phys), 0x500000) & ~0xFFFULL) != (phys_a & ~0xFFFULL)) {
        kprintf("[TEST] VMM contract self-test fail: verify_map_a\n");
        ok = false;
        goto cleanup;
    }
    if (!vmm_map(proc, 0x501000, phys_b, PAGE_USER_DATA)) {
        kprintf("[TEST] VMM contract self-test fail: map_b\n");
        ok = false;
        goto cleanup;
    }
    mapped_b = true;

    if (vmm_map(proc, 0x500000, phys_b, PAGE_USER_DATA)) {
        kprintf("[TEST] VMM contract self-test fail: overlap_expected_fail\n");
        ok = false;
        goto cleanup;
    }

    if ((vmm_virt_to_phys((pt_entry_t*)phys_to_virt(proc->pml4_phys), 0x501000) & ~0xFFFULL) != (phys_b & ~0xFFFULL)) {
        kprintf("[TEST] VMM contract self-test fail: verify_map_b\n");
        ok = false;
        goto cleanup;
    }

    if (!vmm_unmap_region(proc, 0x500000, PAGE_SIZE * 2)) {
        kprintf("[TEST] VMM contract self-test fail: unmap_region\n");
        ok = false;
        goto cleanup;
    }
    mapped_a = false;
    mapped_b = false;
    if (vmm_virt_to_phys((pt_entry_t*)phys_to_virt(proc->pml4_phys), 0x500000) != 0) {
        kprintf("[TEST] VMM contract self-test fail: verify_unmap_a\n");
        ok = false;
        goto cleanup;
    }
    if (vmm_virt_to_phys((pt_entry_t*)phys_to_virt(proc->pml4_phys), 0x501000) != 0) {
        kprintf("[TEST] VMM contract self-test fail: verify_unmap_b\n");
        ok = false;
        goto cleanup;
    }

cleanup:
    if (!mapped_a && phys_a) pmm_free(phys_a);
    if (!mapped_b && phys_b) pmm_free(phys_b);
    process_destroy(proc);
    return ok;
}

void vmm_unmap(pt_entry_t* pml4, uint64_t virt) {
    if (!pml4) {
        (void)vmm_unmap_region(NULL, virt, PAGE_SIZE);
        return;
    }
    (void)vmm_unmap_leaf_noalloc(pml4, virt);
}

void vmm_switch(uint64_t pml4_phys, uint16_t pcid, mode_id_t mode) { // Modified signature
    if (pcid > PCID_MAX) { // Use PCID_MAX from pcid.h
        vmm_stats.pcid_switch_rejects++;
        klog_critical("VMM: Attempted to switch to invalid PCID %u (max %u). PANIC!", pcid, PCID_MAX);
        kpanic("PCID COLORIZATION VIOLATION: Invalid PCID value.");
    }

    if ((pml4_phys & 0xFFFULL) != 0) {
        vmm_stats.pcid_switch_rejects++;
        klog_critical("VMM: Attempted to switch with unaligned PML4 0x%lx. PANIC!", pml4_phys);
        kpanic("VMM SWITCH VIOLATION: Unaligned PML4.");
    }

    if (mode <= MODE_VOID || mode > MODE_KERNEL) {
        vmm_stats.pcid_switch_rejects++;
        klog_critical("VMM: Attempted to switch with invalid mode %d. PANIC!", mode);
        kpanic("PCID COLORIZATION VIOLATION: Invalid mode.");
    }

    // --- PCID Colorization Validation ---
    // Check if the provided PCID is valid for the given mode
    uint16_t expected_mode_base = pcid_get_mode_base(mode);
    uint16_t expected_mode_count = pcid_get_mode_count(mode);

    if (mode == MODE_KERNEL) {
        if (pcid != PCID_KERNEL && pcid != PCID_KERNEL_SECURE) {
            vmm_stats.pcid_switch_rejects++;
            klog_critical("VMM: PCID Colorization Violation! Attempted to switch to non-kernel PCID %u while in MODE_KERNEL context. PANIC!", pcid);
            kpanic("PCID COLORIZATION VIOLATION: Kernel tried to use non-kernel PCID.");
        }
    } else { // User modes
        if (pcid < expected_mode_base || pcid >= (expected_mode_base + expected_mode_count)) {
            vmm_stats.pcid_switch_rejects++;
            klog_critical("VMM: PCID Colorization Violation! PCID %u is outside range [%u-%u] for mode %d. PANIC!",
                          pcid, expected_mode_base, expected_mode_base + expected_mode_count - 1, mode);
            kpanic("PCID COLORIZATION VIOLATION: Invalid PCID for target mode.");
        }
    }

    // Update VMM Stats from PCID Manager
    vmm_stats.max_pcid_casual = (uint16_t)pcid_manager.max_count_casual;
    vmm_stats.max_pcid_secure = (uint16_t)pcid_manager.max_count_secure;
    vmm_stats.max_pcid_lockdown = (uint16_t)pcid_manager.max_count_lockdown;
    vmm_stats.max_pcid_ghost = (uint16_t)pcid_manager.max_count_ghost;

    // --- End PCID Colorization Validation ---

    uint64_t cr3_val = pml4_phys & ~0xFFF;

    if (read_cr4() & (1 << 17)) {
        // PCID is enabled
        cr3_val |= (pcid & 0xFFF);

        // NOFLUSH bit: Preserve TLB entries for this PCID
        // CRITICAL: If we are in GHOST mode, we force a flush to ensure 100% isolation.
        // The NOFLUSH bit being 0 means an implicit flush.
        if (mode != MODE_GHOST) {
            cr3_val |= (1ULL << 63); // Set NOFLUSH bit
        } else {
            vmm_stats.pcid_switch_flushes++;
            // For GHOST mode, we explicitly do NOT set the NOFLUSH bit,
            // which results in an implicit flush of the old context's TLB.
            klog_debug("VMM: GHOST mode switch detected. Enforcing TLB flush (NOFLUSH=0).");
        }
    }

    write_cr3(cr3_val);
    vmm_stats.pcid_switches++;
    klog_debug("VMM: Switched to PML4 0x%lx, PCID %u, Mode %d.", pml4_phys, pcid, mode);
}

uint16_t vmm_get_current_pcid(void) {
    return (uint16_t)(read_cr3() & 0xFFF);
}

void invpcid_flush_single(uint16_t pcid, uint64_t addr) {
    invpcid(INVPCID_TYPE_INDIVIDUAL_ADDR, pcid, addr);
}

void invpcid_flush_context(uint16_t pcid) {
    invpcid(INVPCID_TYPE_SINGLE_CONTEXT, pcid, 0);
}

void invpcid_flush_all(void) {
    invpcid(INVPCID_TYPE_ALL_CONTEXTS, 0, 0);
}

uint64_t vmm_fork_pml4(void) {
    uint64_t new_pml4_phys = pmm_alloc(COLOR_VOID, 0);
    if (!new_pml4_phys) {
        klog_error("VMM: Failed to allocate physical memory for new PML4.");
        return 0;
    }

    
    pt_entry_t* new_pml4 = phys_to_virt(new_pml4_phys);
    fast_zero(new_pml4, PAGE_SIZE);

    // CRITICAL: Always fork from the kernel's master PML4.
    // This ensures a clean slate (Kernel mapped, User empty).
    pt_entry_t* src_pml4 = kernel_pml4;

    // Only copy the kernel half (256-511)
    // The lower half (0-255) must be empty for the new process.
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = src_pml4[i];
    }

    return new_pml4_phys;
}

static void destroy_table_recursive(pt_entry_t* table, int level) {
    for (int i = 0; i < 512; i++) {
        if (table[i] & VMM_PRESENT) {
            uint64_t phys = table[i] & ~0xFFFULL;
            // The kernel PML4 entries (256-511) are cloned,
            // so we must ensure we do not free kernel tables.
            // Check if the physical address is within the kernel's mapped range
            // This is a rough check, a more robust solution would track ownership.
            if ((phys >= (uint64_t)kernel_start && phys < (uint64_t)kernel_end)) {
                continue; // Do not free kernel tables.
            }

            /* If not a leaf node (and not a huge page), recurse */
            if (level > 1 && !(table[i] & VMM_HUGE)) {
                destroy_table_recursive(phys_to_virt(phys), level - 1);
            }
            
            /* Free the leaf frame (or the sub-table frame) */
            pmm_free(phys);
            table[i] = 0;
        }
    }
}

void vmm_destroy_pml4(uint64_t pml4_phys) {
    /* CRITICAL: Never destroy the universe you are currently standing in! */
    if ((read_cr3() & ~0xFFFULL) == (pml4_phys & ~0xFFFULL)) {
        klog_error("VMM: Attempted to destroy current active PML4 0x%lx. Ignoring.", pml4_phys);
        return;
    }

    pt_entry_t* pml4 = phys_to_virt(pml4_phys);

    /* ONLY destroy the user half (0-255) */
    for (int i = 0; i < 256; i++) {
        if (pml4[i] & VMM_PRESENT) {
            uint64_t pdpt_phys = pml4[i] & ~0xFFFULL;
            destroy_table_recursive(phys_to_virt(pdpt_phys), 3);
            pmm_free(pdpt_phys);
            pml4[i] = 0; // Clear the entry after freeing
        }
    }

    /* Finally free the PML4 itself */
    pmm_free(pml4_phys);
    klog_debug("VMM: Destroyed PML4 0x%lx.", pml4_phys);
}

void vmm_init(void) {
    kprintf("[VMM] Initializing Virtual Memory Manager...\n");
    if (!hhdm_request.response) kpanic("VMM: HHDM response missing!");
    hhdm_offset = hhdm_request.response->offset;

        
    uint64_t kernel_pml4_phys = pmm_alloc(COLOR_VOID, 0);
    kernel_pml4 = phys_to_virt(kernel_pml4_phys);
    fast_zero(kernel_pml4, PAGE_SIZE);

    // Get current PML4 to clone from
    uint64_t old_pml4_phys;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(old_pml4_phys));
    pt_entry_t* old_pml4 = phys_to_virt(old_pml4_phys);

    // Clone the bootloader's mappings (PML4 entries)
    // This ensures we keep the identity maps, kernel maps, and HHDM exactly
    // as Limine set them up, but now on our own tables.
    for (int i = 0; i < 512; i++) {
        if (old_pml4[i] & VMM_PRESENT) {
            kernel_pml4[i] = old_pml4[i];
        }
    }

     
    vmm_switch(kernel_pml4_phys, PCID_KERNEL, MODE_KERNEL); // Updated call site
}

static void vmm_verify_init(void) {
    klog_info("VMM: Initialized. HHDM offset: 0x%lx. Kernel PML4: 0x%lx.", hhdm_offset, (uint64_t)kernel_pml4);
}

#include "include/init.h"
initcall(vmm_verify_init);

// --- Debugging Tools ---

void vmm_check_efer(void) {
    uint64_t efer = rdmsr(MSR_EFER);
        if (efer & (1 << 11)) {
            klog_info("VMM: EFER.NXE (No-Execute) is enabled.");
            } else {
            klog_warn("VMM: EFER.NXE (No-Execute) is DISABLED. Security risk!");
            }
}

void vmm_dump_page_walk(uint64_t pml4_phys, uint64_t virt) {
    kprintf("VMM: Dumping page walk for VA 0x%lx in PML4 0x%lx.\n", virt, pml4_phys);
    pt_entry_t* pml4 = phys_to_virt(pml4_phys);
    
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    kprintf("PML4[0x%lx]: 0x%lx\n", pml4_idx, pml4[pml4_idx]);
    if (!(pml4[pml4_idx] & VMM_PRESENT)) { kprintf("  -> Not Present\n"); return; }
    
    pt_entry_t* pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    kprintf("  PDPT[0x%lx]: 0x%lx\n", pdpt_idx, pdpt[pdpt_idx]);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) { kprintf("    -> Not Present\n"); return; }

    pt_entry_t* pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
    kprintf("    PD[0x%lx]: 0x%lx\n", pd_idx, pd[pd_idx]);
    if (!(pd[pd_idx] & VMM_PRESENT)) { kprintf("      -> Not Present\n"); return; }

    pt_entry_t* pt = phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    kprintf("      PT[0x%lx]: 0x%lx\n", pt_idx, pt[pt_idx]);
    if (!(pt[pt_idx] & VMM_PRESENT)) { kprintf("        -> Not Present\n"); return; }
    
    kprintf("        -> Mapped Physical: 0x%lx, Flags: 0x%lx\n", pt[pt_idx] & ~0xFFFULL, pt[pt_idx] & 0xFFF);
}
bool vmm_handle_fault(uint64_t vaddr, uint64_t error_code) {
    uint64_t active_cr3 = read_cr3() & ~0xFFFULL;
    pt_entry_t* pml4 = (pt_entry_t*)phys_to_virt(active_cr3);
    if (!pml4) return false;

    process_t* proc = scheduler_get_current() ? scheduler_get_current()->owner : NULL;
    
    uint64_t virt = vaddr & ~0xFFFULL;
    pt_entry_t* pt;
    uint64_t pt_idx;
    
    /* 1. Walk the table to find the entry. */
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & VMM_PRESENT)) { kprintf("VMM: Missing PML4E for addr 0x%lx\n", vaddr); return false; }
    pt_entry_t* pdpt = phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) { kprintf("VMM: Missing PDPTE for addr 0x%lx\n", vaddr); return false; }
    pt_entry_t* pd = phys_to_virt(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & VMM_PRESENT)) { 
        kprintf("VMM: Missing PDE for addr 0x%lx (PD index 0x%lx)\n", vaddr, pd_idx); 
        return false; 
    }
    
    /* Handle Huge Page mapping */
    if (pd[pd_idx] & VMM_HUGE) {
        if (pd[pd_idx] & VMM_PRESENT) return false;
        kprintf("VMM: Huge page encountered at 0x%lx, cannot demand page\n", vaddr);
        return false;
    }
    
    pt = (pt_entry_t*)phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    uint64_t entry = pt[pt_idx];

    /* Case A: Demand Paging (Not Present, but VMM_DEMAND bit set) */
    if (!(entry & VMM_PRESENT) && (entry & VMM_DEMAND)) {
        uint64_t new_frame = pmm_alloc(proc ? mode_to_color(proc->mode) : COLOR_VOID, proc ? proc->pid : 0);
        if (!new_frame) return false;

        hyper_scrub(phys_to_virt(new_frame), PAGE_SIZE / 8);
        pt[pt_idx] = (new_frame & ~0xFFFULL) | (entry & 0xFFF) | VMM_PRESENT;
        
        cpu_tlb_shootdown_page(virt);
        return true;
    }
/* Case B: Copy-on-Write (Present, Read-only, VMM_COW set, and Write fault) */
bool is_write = (error_code & 2);
if ((entry & VMM_PRESENT) && (entry & VMM_COW) && is_write && !(entry & VMM_WRITABLE)) {
    uint64_t old_frame = entry & 0x000FFFFFFFFFF000ULL;
    uint64_t new_frame = pmm_alloc(proc ? mode_to_color(proc->mode) : COLOR_VOID, proc ? proc->pid : 0);
    if (!new_frame) return false;

    void* dst = (void*)(new_frame + hhdm_offset);
    void* src = (void*)(old_frame + hhdm_offset);

    kprintf("VMM: COW Copy - old=0x%lx new=0x%lx src=0x%lx dst=0x%lx\n",
            old_frame, new_frame, (uint64_t)src, (uint64_t)dst);

    memcpy(dst, src, PAGE_SIZE);

    /* Map new frame as WRITABLE, remove COW bit */
    pt[pt_idx] = (new_frame & 0x000FFFFFFFFFF000ULL) | (entry & 0xFFF & ~VMM_COW) | VMM_WRITABLE | VMM_PRESENT;

    /* Flush TLB for this address */
    cpu_tlb_shootdown_page(virt);
    return true;
}

    return false;
}
