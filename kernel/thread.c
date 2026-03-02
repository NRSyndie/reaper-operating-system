#include "include/thread.h"
#include "include/process.h"
#include "include/slab.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/utils.h"
#include "include/console.h"
#include "include/scheduler.h"
#include "include/klog.h"
#include "include/mode.h"

static uint32_t next_tid = 1;
static slab_cache_t* thread_cache = NULL;

thread_t* thread_create(process_t* owner, void (*entry)(void)) {
    if (!thread_cache) {
        thread_cache = slab_create_cache("ThreadCache", sizeof(thread_t), 8);
    }

    thread_t* t = (thread_t*)slab_alloc(thread_cache);
    if (!t) {
                return NULL;
    }
    memset(t, 0, sizeof(thread_t));

    /* Allocate private kernel stack (Void Color) */
    uint64_t stack_phys = pmm_alloc(COLOR_VOID, 0);
    if (!stack_phys) {
                slab_free(thread_cache, t);
        return NULL;
    }
    uint64_t stack_virt = (uint64_t)pmm_phys_to_virt(stack_phys) + 4096;

    /* Allocate Extended State Buffer (SSE/FPU/XSAVE) */
    uint64_t ext_phys = pmm_alloc(COLOR_VOID, 0);
    if (!ext_phys) {
                pmm_free(stack_phys);
        slab_free(thread_cache, t);
        return NULL;
    }
    t->extended_state = pmm_phys_to_virt(ext_phys);
    memset(t->extended_state, 0, 4096);

    t->tid = next_tid++;
    t->kernel_stack_top = stack_virt;
    t->stack_canary = 0x535441434B444541ULL; /* "STACKDEA" */
    
    /* Write canary to the absolute bottom of the stack frame */
    uint64_t* canary_ptr = (uint64_t*)(stack_virt - 4096);
    *canary_ptr = t->stack_canary;

    t->state = THREAD_READY;
    t->owner = owner;
    t->ticks_remaining = DEFAULT_QUANTUM;
    t->last_cpu = 0;
    t->sched_class = SCHED_CLASS_NORMAL;
    t->remaining_thread_budget = DEFAULT_QUANTUM;
    t->max_slice = DEFAULT_QUANTUM;
    t->refill_period_ticks = DEFAULT_QUANTUM;
    t->max_accumulated = SCHED_DEFAULT_MAX_ACCUMULATED;
    t->last_thread_refill = 0;
    t->sched_auth_slot = 0;
    t->sched_weight = 1;
    t->sched_tokens = 0;
    t->sched_auth_mode = owner ? (uint8_t)owner->mode : (uint8_t)MODE_CASUAL;
    t->sched_auth_required = (owner && owner->mode != MODE_KERNEL && owner->cspace != NULL);
    t->sched_auth_valid = !t->sched_auth_required;

    /* Initialize Day 11 Entry Lease */
    if (owner) {
        t->lease.id = 0; // Genesis lease for this thread
        t->lease.epoch = mode_get_security_epoch();
        t->lease.mode_mask = (1 << (uint8_t)owner->mode);
        t->lease.authority_mask = 0xFFFF;
    }

    owner->thread_count++;

    /* Prepare initial stack frame for context_switch */
    /* Stack layout: [RIP, RBX, RBP, R12, R13, R14, R15, RFLAGS] */
    uint64_t* stack = (uint64_t*)t->kernel_stack_top;
    
    stack[-1] = (uint64_t)entry;     /* RIP (Ret target) */
    stack[-2] = 0;                  /* RBX */
    stack[-3] = 0;                  /* RBP */
    stack[-4] = 0;                  /* R12 */
    stack[-5] = 0;                  /* R13 */
    stack[-6] = 0;                  /* R14 */
    stack[-7] = 0;                  /* R15 */
    stack[-8] = 0x202;              /* RFLAGS (IF=1, Reserved=1) */
    
    t->rsp = t->kernel_stack_top - (8 * 8);

    
    return t;
}

void thread_destroy(thread_t* thread) {
    if (!thread) return;

    process_t* owner = thread->owner;

    
    /* Free kernel stack */
    if (thread->kernel_stack_top) {
        void* stack_base = (void*)(thread->kernel_stack_top - 4096);
        uint64_t stack_phys = pmm_virt_to_phys(stack_base);
        hyper_scrub(stack_base, 4096);
        pmm_free(stack_phys);
    }

    /* Free extended state buffer */
    if (thread->extended_state) {
        hyper_scrub(thread->extended_state, 4096);
        uint64_t ext_phys = pmm_virt_to_phys(thread->extended_state);
        pmm_free(ext_phys);
    }

    slab_free(thread_cache, thread);

    /* If this was the last soul in the world, destroy the world */
    if (owner) {
        if (owner->waiter_thread == thread) {
            owner->waiter_thread = NULL;
        }
        owner->thread_count--;
        if (owner->thread_count == 0 && owner->pid != 0) {
            process_destroy(owner);
        }
    }
}

void thread_exit(void) {
    thread_t* current = scheduler_get_current();
    process_t* owner = current ? current->owner : NULL;

    /*
     * Wake a waiter in the same process before we fully yield to the reaper.
     * This establishes a minimal join-like SYS_WAIT contract.
     */
    if (owner) {
        owner->exit_events++;
    }

    if (owner && owner->waiter_thread && owner->waiter_thread != current) {
        thread_t* waiter = owner->waiter_thread;
        owner->waiter_thread = NULL;
        if (waiter->state == THREAD_BLOCKED) {
            scheduler_wake(waiter);
        }
    }

    /* Enqueue for the Reaper */
    scheduler_add_zombie(current);

    /* Force immediate reschedule */
    schedule();
    
    /* We should never return here */
    while(1);
}

void thread_yield(void) {
    schedule();
}
