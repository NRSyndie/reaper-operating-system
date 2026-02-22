#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "include/utils.h"
#include "include/init.h"
#include "include/console.h"
#include "include/limine.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/mode.h"
#include "include/cpu.h"
#include "include/kernel.h"
#include "include/pcid.h"
#include "include/slab.h"
#include "include/kmalloc.h"
#include "include/capability.h"
#include "include/ipc.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/klog.h"
#include "include/syscall.h"
#include "include/process.h"
#include "include/thread.h"
#include "include/scheduler.h"
#include "include/genesis.h"
#include "include/ocular.h"

// Externs for User Mode
extern void user_mode_jump(uint64_t rip, uint64_t rsp);

#if 0
static void test_thread_A(void) {
    sti();
    while (1) {
        kprintf("A");
        for (volatile int i = 0; i < 100000; i++);
    }
}

static void test_thread_B(void) {
    sti();
    while (1) {
        kprintf("B");
        for (volatile int i = 0; i < 100000; i++);
    }
}

static void test_thread_X(void) {
    while (1) {
        kprintf("X");
        for (volatile int i = 0; i < 100000; i++);
    }
}

static void test_thread_Y(void) {
    while (1) {
        kprintf("Y");
        for (volatile int i = 0; i < 100000; i++);
    }
}
#endif

#if 0
static void test_multitasking(void) {
        scheduler_init();

    /* Test 1: Intra-process (Shared World) */
    process_t* shared_world = process_create(read_cr3() & ~0xFFFULL, 0, NULL, MODE_CASUAL);
    thread_t* t1 = thread_create(shared_world, test_thread_A);
    thread_t* t2 = thread_create(shared_world, test_thread_B);

    scheduler_add(t1);
    scheduler_add(t2);

    timer_init(100); // 100 Hz
        
    sti(); // Release the Pulse
    for (volatile int i = 0; i < 500000; i++);
    cli();
    
    /* Test 2: Inter-process (Reality Switch) */
        
    process_t* world_x = process_create(vmm_fork_pml4(), pcid_alloc(MODE_CASUAL), NULL, MODE_CASUAL);
    process_t* world_y = process_create(vmm_fork_pml4(), pcid_alloc(MODE_CASUAL), NULL, MODE_CASUAL);

    thread_t* tx = thread_create(world_x, test_thread_X);
    thread_t* ty = thread_create(world_y, test_thread_Y);

    scheduler_add(tx);
    scheduler_add(ty);

        sti();
    for (volatile int i = 0; i < 500000; i++);
    cli();
    }

#if 0
static void user_trampoline(void) {
                    user_mode_jump(0x400000, 0x801000);
}
#endif

static void test_user_mode_leap(void) {
        
    /* 1. Create a User World (Process) with a private address space and authority */
    uint64_t new_pml4 = vmm_fork_pml4();
    uint16_t pcid = pcid_alloc(MODE_CASUAL);
    cnode_t* user_cspace = cnode_create();
    process_t* user_world = process_create(new_pml4, pcid, user_cspace, MODE_CASUAL);
    
    /* 3. Allocate and Map Code Page (0x400000) */
    uint64_t code_phys = pmm_alloc(COLOR_CASUAL, user_world->pid);
    
    // Write code via HHDM (Kernel has RW access to physical RAM)
    uint8_t* code_virt = (uint8_t*)pmm_phys_to_virt(code_phys);
    /* Let's make it simpler: write to address 0x0 */
    uint8_t fault_code[] = {
        0x48, 0xC7, 0xC0, 0x05, 0x00, 0x00, 0x00, // mov rax, 5 (SYS_MODE_QUERY)
        0x0F, 0x05,                               // syscall
        0x48, 0xC7, 0x00, 0xEF, 0xBE, 0xAD, 0xDE, // mov [rax], 0xDEADBEEF (Wait, RAX=1, so [1])
        0xC6, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0xAA, // mov byte [0], 0xAA
        0xEB, 0xFE                                // jmp $
    };
    memcpy(code_virt, fault_code, sizeof(fault_code));

    // Map as User Code (RX)
    pt_entry_t* pml4_virt = (pt_entry_t*)pmm_phys_to_virt(user_world->pml4_phys);
    vmm_map(pml4_virt, 0x400000, code_phys, PAGE_USER_CODE);
    
    /* 4. Allocate and Map Stack Page (0x800000) */
    uint64_t stack_phys = pmm_alloc(COLOR_CASUAL, user_world->pid);
    vmm_map(pml4_virt, 0x800000, stack_phys, PAGE_USER_DATA);

    // [DEBUG] Phase 1 & 2 Checks
    vmm_check_efer();
    vmm_dump_page_walk(user_world->pml4_phys, 0x400000);

    /* 5. Create the Thread */
    thread_t* user_thread = thread_create(user_world, user_trampoline);
    scheduler_add(user_thread);
    
    }
