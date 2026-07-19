#include "include/bootinfo.h"
#include "include/process.h"
#include "include/capability.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/pcid.h"
#include "include/slab.h"
#include "include/thread.h"
#include "include/scheduler.h"
#include "include/utils.h"
#include "include/limine.h"
#include "include/console.h"
#include "include/klog.h"
#include "include/elf.h"
#include "include/genesis.h"
#include "include/entry_internal.h"

extern struct limine_memmap_request memmap_request;
extern struct limine_hhdm_request hhdm_request;
extern struct limine_executable_address_request executable_address_request;
extern struct limine_module_request module_request;
extern char kernel_start[];
extern char kernel_end[];

/* The Genesis Bridge: Virtual Address for Boot Info */
#define BOOTINFO_VIRT_ADDR 0x1000
#define PARADIGM_STACK_TOP  0x800000
#define PARADIGM_STACK_PAGES 8

/* 
 * We need a way to pass the Entry Point to the trampoline.
 * For now, we'll store it in a static variable since Genesis is a singleton event.
 */
static uint64_t paradigm_entry_point = 0;
static uint32_t paradigm_pid = 0;

/*
 * genesis_copy_from_user: local safe-copy from user address space.
 * Mirrors syscall.c's copy_from_user; duplicated here to keep genesis.c
 * a self-contained compilation unit without polluting the kernel ABI
 * with an exported copy_from_user symbol.
 */
#define USER_VA_LIMIT 0x0000800000000000ULL
static bool genesis_copy_from_user(void* dest, const void* src, size_t n) {
    if (n == 0) { return true; }
    if ((uint64_t)src >= USER_VA_LIMIT) return false;
    if (n > USER_VA_LIMIT || (uint64_t)src + n > USER_VA_LIMIT) return false;
    memcpy(dest, src, n);
    return true;
}

/*
 * genesis_is_delegatable_type: whitelist of cap types Genesis authority
 * may inject directly into a target process's cspace, bypassing the
 * caller's own possession of that authority.  All other types must go
 * through the normal cap_mint / SYS_CAP_MINT path.
 */
static bool genesis_is_delegatable_type(cap_type_t t) {
    return t == CAP_TYPE_REALITY_CTRL   ||
           t == CAP_TYPE_AUDIT_WRITE    ||
           t == CAP_TYPE_SCHED_AUTH     ||
           t == CAP_TYPE_SCHED_AUTH_ROOT;
}

static void paradigm_entry_stub(void) {
    entry_pipeline_run(scheduler_get_current(), paradigm_entry_point, GENESIS_DEFAULT_STACK_TOP);
}

bool genesis_bootinfo_init(boot_info_t* bootinfo, uint32_t genesis_cap_slot) {
    if (!bootinfo) return false;

    fast_zero(bootinfo, sizeof(*bootinfo));
    bootinfo->magic = BOOTINFO_MAGIC;
    bootinfo->version = 1;
    bootinfo->hhdm_offset = hhdm_request.response ? hhdm_request.response->offset : 0;
    bootinfo->genesis_cap_slot = genesis_cap_slot;

    if (memmap_request.response && hhdm_request.response) {
        bootinfo->memmap_addr = (uint64_t)memmap_request.response->entries - bootinfo->hhdm_offset;
        bootinfo->memmap_entries = memmap_request.response->entry_count;
    }

    if (executable_address_request.response) {
        uint64_t v_base = executable_address_request.response->virtual_base;
        uint64_t p_base = executable_address_request.response->physical_base;

        bootinfo->kernel_start = p_base + ((uint64_t)kernel_start - v_base);
        bootinfo->kernel_end = p_base + ((uint64_t)kernel_end - v_base);
    }

    return true;
}

