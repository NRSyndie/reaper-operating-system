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

extern void user_mode_jump(uint64_t rip, uint64_t rsp);
extern struct limine_memmap_request memmap_request;
extern struct limine_hhdm_request hhdm_request;
extern struct limine_executable_address_request executable_address_request;
extern struct limine_module_request module_request;
extern char kernel_start[];
extern char kernel_end[];

/* The Genesis Bridge: Virtual Address for Boot Info */
#define BOOTINFO_VIRT_ADDR 0x1000

/* 
 * We need a way to pass the Entry Point to the trampoline.
 * For now, we'll store it in a static variable since Genesis is a singleton event.
 */
static uint64_t paradigm_entry_point = 0;

static void paradigm_entry_stub(void) {
    user_mode_jump(paradigm_entry_point, 0x800000);
}

void genesis_bridge_spawn(void) {
    kprintf("[GENESIS] Constructing the Bridge...\n");

    /* 0. Find the Module */
    if (!module_request.response || module_request.response->module_count == 0) {
        kpanic("GENESIS: No modules found! Paradigm Lost.");
    }
    
    struct limine_file* module = module_request.response->modules[0];
    kprintf("[GENESIS] Module Found. Address: 0x%lx, Size: %ld\n", (uint64_t)module->address, module->size);

    /* 1. Create Paradigm's World */
    uint64_t new_pml4 = vmm_fork_pml4();
    uint16_t pcid = pcid_alloc(MODE_CASUAL);
    cnode_t* cspace = cnode_create();
    process_t* paradigm = process_create(new_pml4, pcid, cspace, MODE_CASUAL);
    
    if (!paradigm) {
        kpanic("GENESIS: Failed to create Paradigm process!");
    }

    /* 2. Inject the Genesis Capability */
    cap_identity_t* genesis_ident = cap_identity_create(0, CAP_TYPE_GENESIS, 0xFFFF, 0xDEADBEEF, CAP_MODE_ALL);
    cap_insert(cspace, 1, genesis_ident); /* Slot 1 is the Genesis Rune */

    /* 2b. Inject PML4 and RAM Capabilities */
    cap_identity_t* pml4_ident = cap_identity_create(paradigm->pml4_phys, CAP_TYPE_PAGETABLE, CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT, 0, CAP_MODE_ALL);
    cap_insert(cspace, 2, pml4_ident);

    uint64_t free_frame = pmm_alloc(COLOR_CASUAL, paradigm->pid);
    cap_identity_t* ram_ident = cap_identity_create(free_frame, CAP_TYPE_RAM, CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_EXECUTE, 0, CAP_MODE_ALL);
    cap_insert(cspace, 3, ram_ident);

    /* 2d. Inject Auditor Capability (Fatal Forensics) */
    cap_identity_t* audit_ident = cap_identity_create(0, CAP_TYPE_AUDITOR, CAP_RIGHT_READ, 0, CAP_MODE_ALL);
    cap_insert(cspace, 4, audit_ident);

    /* 3. Prepare Boot Information */
    uint64_t bootinfo_phys = pmm_alloc(COLOR_SECURE, paradigm->pid);
    boot_info_t* bootinfo = (boot_info_t*)pmm_phys_to_virt(bootinfo_phys);
    fast_zero(bootinfo, sizeof(boot_info_t));

    bootinfo->magic = BOOTINFO_MAGIC;
    bootinfo->version = 1;
    bootinfo->hhdm_offset = hhdm_request.response->offset;
    bootinfo->genesis_cap_slot = 1;
    
    if (memmap_request.response) {
        bootinfo->memmap_addr = (uint64_t)memmap_request.response->entries - bootinfo->hhdm_offset;
        bootinfo->memmap_entries = memmap_request.response->entry_count;
    }

    if (executable_address_request.response) {
        uint64_t v_base = executable_address_request.response->virtual_base;
        uint64_t p_base = executable_address_request.response->physical_base;
        
        bootinfo->kernel_start = p_base + ((uint64_t)kernel_start - v_base);
        bootinfo->kernel_end = p_base + ((uint64_t)kernel_end - v_base);
    }

    /* 4. Map Boot Info into Paradigm's Reality */
    vmm_map(paradigm, BOOTINFO_VIRT_ADDR, bootinfo_phys, PAGE_USER_DATA);

    /* 5. Load the ELF */
    if (elf_load(module->address, paradigm, &paradigm_entry_point) != 0) {
        kpanic("GENESIS: ELF Load Failed!");
    }
    
    kprintf("[GENESIS] ELF Loaded. Entry Point: 0x%lx\n", paradigm_entry_point);

    /* 6. Setup Stack */
    // We allocate a single page for the stack at 0x800000 - 4096 (growing down)
    // Wait, 0x800000 is top of stack?
    // Let's allocate the page at 0x7FF000
    uint64_t stack_phys = pmm_alloc(COLOR_CASUAL, paradigm->pid);
    vmm_map(paradigm, 0x7FF000, stack_phys, PAGE_USER_DATA);
    
    /* 7. Launch Paradigm */
    thread_t* paradigm_thread = thread_create(paradigm, paradigm_entry_stub);
    if (!paradigm_thread) {
        kpanic("GENESIS: Failed to create Paradigm thread.");
    }

    if (scheduler_mint_root_auth(paradigm,
                                 5,
                                 MODE_CASUAL,
                                 (uint64_t)DEFAULT_QUANTUM * 32ULL,
                                 (uint64_t)DEFAULT_QUANTUM,
                                 (uint64_t)SCHED_DEFAULT_MAX_ACCUMULATED * 4ULL) != 0) {
        kpanic("GENESIS: Failed to mint root scheduling authority.");
    }

    if (scheduler_derive_thread_auth(paradigm,
                                     paradigm_thread,
                                     5,
                                     6,
                                     DEFAULT_QUANTUM,
                                     1,
                                     SCHED_DEFAULT_MAX_ACCUMULATED) != 0) {
        kpanic("GENESIS: Failed to derive thread scheduling authority.");
    }
    scheduler_add(paradigm_thread);

    kprintf("[GENESIS] Paradigm soul forged and queued. C-Slot 1: GENESIS_CAP.\n");
}