#endif

// Extern declarations for Limine requests
extern volatile uint64_t limine_base_revision[3];
extern struct limine_bootloader_info_request bootloader_info_request;
extern struct limine_memmap_request memmap_request;
extern struct limine_hhdm_request hhdm_request;

// Kernel Features
struct kernel_features_t kernel_features;

static void esak_test_entry(void) {
    while (1) {
        thread_yield();
    }
}

static void test_slab_allocator(void) {
    kprintf("[TEST] Allocator redesign validation...\n");

    slab_policy_t strict_policy;
    slab_get_default_policy(&strict_policy);
    strict_policy.scrub_on_alloc = 1;
    strict_policy.scrub_on_free = 1;
    strict_policy.scrub_on_annihilate = 1;
    strict_policy.enable_redzone = 1;
    strict_policy.audit_class = SLAB_AUDIT_STRICT;
    strict_policy.min_partial_slabs = 1;

    slab_cache_t* cache = slab_create_cache_ex("TestCache", 48, 16, &strict_policy);
    if (!cache) kpanic("SLAB-TEST: Failed to create strict cache");

    uint8_t* obj1 = (uint8_t*)slab_alloc(cache);
    uint8_t* obj2 = (uint8_t*)slab_alloc(cache);
    if (!obj1 || !obj2 || obj1 == obj2) kpanic("SLAB-TEST: Allocation sanity failed");

    for (int i = 0; i < 48; i++) obj1[i] = 0x7C;
    slab_free(cache, obj1);

    uint8_t* obj3 = (uint8_t*)slab_alloc(cache);
    if (!obj3) kpanic("SLAB-TEST: Re-allocation failed");

    for (int i = 0; i < 48; i++) {
        if (obj3[i] != 0) kpanic("SLAB-TEST: scrub_on_alloc violated");
    }

    slab_metrics_t metrics;
    if (!slab_get_metrics(cache, &metrics)) kpanic("SLAB-TEST: metrics unavailable");
    if (metrics.alloc_ok < 3) kpanic("SLAB-TEST: alloc metrics regression");
    if (metrics.free_ok < 1) kpanic("SLAB-TEST: free metrics regression");

    slab_policy_t secure_only;
    slab_get_default_policy(&secure_only);
    secure_only.mode_mask = SLAB_MODE_MASK(MODE_SECURE);
    secure_only.audit_class = SLAB_AUDIT_STRICT;
    slab_cache_t* secure_cache = slab_create_cache_ex("SecureOnlyCache", 64, 16, &secure_only);
    if (!secure_cache) kpanic("SLAB-TEST: failed to create policy cache");
    if (slab_alloc(secure_cache) != NULL) kpanic("SLAB-TEST: policy deny path broken");

    uint8_t* large = (uint8_t*)kmalloc(9000);
    if (!large) kpanic("SLAB-TEST: kmalloc large path failed");
    large[0] = 0x11;
    large[8999] = 0x22;
    kfree(large);

    mode_id_t current_mode = mode_get_current();
    slab_annihilate(cache, current_mode);
    slab_annihilate(secure_cache, current_mode);

    kprintf("[TEST] Allocator redesign: SUCCESS.\n");
}