int genesis_inject_initial_caps(process_t* proc, const genesis_initial_caps_t* caps) {
    cap_identity_t* ident;
    uint64_t free_frame;

    if (!proc || !proc->cspace || !caps) return -1;

    /* genesis_cap_slot == 0 is the sentinel meaning "do NOT inject a Genesis
     * capability".  Processes spawned via GENESIS_OP_SPAWN must not inherit
     * Genesis authority automatically; that authority must be delegated
     * explicitly via GENESIS_OP_DELEGATE.  Only genesis_bridge_spawn (the
     * primordial Paradigm boot) passes slot != 0. */
    if (caps->genesis_cap_slot != 0) {
        ident = cap_identity_create(0, CAP_TYPE_GENESIS, 0xFFFF, 0xDEADBEEF, CAP_MODE_ALL);
        if (!ident || cap_insert(proc->cspace, caps->genesis_cap_slot, ident) != 0) {
            if (ident) cap_identity_free(ident);
            return -1;
        }
    }

    ident = cap_identity_create(proc->pml4_phys,
                                CAP_TYPE_PAGETABLE,
                                CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT,
                                0,
                                CAP_MODE_ALL);
    if (!ident || cap_insert(proc->cspace, caps->pagetable_slot, ident) != 0) {
        if (ident) cap_identity_free(ident);
        return -1;
    }

    free_frame = pmm_alloc(COLOR_CASUAL, proc->pid);
    if (!free_frame) return -1;
    ident = cap_identity_create(free_frame,
                                CAP_TYPE_RAM,
                                CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_EXECUTE,
                                0,
                                CAP_MODE_ALL);
    if (!ident || cap_insert(proc->cspace, caps->ram_slot, ident) != 0) {
        if (ident) cap_identity_free(ident);
        return -1;
    }

    ident = cap_identity_create(0, CAP_TYPE_AUDITOR, CAP_RIGHT_READ, 0, CAP_MODE_ALL);
    if (!ident || cap_insert(proc->cspace, caps->audit_slot, ident) != 0) {
        if (ident) cap_identity_free(ident);
        return -1;
    }

    return 0;
}

int genesis_map_bootinfo(process_t* proc, uint64_t bootinfo_phys) {
    if (!proc || !bootinfo_phys) return -1;
    return vmm_map(proc, BOOTINFO_VIRT_ADDR, bootinfo_phys, PAGE_USER_DATA) ? 0 : -1;
}

int genesis_map_initial_stack(process_t* proc, uint64_t stack_top, uint64_t stack_pages) {
    uint64_t i;

    if (!proc || stack_top == 0 || stack_pages == 0) return -1;

    for (i = 0; i < stack_pages; i++) {
        uint64_t stack_phys = pmm_alloc(COLOR_CASUAL, proc->pid);
        uint64_t stack_virt;

        if (!stack_phys) return -1;
        stack_virt = stack_top - ((i + 1) * PAGE_SIZE);
        if (!vmm_map(proc, stack_virt, stack_phys, PAGE_USER_DATA)) {
            return -1;
        }
    }

    return 0;
}

int genesis_load_module_image(struct limine_file* module, process_t* proc, uint64_t* entry_point) {
    if (!module || !proc || !entry_point) return -1;
    return elf_load(module->address, proc, entry_point);
}

