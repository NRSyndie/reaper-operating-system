#include "include/scheduler.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/pcid.h"
#include "include/console.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/port_io.h"
#include "include/cpu.h"
#include "include/syscall.h"
#include "include/capability.h"
#include "include/ocular.h"

#include "include/klog.h"

/* Assembly context switch function */
extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp, void* old_ext, void* new_ext, uint8_t fpu_mode);

static thread_t* current_thread = NULL;
static thread_t* ready_queue_head = NULL;
static thread_t* ready_queue_tail = NULL;

static thread_t* zombie_queue_head = NULL;
static thread_t* zombie_queue_tail = NULL;

static uint64_t schedule_count = 0;

/* The Kernel's own boot soul */
static thread_t boot_thread;
static process_t boot_process;

void scheduler_add_zombie(thread_t* t) {
    t->state = THREAD_ZOMBIE;
    t->next = NULL;
    if (!zombie_queue_head) {
        zombie_queue_head = t;
        zombie_queue_tail = t;
    } else {
        zombie_queue_tail->next = t;
        zombie_queue_tail = t;
    }
}

void scheduler_reap(void) {
    if (!zombie_queue_head) return;

    thread_t* current = scheduler_get_current();
    thread_t* prev = NULL;
    thread_t* list = zombie_queue_head;

    while (list) {
        thread_t* next = list->next;

        /* CRITICAL: Never reap the thread we are currently running on! */
        if (list == current) {
            prev = list;
            list = next;
            continue;
        }

                
        /* Remove from queue */
        if (prev) prev->next = next;
        else zombie_queue_head = next;
        if (list == zombie_queue_tail) zombie_queue_tail = prev;

        thread_destroy(list);
        list = next;
    }
}

void scheduler_init(void) {
    /* Create a placeholder for the current execution context (the boot thread) */
    boot_process.pml4_phys = read_cr3() & ~0xFFFULL;
    boot_process.pcid = 0;
    boot_process.pid = 0;
    boot_process.mode = MODE_KERNEL;

    boot_thread.tid = 0;
    boot_thread.owner = &boot_process;
    boot_thread.state = THREAD_RUNNING;
    boot_thread.ticks_remaining = DEFAULT_QUANTUM;
    
    /* Allocate extended state for boot thread (Zero-Residue) */
    uint64_t ext_phys = pmm_alloc(COLOR_VOID, 0);
    boot_thread.extended_state = pmm_phys_to_virt(ext_phys);
    memset(boot_thread.extended_state, 0, 4096);

    current_thread = &boot_thread;

    }

thread_t* scheduler_get_current(void) {
    return current_thread;
}

void scheduler_add(thread_t* t) {
        t->next = NULL;
    if (!ready_queue_head) {
        ready_queue_head = t;
        ready_queue_tail = t;
    } else {
        ready_queue_tail->next = t;
        ready_queue_tail = t;
    }
}

void schedule(void) {
    schedule_count++;
    if (schedule_count % 100 == 0) {
        scheduler_reap();
        cap_reaper();
    }

retry:
    if (!ready_queue_head) {
        /* No threads ready. If current is running, keep it. */
        if (current_thread && current_thread->state == THREAD_RUNNING) {
            return;
        }

        /* IDLE: We have no runnable context. Wait for an interrupt. */
        scheduler_reap();
        cap_reaper();
        ocular_project();

        __asm__ volatile ("sti; hlt; cli");
        goto retry;
    }

    thread_t* old = current_thread;
    
    /* ABYSS SENTRY: Verify stack integrity for managed souls */
    if (old->tid > 0 && old->stack_canary != 0) {
        uint64_t* canary_ptr = (uint64_t*)(old->kernel_stack_top - 4096);
        if (*canary_ptr != old->stack_canary) {
            kpanic("SCHEDULER: Stack Overflow detected! Soul TID:%d has fallen into the abyss.", old->tid);
        }
    }

    /* Simple Round Robin: Get next from head */
    thread_t* new = ready_queue_head;
    while (new && new->state != THREAD_READY) {
        ready_queue_head = new->next;
        new = ready_queue_head;
    }

    if (!new) {
        if (!ready_queue_head) ready_queue_tail = NULL;
        /* Re-check idle condition if no ready thread found in loop */
        if (current_thread && current_thread->state == THREAD_RUNNING) return;
        goto retry;
    }

    ready_queue_head = new->next;
    if (!ready_queue_head) ready_queue_tail = NULL;

    /* If old thread is still runnable, put it back at the end */
    if (old->state == THREAD_RUNNING) {
        old->state = THREAD_READY;
        scheduler_add(old);
    }

    new->state = THREAD_RUNNING;
    new->ticks_remaining = DEFAULT_QUANTUM;
    current_thread = new;

    if (old != new) {
        /* Log detail: switching contexts */
        /* To reduce spam, we could check a 'verbose' flag, but user requested "everything" */
        // 
        /* CRITICAL: Update TSS and Syscall stacks for the new thread's kernel stack */
        tss_set_stack(new->kernel_stack_top);
        syscall_set_kernel_stack(new->kernel_stack_top);

        /* CRITICAL: Phase Shift (Reality Switch) */
        if (old->owner->pml4_phys != new->owner->pml4_phys) {
            /*  */
            vmm_switch(new->owner->pml4_phys, new->owner->pcid, new->owner->mode);
        }

        context_switch(&old->rsp, new->rsp, old->extended_state, new->extended_state, cpu_get_fpu_mode());
    }
}

/* PIT Timer Setup (Fallback to start) */
void timer_init(uint32_t hz) {
        uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler(void) {
    if (!current_thread) return;
    
    if (current_thread->ticks_remaining > 0) {
        current_thread->ticks_remaining--;
    }

    if (current_thread->ticks_remaining == 0) {
                schedule();
    }
}