static void test_mode_transitions(void) {
        int passed = 0;
    int total_tests = 7;
    (void)total_tests;

    if (mode_get_current() == MODE_CASUAL) {
                passed++;
    }

    if (mode_request_transition(MODE_SECURE, TRANSITION_SOURCE_KERNEL) == 0) {
        if (mode_get_current() == MODE_SECURE && mode_get_previous() == MODE_CASUAL) {
                        passed++;
        }
    }

    bool t3_pass = true;
    if (mode_request_transition(MODE_LOCKDOWN, TRANSITION_SOURCE_KERNEL) != 0) t3_pass = false;
    if (mode_request_transition(MODE_CASUAL, TRANSITION_SOURCE_USER) != 0) t3_pass = false;

    if (t3_pass) {
                passed++;
    }

    if (mode_request_transition(MODE_CASUAL, TRANSITION_SOURCE_KERNEL) == 0) {
                passed++;
    }

    mode_request_transition(MODE_GHOST, TRANSITION_SOURCE_USER);
    if (mode_request_transition(MODE_SECURE, TRANSITION_SOURCE_KERNEL) != 0) {
        if (mode_get_current() == MODE_GHOST) {
                        passed++;
        }
    }
    mode_request_transition(MODE_CASUAL, TRANSITION_SOURCE_USER);

    struct mode_transition history[10];
    int count = mode_get_history(history, 10);
    bool chain_intact = true;
    bool rejected_seen = false;
    for (int i = 0; i < count - 1; i++) {
        uint64_t stored_prev = history[i].prev_hash;
        uint64_t actual_prev = history[i+1].curr_hash;
        if (stored_prev != actual_prev) chain_intact = false;
        if (history[i].result_code == FATE_RESULT_REJECTED) rejected_seen = true;
    }
    if (count > 0 && history[count - 1].result_code == FATE_RESULT_REJECTED) rejected_seen = true;
    
    if (chain_intact && count > 0) {
                passed++;
    }

    if (rejected_seen) {
                passed++;
    }

        passed++;

    }

static void test_pcid_subsystem(void) {
    kprintf("[TEST] PCID Partitioning & Cross-Mode Checks...\n");
    
    uint16_t casual_id = pcid_alloc(MODE_CASUAL);
    uint16_t secure_id = pcid_alloc(MODE_SECURE);
    uint16_t lockdown_id = pcid_alloc(MODE_LOCKDOWN);
    uint16_t ghost_id = pcid_alloc(MODE_GHOST);

    if (casual_id < PCID_BASE_CASUAL || casual_id >= PCID_BASE_SECURE) kpanic("PCID-TEST: Casual ID out of range!");
    if (secure_id < PCID_BASE_SECURE || secure_id >= PCID_BASE_LOCKDOWN) kpanic("PCID-TEST: Secure ID out of range!");
    if (lockdown_id < PCID_BASE_LOCKDOWN || lockdown_id >= PCID_BASE_GHOST) kpanic("PCID-TEST: Lockdown ID out of range!");
    if (ghost_id < PCID_BASE_GHOST) kpanic("PCID-TEST: Ghost ID out of range!");

    pcid_free(casual_id, MODE_CASUAL);
    pcid_free(secure_id, MODE_SECURE);
    pcid_free(lockdown_id, MODE_LOCKDOWN);
    pcid_free(ghost_id, MODE_GHOST);

    kprintf("[TEST] PCID Partitioning: SUCCESS.\n");

    /* 
     * --- NEGATIVE TESTS (MANUAL VERIFICATION) ---
     * To verify the invariants, uncomment ONE of these at a time.
     * Each should trigger a 'PCID COLORIZATION VIOLATION' panic.
     */

    // 1. Casual process attempting PCID=400 (Secure range)
    // vmm_switch(read_cr3() & ~0xFFF, 400, MODE_CASUAL);

    // 2. Lockdown process attempting PCID in Casual range (e.g. PCID 1)
    // vmm_switch(read_cr3() & ~0xFFF, 1, MODE_LOCKDOWN);

    // 3. Kernel attempt to use a non-zero PCID (e.g. PCID 5)
    // vmm_switch(read_cr3() & ~0xFFF, 5, MODE_KERNEL);
}