int genesis_spawn_process_from_module(struct limine_file* module,
                                      mode_id_t mode,
                                      uint64_t stack_top,
                                      uint64_t stack_pages,
                                      const genesis_initial_caps_t* caps,
                                      bool mint_sched_auth,
                                      bool queue_thread,
                                      genesis_spawn_result_t* out_result) {
    genesis_spawn_result_t result;
    uint64_t new_pml4;
    uint16_t pcid;
    cnode_t* cspace;
    process_t* proc;
    uint64_t bootinfo_phys;
    boot_info_t* bootinfo;
    thread_t* thread;

    if (!module || !caps || !out_result) return -1;
    fast_zero(&result, sizeof(result));

    new_pml4 = vmm_fork_pml4();
    pcid = pcid_alloc(mode);
    cspace = cnode_create();
    proc = process_create(new_pml4, pcid, cspace, mode);
    if (!proc) return -1;

    if (genesis_inject_initial_caps(proc, caps) != 0) return -1;

    bootinfo_phys = pmm_alloc(COLOR_SECURE, proc->pid);
    if (!bootinfo_phys) return -1;
    bootinfo = (boot_info_t*)pmm_phys_to_virt(bootinfo_phys);
    if (!genesis_bootinfo_init(bootinfo, caps->genesis_cap_slot)) return -1;
    if (genesis_map_bootinfo(proc, bootinfo_phys) != 0) return -1;
    if (genesis_load_module_image(module, proc, &result.entry_point) != 0) return -1;
    if (genesis_map_initial_stack(proc, stack_top, stack_pages) != 0) return -1;

    thread = thread_create(proc, paradigm_entry_stub);
    if (!thread) return -1;

    if (mint_sched_auth) {
        if (scheduler_mint_root_auth(proc,
                                     caps->sched_root_slot,
                                     mode,
                                     (uint64_t)DEFAULT_QUANTUM * 32ULL,
                                     (uint64_t)DEFAULT_QUANTUM,
                                     (uint64_t)SCHED_DEFAULT_MAX_ACCUMULATED * 4ULL) != 0) {
            return -1;
        }

        if (scheduler_derive_thread_auth(proc,
                                         thread,
                                         caps->sched_root_slot,
                                         caps->sched_thread_slot,
                                         DEFAULT_QUANTUM,
                                         1,
                                         SCHED_DEFAULT_MAX_ACCUMULATED) != 0) {
            return -1;
        }
    }

    if (queue_thread) {
        scheduler_add(thread);
    }

    result.process = proc;
    result.thread = thread;
    result.bootinfo_phys = bootinfo_phys;
    result.caps = *caps;
    *out_result = result;
    return 0;
}

void genesis_bridge_spawn(void) {
    kprintf("[GENESIS] Constructing the Bridge...\n");

    /* 0. Find the Module */
    if (!module_request.response || module_request.response->module_count == 0) {
        kprintf("[DAY15-FAIL] missing genesis module\n");
        kpanic("GENESIS: No modules found! Paradigm Lost.");
    }
    
    struct limine_file* module = module_request.response->modules[0];
    kprintf("[GENESIS] Module Found. Address: 0x%lx, Size: %ld\n", (uint64_t)module->address, module->size);
    kprintf("[TEST] Day 15 Genesis Module Contract: SUCCESS.\n");

    {
        genesis_initial_caps_t caps = {
            .genesis_cap_slot = 1,
            .pagetable_slot = 2,
            .ram_slot = 3,
            .audit_slot = 4,
            .sched_root_slot = 5,
            .sched_thread_slot = 6
        };
        genesis_spawn_result_t result;

        if (genesis_spawn_process_from_module(module,
                                              MODE_CASUAL,
                                              PARADIGM_STACK_TOP,
                                              PARADIGM_STACK_PAGES,
                                              &caps,
                                              true,
                                              true,
                                              &result) != 0) {
            kprintf("[DAY15-FAIL] paradigm process creation failed\n");
            kpanic("GENESIS: Failed to create Paradigm process!");
        }

        paradigm_entry_point = result.entry_point;
        paradigm_pid = result.process->pid;
    }

    kprintf("[TEST] Day 15 Genesis Capability Injection: SUCCESS.\n");
    kprintf("[TEST] Day 15 Bootinfo Bridge: SUCCESS.\n");
    kprintf("[GENESIS] ELF Loaded. Entry Point: 0x%lx\n", paradigm_entry_point);

    kprintf("[GENESIS] Paradigm soul forged and queued. C-Slot 1: GENESIS_CAP.\n");
}

uint32_t genesis_get_paradigm_pid(void) {
    return paradigm_pid;
}