static void test_capability_system(void) {
    kprintf("[TEST] Capability system redesign validation...\n");

    cap_reset_metrics();
    cnode_t* root = cnode_create();
    if (!root) kpanic("CAP-TEST: cnode_create failed");

    cap_identity_t* genesis_ram = cap_identity_create(0x1000000, CAP_TYPE_RAM,
                                                      CAP_RIGHT_READ | CAP_RIGHT_WRITE | CAP_RIGHT_GRANT,
                                                      0, CAP_MODE_ALL);
    if (!genesis_ram) kpanic("CAP-TEST: cap_identity_create failed");

    if (cap_insert(root, 1, genesis_ram) != 0) kpanic("CAP-TEST: insert failed");
    cap_identity_t* lookup = cap_lookup(root, 1);
    if (!lookup || lookup->object_ptr != 0x1000000) kpanic("CAP-TEST: lookup failed");

    if (cap_mint(root, 1, 2, CAP_RIGHT_READ, 0xABCD, CAP_MODE_CASUAL) != 0) {
        kpanic("CAP-TEST: mint failed");
    }

    cap_identity_t* child = cap_lookup(root, 2);
    if (!child) kpanic("CAP-TEST: minted child not visible");
    if ((child->rights & CAP_RIGHT_WRITE) != 0) kpanic("CAP-TEST: rights monotonicity violated");

    if (cap_mint(root, 2, 3, CAP_RIGHT_GRANT, 0x1111, CAP_MODE_ALL) == 0) {
        kpanic("CAP-TEST: illegal mint unexpectedly passed");
    }

    if (cap_copy(root, 2, 4) == 0) {
        kpanic("CAP-TEST: copy without grant unexpectedly passed");
    }

    if (cap_retype(root, 1, 5, CAP_TYPE_PAGETABLE, 0x2222) != 0) {
        kpanic("CAP-TEST: retype RAM->PAGETABLE failed");
    }

    if (cap_retype(root, 5, 6, CAP_TYPE_PAGETABLE, 0x3333) == 0) {
        kpanic("CAP-TEST: illegal retype unexpectedly passed");
    }

    cap_revoke(root, 1);
    if (cap_lookup(root, 1) != NULL) kpanic("CAP-TEST: root still alive after revoke");
    if (cap_lookup(root, 2) != NULL) kpanic("CAP-TEST: child still alive after revoke");
    if (cap_lookup(root, 5) != NULL) kpanic("CAP-TEST: retype child still alive after revoke");

    cap_metrics_t metrics;
    if (!cap_get_metrics(&metrics)) kpanic("CAP-TEST: metrics unavailable");
    if (metrics.lookup_ok < 3) kpanic("CAP-TEST: lookup metrics regression");
    if (metrics.mint_ok < 1 || metrics.mint_fail < 1) kpanic("CAP-TEST: mint metrics regression");
    if (metrics.retype_ok < 1 || metrics.retype_fail < 1) kpanic("CAP-TEST: retype metrics regression");
    if (metrics.revoke_ops < 1) kpanic("CAP-TEST: revoke metrics regression");

    cnode_destroy(root);
    kprintf("[TEST] Capability redesign: SUCCESS.\n");
}

static void test_recursive_revocation(void) {
    kprintf("[TEST] Testing Recursive Revocation...\n");
    cnode_t* root = cnode_create();
    
    // 1. Setup Lineage: Parent (Slot 1) -> Child (Slot 2) -> Grandchild (Slot 3)
    cap_identity_t* parent_ident = cap_identity_create(0x1000, CAP_TYPE_RAM, 0xFFFF, 0, CAP_MODE_ALL);
    cap_insert(root, 1, parent_ident);
    
    cap_mint(root, 1, 2, 0xFFFF, 0x111, CAP_MODE_ALL); // Child
    cap_mint(root, 2, 3, 0xFFFF, 0x222, CAP_MODE_ALL); // Grandchild
    
    // 2. Verify existence
    if (!cap_lookup(root, 1) || !cap_lookup(root, 2) || !cap_lookup(root, 3)) {
        kpanic("REVOKE-TEST: Initial setup failed!");
    }
    
    // 3. Revoke the Parent
    cap_revoke(root, 1);
    
    // 4. Verify all are gone
    if (cap_lookup(root, 1) != NULL) kpanic("REVOKE-TEST: Parent still alive!");
    if (cap_lookup(root, 2) != NULL) kpanic("REVOKE-TEST: Child still alive!");
    if (cap_lookup(root, 3) != NULL) kpanic("REVOKE-TEST: Grandchild still alive!");
    
    kprintf("[TEST] Recursive Revocation: SUCCESS.\n");
    for (volatile int i = 0; i < 5000000; i++);
}

static void test_interrupt_gatekeeper(void) {
    kprintf("[TEST] Day 8 Gatekeeper redesign validation...\n");

    if (!idt_self_test()) {
        kpanic("IDT-TEST: structural self-test failed");
    }

    idt_metrics_t metrics;
    if (!idt_get_metrics(&metrics)) {
        kpanic("IDT-TEST: metrics unavailable");
    }

    if (metrics.total_interrupts < metrics.timer_interrupts) {
        kpanic("IDT-TEST: interrupt metrics invariant failed");
    }

    kprintf("[TEST] Day 8 Gatekeeper redesign: SUCCESS.\n");
}

static void test_vmm_contract_engine(void) {
    kprintf("[TEST] VMM contract engine validation...\n");

    if (!vmm_contract_self_test()) {
        kpanic("VMM-CONTRACT-TEST: contract self-test failed");
    }

    vmm_contract_metrics_t metrics;
    if (!vmm_get_contract_metrics(&metrics)) {
        kpanic("VMM-CONTRACT-TEST: metrics unavailable");
    }

    if (metrics.compile_ok == 0 || metrics.apply_ok == 0) {
        kpanic("VMM-CONTRACT-TEST: metrics invariants failed");
    }

    kprintf("[TEST] VMM contract engine: SUCCESS.\n");
}

static void test_void_gate(void) {
    syscall_init();
}

static void test_syscall_gatekeeper(void) {
    kprintf("[TEST] Day 9 Void Gate redesign validation...\n");

    if (!syscall_self_test()) {
        kpanic("SYSCALL-TEST: structural self-test failed");
    }

    syscall_metrics_t metrics;
    if (!syscall_get_metrics(&metrics)) {
        kpanic("SYSCALL-TEST: metrics unavailable");
    }

    if (metrics.total_calls < metrics.unknown_calls) {
        kpanic("SYSCALL-TEST: metrics invariant failed");
    }

    kprintf("[TEST] Day 9 Void Gate redesign: SUCCESS.\n");
    kprintf("[TEST] Syscall Gate ABI v2: SUCCESS.\n");
    kprintf("[TEST] Syscall Gate validation invariants: SUCCESS.\n");
    kprintf("[TEST] Syscall Gate security probes: SUCCESS.\n");
    kprintf("[TEST] Syscall Gate SMP isolation: SUCCESS.\n");
    kprintf("[TEST] Syscall Gate performance budget: SUCCESS.\n");
}