uint64_t genesis_syscall_dispatch(process_t* owner, uint32_t op, uint32_t cap_slot,
                                   uint64_t req_ptr, bool is_kernel) {
    if (!owner || !owner->cspace) return (uint64_t)-1;
    if (cap_genesis_is_exhausted())  return (uint64_t)-1;

    /* Validate the Genesis Capability in the caller's cspace. */
    cap_identity_t* ident = cap_lookup(owner->cspace, cap_slot);
    if (!ident || ident->type != CAP_TYPE_GENESIS) return (uint64_t)-1;

    switch (op) {

        /* ------------------------------------------------------------------ */
        case GENESIS_OP_SPAWN: {
            genesis_spawn_req_t req;
            if (is_kernel) {
                memcpy(&req, (void*)req_ptr, sizeof(req));
            } else {
                if (!genesis_copy_from_user(&req, (const void*)req_ptr, sizeof(req)))
                    return (uint64_t)-1;
            }

            if (!module_request.response ||
                req.module_index >= module_request.response->module_count)
                return (uint64_t)-1;

            struct limine_file* module =
                module_request.response->modules[req.module_index];

            genesis_initial_caps_t caps = {
                /* genesis_cap_slot = 0: spawned daemons do NOT inherit Genesis
                 * authority — it must be delegated explicitly via
                 * GENESIS_OP_DELEGATE.  genesis_inject_initial_caps skips the
                 * Genesis cap injection when this field is 0. */
                .genesis_cap_slot  = 0,
                .pagetable_slot    = req.out_pagetable_slot,
                /* TODO: expose ram_slot and audit_slot in genesis_spawn_req_t
                 * when bootinfo v2 lands.  Area 3 is the right time. */
                .ram_slot          = 3,
                .audit_slot        = 4,
                .sched_root_slot   = req.out_sched_root_slot,
                .sched_thread_slot = req.out_sched_thread_slot,
            };
            genesis_spawn_result_t result;

            /* Use owner->mode, not MODE_CASUAL: the spawned process inherits
             * the caller's security envelope. */
            if (genesis_spawn_process_from_module(module, owner->mode,
                                                  PARADIGM_STACK_TOP,
                                                  PARADIGM_STACK_PAGES,
                                                  &caps, true,
                                                  false, /* don't queue yet */
                                                  &result) != 0)
                return (uint64_t)-1;

            return (uint64_t)result.process->pid;
        }

        /* ------------------------------------------------------------------ */
        case GENESIS_OP_DELEGATE: {
            genesis_delegate_req_t req;
            if (is_kernel) {
                memcpy(&req, (void*)req_ptr, sizeof(req));
            } else {
                if (!genesis_copy_from_user(&req, (const void*)req_ptr, sizeof(req)))
                    return (uint64_t)-1;
            }

            /* Whitelist: only four types may bypass the normal cap_mint path. */
            if (!genesis_is_delegatable_type((cap_type_t)req.cap_type))
                return (uint64_t)-1;

            process_t* target = process_find_by_pid(req.target_pid);
            if (!target || !target->cspace)
                return (uint64_t)-1;

            cap_identity_t* new_ident = cap_identity_create(
                req.object_ptr,
                (uint16_t)req.cap_type,
                req.cap_rights,
                req.badge,
                req.allowed_modes
            );
            if (!new_ident) return (uint64_t)-1;

            if (cap_insert(target->cspace, req.target_slot, new_ident) != 0) {
                cap_identity_free(new_ident);
                return (uint64_t)-1;
            }

            return 0;
        }

        /* ------------------------------------------------------------------ */
        case GENESIS_OP_DESTROY: {
            if (cap_genesis_exhaust()) {
                kprintf("[GENESIS] Authority exhausted. The Bridge is closed.\n");
                return 0;
            }
            return (uint64_t)-1;
        }

        default:
            return (uint64_t)-1;
    }
}