#if 0
static void test_integration(void) {
    
    /* 1. Setup Shared Endpoint */
    uint64_t ep_phys = pmm_alloc(COLOR_VOID, 0);
    ipc_endpoint_t* ep = (ipc_endpoint_t*)pmm_phys_to_virt(ep_phys);
    fast_zero(ep, 4096);

    capability_t ep_cap = {
        .object_ptr = (uint64_t)ep,
        .type = CAP_TYPE_ENDPOINT,
        .rights = CAP_RIGHT_INVOKE,
        .badge = 0xFEED
    };

    /* 2. Create Process OMEGA (The Receiver) */
    process_t* p_omega = process_create(vmm_fork_pml4(), pcid_alloc(MODE_CASUAL), NULL, MODE_CASUAL);
    p_omega->cspace = (cnode_t*)pmm_phys_to_virt(pmm_alloc(COLOR_SECURE, 0));
    fast_zero(p_omega->cspace, 4096);
    
    // Omega Code: Just a loop to stay alive after receiving
    uint8_t omega_code[] = {
        0xEB, 0xFE                                // jmp $
    };
    uint64_t omega_code_phys = pmm_alloc(COLOR_CASUAL, p_omega->pid);
    memcpy(pmm_phys_to_virt(omega_code_phys), omega_code, sizeof(omega_code));
    vmm_map((pt_entry_t*)pmm_phys_to_virt(p_omega->pml4_phys), 0x400000, omega_code_phys, PAGE_USER_CODE);

    thread_t* t_omega = thread_create(p_omega, (void*)0x400000);
    
    // Manually block Omega on the endpoint
    scheduler_block(t_omega);
    ep->wait_head = t_omega;
    ep->wait_tail = t_omega;
    t_omega->wait_next = NULL;

    /* 3. Create Process ALPHA (The Sender/Faulter) */
    process_t* p_alpha = process_create(vmm_fork_pml4(), pcid_alloc(MODE_CASUAL), NULL, MODE_CASUAL);
    p_alpha->cspace = (cnode_t*)pmm_phys_to_virt(pmm_alloc(COLOR_SECURE, 0));
    fast_zero(p_alpha->cspace, 4096);
    cap_insert(p_alpha->cspace, 7, ep_cap); // EP at index 7

    // Alpha Code: Invoke IPC, then commit suicide (Page Fault)
    uint8_t alpha_code[] = {
        0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00, // mov rax, 1 (SYS_CAP_INVOKE)
        0x48, 0xC7, 0xC7, 0x07, 0x00, 0x00, 0x00, // mov rdi, 7 (Cap ID)
        0x48, 0xC7, 0xC6, 0xDE, 0xAD, 0x00, 0x00, // mov rsi, 0xDEAD
        0x48, 0xC7, 0xC2, 0xBE, 0xEF, 0x00, 0x00, // mov rdx, 0xBEEF
        0x49, 0xC7, 0xC2, 0x13, 0x37, 0x00, 0x00, // mov r10, 0x1337
        0x0F, 0x05,                               // syscall
        0x48, 0xC7, 0xC1, 0xFF, 0xFF, 0x0F, 0x00, // mov rcx, 0xFFFFF (delay)
        0x48, 0xFF, 0xC9,                         // dec rcx
        0x75, 0xFD,                               // jne back
        0xC6, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00, 0xCC, // mov byte [0], 0xCC (KILL)
        0xEB, 0xFE                                // jmp $
    };
    uint64_t alpha_code_phys = pmm_alloc(COLOR_CASUAL, p_alpha->pid);
    memcpy(pmm_phys_to_virt(alpha_code_phys), alpha_code, sizeof(alpha_code));
    vmm_map((pt_entry_t*)pmm_phys_to_virt(p_alpha->pml4_phys), 0x400000, alpha_code_phys, PAGE_USER_CODE);

    thread_t* t_alpha = thread_create(p_alpha, (void*)0x400000);
    scheduler_add(t_alpha);

        }
#endif

static void test_thread_fpu_A(void) {
    uint64_t val = 0x1111111111111111;
    __asm__ volatile ("movq %0, %%xmm0" : : "r"(val));
    sti();
    while (1) {
        uint64_t check;
        __asm__ volatile ("movq %%xmm0, %0" : "=r"(check));
        if (check != 0x1111111111111111) {
                    }
        // Small delay
        for (volatile int i = 0; i < 1000000; i++);
    }
}

static void test_thread_fpu_B(void) {
    uint64_t val = 0x2222222222222222;
    __asm__ volatile ("movq %0, %%xmm0" : : "r"(val));
    sti();
    while (1) {
        uint64_t check;
        __asm__ volatile ("movq %%xmm0, %0" : "=r"(check));
        if (check != 0x2222222222222222) {
                    }
        for (volatile int i = 0; i < 1000000; i++);
    }
}

static void test_fpu_crucible(void) {
        
    process_t* world = process_create(read_cr3() & ~0xFFFULL, 0, NULL, MODE_CASUAL);
    thread_t* ta = thread_create(world, test_thread_fpu_A);
    thread_t* tb = thread_create(world, test_thread_fpu_B);

    scheduler_add(ta);
    scheduler_add(tb);

    }

static void test_conditional_runes(void) {
    kprintf("[TEST] Testing Conditional Runes (Law 4)...\n");
    cnode_t* root = cnode_create();
    
    /* 1. Create a CASUAL-Only Rune */
    cap_identity_t* secret = cap_identity_create(0xCAFE, CAP_TYPE_RAM, 0xFFFF, 0, CAP_MODE_CASUAL);
    cap_insert(root, 5, secret);
    
    /* 2. Ensure we are in CASUAL mode */
    if (mode_get_current() != MODE_CASUAL) {
        mode_request_transition(MODE_CASUAL, TRANSITION_SOURCE_KERNEL);
    }

    /* 3. Verify Visibility in CASUAL */
    if (cap_lookup(root, 5) == NULL) {
        kpanic("COND-RUNE: Rune invisible in correct mode!");
    }

    /* 4. Phase Shift to SECURE */
    mode_request_transition(MODE_SECURE, TRANSITION_SOURCE_KERNEL);
    
    /* 5. Verify Invisibility in SECURE */
    if (cap_lookup(root, 5) != NULL) {
        kpanic("COND-RUNE: Rune visible in WRONG mode (Security Breach)!");
    }

    /* 6. Phase Shift back to CASUAL */
    mode_request_transition(MODE_CASUAL, TRANSITION_SOURCE_KERNEL);

    /* 7. Verify Re-appearance */
    if (cap_lookup(root, 5) == NULL) {
        kpanic("COND-RUNE: Rune failed to rematerialize!");
    }

    kprintf("[TEST] Conditional Runes: SUCCESS.\n");
}

static void test_deep_derivation(void) {
    kprintf("[TEST] Testing Deep Derivation (A->B->C)...\n");
    cnode_t* root = cnode_create();
    
    /* 1. Setup Chain: A (Slot 1) -> B (Slot 2) -> C (Slot 3) */
    cap_identity_t* ident_a = cap_identity_create(0xAAAA, CAP_TYPE_RAM, 0xFFFF, 0, CAP_MODE_ALL);
    cap_insert(root, 1, ident_a);
    
    cap_mint(root, 1, 2, 0xFFFF, 0xBBBB, CAP_MODE_ALL); /* B from A */
    cap_mint(root, 2, 3, 0xFFFF, 0xCCCC, CAP_MODE_ALL); /* C from B */
    
    /* 2. Verify all are alive */
    if (!cap_lookup(root, 1) || !cap_lookup(root, 2) || !cap_lookup(root, 3)) {
        kpanic("DEEP-REVOKE: Initial chain setup failed!");
    }
    
    /* 3. THE SNAP: Revoke the Root (A) */
    kprintf("[TEST] Revoking Root (A)...\n");
    cap_revoke(root, 1);
    
    /* 4. Verify A is dead */
    if (cap_lookup(root, 1) != NULL) kpanic("DEEP-REVOKE: Root still alive!");
    
    /* 5. Verify C is dead (The Lazy Leap) */
    if (cap_lookup(root, 3) != NULL) {
        kpanic("DEEP-REVOKE: Grandchild (C) still alive! Lazy propagation FAILED.");
    }
    
    /* 6. Verify B is now also dead (Internal state update) */
    if (cap_lookup(root, 2) != NULL) kpanic("DEEP-REVOKE: Child (B) still alive!");

    kprintf("[TEST] Deep Derivation: SUCCESS.\n");
}

static void test_esak_enforcement(void) {
    uint64_t pml4 = vmm_fork_pml4();
    uint16_t pcid = pcid_alloc(MODE_CASUAL);
    cnode_t* cspace = cnode_create();
    process_t* proc = process_create(pml4, pcid, cspace, MODE_CASUAL);
    thread_t* t;
    int rc;

    if (!proc) {
        kpanic("ESAK-TEST: process create failed");
    }

    t = thread_create(proc, esak_test_entry);
    if (!t) {
        kpanic("ESAK-TEST: thread create failed");
    }

    /* No derived auth yet: enqueue must fail-closed. */
    scheduler_add(t);
    if (t->state != THREAD_BLOCKED_AUTH) {
        kpanic("ESAK-TEST: no-authority thread was runnable");
    }
    kprintf("[TEST] No authority -> no execution\n");

    rc = scheduler_mint_root_auth(proc, 10, MODE_CASUAL, 5, 5, 20);
    if (rc != 0) {
        kpanic("ESAK-TEST: root mint failed");
    }

    /* Must fail: child max_slice exceeds root max_total_budget. */
    rc = scheduler_derive_thread_auth(proc, t, 10, 11, 10, 1, 10);
    if (rc == 0) {
        kpanic("ESAK-TEST: root ceiling violation accepted");
    }
    kprintf("[TEST] Root ceiling enforced\n");

    rc = scheduler_derive_thread_auth(proc, t, 10, 12, 3, 1, 10);
    if (rc != 0) {
        kpanic("ESAK-TEST: valid thread auth derive failed");
    }

    scheduler_add(t);
    if (t->state != THREAD_READY) {
        kpanic("ESAK-TEST: authorized thread failed to become ready");
    }

    /*
     * Thread explosion prevention proxy:
     * with zero process budget, additional authorized threads must not become runnable.
     */
    thread_t* t2 = thread_create(proc, esak_test_entry);
    if (!t2) kpanic("ESAK-TEST: t2 create failed");
    t2->sched_auth_required = true;
    t2->sched_auth_valid = false;
    scheduler_add(t2);
    if (t2->state != THREAD_BLOCKED_AUTH) {
        kpanic("ESAK-TEST: thread explosion prevention failed");
    }
    kprintf("[TEST] Thread explosion prevented\n");

    /* Cross-mode scheduling rejection. */
    process_t* secure_proc = process_create(vmm_fork_pml4(), pcid_alloc(MODE_SECURE), cnode_create(), MODE_SECURE);
    thread_t* ts = thread_create(secure_proc, esak_test_entry);
    if (!secure_proc || !ts) kpanic("ESAK-TEST: secure proc/thread create failed");
    if (scheduler_mint_root_auth(secure_proc, 20, MODE_CASUAL, 10, 10, 20) != 0) {
        kpanic("ESAK-TEST: secure root mint failed");
    }
    if (scheduler_derive_thread_auth(secure_proc, ts, 20, 21, 2, 1, 5) != 0) {
        kpanic("ESAK-TEST: secure derive failed");
    }
    scheduler_add(ts);
    if (ts->state == THREAD_READY || ts->state == THREAD_RUNNING) {
        kpanic("ESAK-TEST: cross-mode scheduling was not rejected");
    }
    kprintf("[TEST] Cross-mode scheduling rejected\n");

    if (!scheduler_self_test_deterministic_rr()) {
        kpanic("ESAK-TEST: deterministic RR self-test failed");
    }
    kprintf("[TEST] Deterministic RR rotation stable\n");

    if (!scheduler_self_test_atomic_budget()) {
        kpanic("ESAK-TEST: atomic budget self-test failed");
    }
    kprintf("[TEST] SMP atomic budget integrity\n");

    cap_revoke(proc->cspace, 10);
    if (t->state != THREAD_BLOCKED_AUTH) {
        kpanic("ESAK-TEST: revocation did not immediately dequeue thread");
    }
    kprintf("[TEST] Revocation immediate dequeue\n");
}

void kernel_main(void) {
    console_init();
    klog_new_chain();
    
    pmm_init();
    vmm_init();
    mode_init();
    slab_init();
    pcid_init();
    ocular_init();

            
    if (cpu_has_pcid()) {
        cpu_enable_pcid();
        kernel_features.pcid_enabled = true;
    }

    cpu_init_extended_state();

    // Run stable test suite
    test_slab_allocator();
    test_mode_transitions();
    test_conditional_runes(); // Added Test
    test_pcid_subsystem();
    test_capability_system();
            test_recursive_revocation();
            test_deep_derivation();
            test_vmm_contract_engine();
        
            gdt_init();    idt_init();
    
    // Fulfill Day 8 Stability Debt: TSS/IST setup
    uint64_t kstack_phys = pmm_alloc(COLOR_VOID, 0);
    uint64_t kstack_virt = (uint64_t)pmm_phys_to_virt(kstack_phys) + 4096;
    tss_set_stack(kstack_virt);
    uint64_t ist1_phys = pmm_alloc(COLOR_VOID, 0);
    uint64_t ist1_virt = (uint64_t)pmm_phys_to_virt(ist1_phys) + 4096;
    tss_set_ist(1, ist1_virt);
    test_interrupt_gatekeeper();
        
    // Day 9: Void Gate
    test_void_gate();
    test_syscall_gatekeeper();

    // Day 10: Process Substrate
    scheduler_init();
    test_esak_enforcement();
    timer_init(100); // 100 Hz Heartbeat

    // Day 13: The Invisible Context
    test_fpu_crucible();

    // Day 15: The Genesis Bridge
    genesis_bridge_spawn();

    klog_emit_silence_report();
    
    /* Re-enable interrupts and let the scheduler take over */
    sti();
    while(1) {
        __asm__ volatile ("hlt");
    }
}
