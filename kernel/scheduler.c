#include "include/scheduler.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/pcid.h"
#include "include/console.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/port_io.h"
#include "include/cpu.h"
#include "include/mode.h"
#include "include/syscall.h"
#include "include/capability.h"
#include "include/ocular.h"
#include "include/utils.h"
#include "include/klog.h"

/* Assembly context switch function */
extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp, void* old_ext, void* new_ext, uint8_t fpu_mode);

typedef struct {
    thread_t* current_thread;
    thread_t* ready_queue_head;
    thread_t* ready_queue_tail;
    thread_t* zombie_queue_head;
    thread_t* zombie_queue_tail;
    uint64_t schedule_count;
    uint64_t switch_count;
    uint64_t remote_enqueue;
    uint64_t migrations;
    uint64_t denied_enqueue;
    uint64_t denied_wake;
    uint64_t denied_dispatch;
    spinlock_t runq_lock;
} scheduler_cpu_state_t;

static scheduler_cpu_state_t g_sched_states[SCHED_MAX_CPUS];

/* Boot context is static, but managed through the same scheduler state machine. */
static thread_t boot_thread;
static process_t boot_process;

static inline uint32_t sched_current_cpu_id(void) {
    uint32_t cpu_id = cpu_get_id();
    if (cpu_id >= SCHED_MAX_CPUS) {
        cpu_id = 0;
    }
    return cpu_id;
}

static inline scheduler_cpu_state_t* sched_cpu_state_by_id(uint32_t cpu_id) {
    if (cpu_id >= SCHED_MAX_CPUS) {
        cpu_id = 0;
    }
    return &g_sched_states[cpu_id];
}

static inline scheduler_cpu_state_t* sched_cpu_state(void) {
    return sched_cpu_state_by_id(sched_current_cpu_id());
}

static inline uint32_t sched_select_target_cpu(thread_t* thread) {
    if (!thread) return 0;
    if (thread->last_cpu < SCHED_MAX_CPUS) return thread->last_cpu;
    return thread->tid % SCHED_MAX_CPUS;
}

static bool scheduler_transition_valid(thread_state_t from, thread_state_t to) {
    if (from == to) return true;

    switch (from) {
        case THREAD_READY:
            return to == THREAD_RUNNING || to == THREAD_BLOCKED || to == THREAD_ZOMBIE;
        case THREAD_RUNNING:
            return to == THREAD_READY || to == THREAD_BLOCKED || to == THREAD_ZOMBIE;
        case THREAD_BLOCKED:
            return to == THREAD_READY || to == THREAD_ZOMBIE;
        case THREAD_ZOMBIE:
            return false;
        default:
            return false;
    }
}

static void ready_enqueue_locked(scheduler_cpu_state_t* cpu, thread_t* t) {
    t->next = NULL;
    if (!cpu->ready_queue_head) {
        cpu->ready_queue_head = t;
        cpu->ready_queue_tail = t;
        return;
    }

    cpu->ready_queue_tail->next = t;
    cpu->ready_queue_tail = t;
}

static thread_t* ready_dequeue_locked(scheduler_cpu_state_t* cpu) {
    thread_t* t = cpu->ready_queue_head;
    if (!t) return NULL;

    cpu->ready_queue_head = t->next;
    if (!cpu->ready_queue_head) {
        cpu->ready_queue_tail = NULL;
    }

    t->next = NULL;
    return t;
}

static void zombie_enqueue_locked(scheduler_cpu_state_t* cpu, thread_t* t) {
    t->next = NULL;
    if (!cpu->zombie_queue_head) {
        cpu->zombie_queue_head = t;
        cpu->zombie_queue_tail = t;
        return;
    }

    cpu->zombie_queue_tail->next = t;
    cpu->zombie_queue_tail = t;
}

static bool scheduler_mode_allowed(thread_t* target, thread_t* actor) {
    uint8_t global_mask;

    if (!target || !target->owner) return true;

    if (target->owner->mode == MODE_KERNEL) return true;

    global_mask = mode_get_current_mask();
    if (global_mask != 0 && (global_mask & (1U << (uint8_t)target->owner->mode)) == 0) {
        return false;
    }

    if (!actor || !actor->owner) return true;
    if (actor->owner->mode == MODE_KERNEL) return true;
    return actor->owner->mode == target->owner->mode;
}

static uint32_t queue_depth(thread_t* head) {
    uint32_t depth = 0;
    while (head) {
        depth++;
        head = head->next;
    }
    return depth;
}

void scheduler_set_state(thread_t* thread, thread_state_t new_state) {
    if (!thread) {
        kpanic("SCHEDULER: NULL thread passed to scheduler_set_state");
    }

    if (!scheduler_transition_valid(thread->state, new_state)) {
        kpanic("SCHEDULER: illegal state transition TID=%u from=%d to=%d",
               thread->tid,
               thread->state,
               new_state);
    }

    thread->state = new_state;
}

void scheduler_get_metrics(sched_metrics_t* out) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    uint64_t flags;

    if (!out) return;

    flags = spinlock_irqsave(&cpu->runq_lock);
    out->schedule_count = cpu->schedule_count;
    out->switch_count = cpu->switch_count;
    out->remote_enqueue = cpu->remote_enqueue;
    out->migrations = cpu->migrations;
    out->denied_enqueue = cpu->denied_enqueue;
    out->denied_wake = cpu->denied_wake;
    out->denied_dispatch = cpu->denied_dispatch;
    out->cpu_id = cpu_get_id();
    out->ready_depth = queue_depth(cpu->ready_queue_head);
    out->zombie_depth = queue_depth(cpu->zombie_queue_head);
    spinlock_irqrestore(&cpu->runq_lock, flags);
}

void scheduler_init(void) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    memset(g_sched_states, 0, sizeof(g_sched_states));

    memset(&boot_process, 0, sizeof(boot_process));
    memset(&boot_thread, 0, sizeof(boot_thread));

    boot_process.pml4_phys = read_cr3() & ~0xFFFULL;
    boot_process.pcid = 0;
    boot_process.pid = 0;
    boot_process.mode = MODE_KERNEL;
    boot_process.thread_count = 1;

    boot_thread.tid = 0;
    boot_thread.owner = &boot_process;
    boot_thread.state = THREAD_RUNNING;
    boot_thread.ticks_remaining = DEFAULT_QUANTUM;
    boot_thread.sched_class = SCHED_CLASS_SYSTEM;
    boot_thread.last_cpu = 0;

    uint64_t ext_phys = pmm_alloc(COLOR_VOID, 0);
    if (!ext_phys) {
        kpanic("SCHEDULER: failed to allocate boot thread extended_state");
    }

    boot_thread.extended_state = pmm_phys_to_virt(ext_phys);
    memset(boot_thread.extended_state, 0, 4096);

    cpu->current_thread = &boot_thread;
}

thread_t* scheduler_get_current(void) {
    return sched_cpu_state()->current_thread;
}

void scheduler_add(thread_t* thread) {
    scheduler_cpu_state_t* local_cpu = sched_cpu_state();
    uint32_t local_cpu_id = sched_current_cpu_id();
    uint32_t target_cpu_id = sched_select_target_cpu(thread);
    scheduler_cpu_state_t* target_cpu = sched_cpu_state_by_id(target_cpu_id);
    thread_t* actor = local_cpu->current_thread;
    uint64_t flags;

    if (!thread) return;

    flags = spinlock_irqsave(&target_cpu->runq_lock);

    if (thread->state == THREAD_ZOMBIE) {
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        kpanic("SCHEDULER: attempted to enqueue zombie TID=%u", thread->tid);
    }

    if (!scheduler_mode_allowed(thread, actor)) {
        target_cpu->denied_enqueue++;
        scheduler_set_state(thread, THREAD_BLOCKED);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    scheduler_set_state(thread, THREAD_READY);
    thread->last_cpu = target_cpu_id;
    ready_enqueue_locked(target_cpu, thread);

    spinlock_irqrestore(&target_cpu->runq_lock, flags);

    if (target_cpu_id != local_cpu_id) {
        local_cpu->remote_enqueue++;
    }
}

void scheduler_block(thread_t* thread) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    uint64_t flags;

    if (!thread) return;

    flags = spinlock_irqsave(&cpu->runq_lock);
    scheduler_set_state(thread, THREAD_BLOCKED);
    spinlock_irqrestore(&cpu->runq_lock, flags);
}

void scheduler_wake(thread_t* thread) {
    scheduler_cpu_state_t* local_cpu = sched_cpu_state();
    uint32_t local_cpu_id = sched_current_cpu_id();
    uint32_t target_cpu_id = sched_select_target_cpu(thread);
    scheduler_cpu_state_t* target_cpu = sched_cpu_state_by_id(target_cpu_id);
    thread_t* actor = local_cpu->current_thread;
    uint64_t flags;

    if (!thread) return;

    flags = spinlock_irqsave(&target_cpu->runq_lock);

    if (thread->state != THREAD_ZOMBIE) {
        if (!scheduler_mode_allowed(thread, actor)) {
            target_cpu->denied_wake++;
            scheduler_set_state(thread, THREAD_BLOCKED);
            spinlock_irqrestore(&target_cpu->runq_lock, flags);
            return;
        }

        scheduler_set_state(thread, THREAD_READY);
        thread->last_cpu = target_cpu_id;
        ready_enqueue_locked(target_cpu, thread);
    }

    spinlock_irqrestore(&target_cpu->runq_lock, flags);

    if (target_cpu_id != local_cpu_id) {
        local_cpu->remote_enqueue++;
    }
}

void scheduler_add_zombie(thread_t* thread) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    uint64_t flags;

    if (!thread) return;

    flags = spinlock_irqsave(&cpu->runq_lock);
    scheduler_set_state(thread, THREAD_ZOMBIE);
    zombie_enqueue_locked(cpu, thread);
    spinlock_irqrestore(&cpu->runq_lock, flags);
}

void scheduler_reap(void) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    thread_t* to_destroy[SCHED_REAP_BUDGET];
    uint32_t destroy_count = 0;
    uint64_t flags = spinlock_irqsave(&cpu->runq_lock);

    while (destroy_count < SCHED_REAP_BUDGET && cpu->zombie_queue_head) {
        thread_t* victim = cpu->zombie_queue_head;
        cpu->zombie_queue_head = victim->next;
        if (!cpu->zombie_queue_head) {
            cpu->zombie_queue_tail = NULL;
        }

        victim->next = NULL;

        if (victim == cpu->current_thread) {
            zombie_enqueue_locked(cpu, victim);
            break;
        }

        to_destroy[destroy_count++] = victim;
    }

    spinlock_irqrestore(&cpu->runq_lock, flags);

    for (uint32_t i = 0; i < destroy_count; i++) {
        thread_destroy(to_destroy[i]);
    }
}

void schedule(void) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    thread_t* old;
    thread_t* next;
    uint64_t flags = read_rflags();

    cli();
    spinlock_acquire(&cpu->runq_lock);

    cpu->schedule_count++;

retry_pick:
    old = cpu->current_thread;

    if (!cpu->ready_queue_head) {
        if (old && old->state == THREAD_RUNNING) {
            spinlock_release(&cpu->runq_lock);
            write_rflags(flags);
            return;
        }

        spinlock_release(&cpu->runq_lock);

        scheduler_reap();
        cap_reaper();
        ocular_project();

        __asm__ volatile ("sti; hlt; cli");
        spinlock_acquire(&cpu->runq_lock);
        goto retry_pick;
    }

    if (old && old->tid > 0 && old->stack_canary != 0) {
        uint64_t* canary_ptr = (uint64_t*)(old->kernel_stack_top - 4096);
        if (*canary_ptr != old->stack_canary) {
            kpanic("SCHEDULER: Stack Overflow detected! TID=%u", old->tid);
        }
    }

    next = ready_dequeue_locked(cpu);
    while (next && next->state != THREAD_READY) {
        next = ready_dequeue_locked(cpu);
    }

    while (next && !scheduler_mode_allowed(next, old)) {
        cpu->denied_dispatch++;
        scheduler_set_state(next, THREAD_BLOCKED);
        next = ready_dequeue_locked(cpu);
        while (next && next->state != THREAD_READY) {
            next = ready_dequeue_locked(cpu);
        }
    }

    if (!next) {
        goto retry_pick;
    }

    if (old && old->state == THREAD_RUNNING && old != next) {
        scheduler_set_state(old, THREAD_READY);
        ready_enqueue_locked(cpu, old);
    }

    scheduler_set_state(next, THREAD_RUNNING);
    next->ticks_remaining = DEFAULT_QUANTUM;
    next->last_cpu = cpu_get_id();
    cpu->current_thread = next;

    spinlock_release(&cpu->runq_lock);

    if (old == next) {
        write_rflags(flags);
        return;
    }

    cpu->switch_count++;
    if (old && old->last_cpu != next->last_cpu) {
        cpu->migrations++;
    }

    tss_set_stack(next->kernel_stack_top);
    syscall_set_kernel_stack(next->kernel_stack_top);

    if (old && old->owner && next->owner && old->owner->pml4_phys != next->owner->pml4_phys) {
        vmm_switch(next->owner->pml4_phys, next->owner->pcid, next->owner->mode);
    }

    context_switch(&old->rsp, next->rsp, old->extended_state, next->extended_state, cpu_get_fpu_mode());
}

/* PIT Timer Setup (Fallback to start) */
void timer_init(uint32_t hz) {
    uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler(void) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    thread_t* current = cpu->current_thread;

    if (!current) return;

    if (current->ticks_remaining > 0) {
        current->ticks_remaining--;
    }

    if (cpu->schedule_count != 0 && (cpu->schedule_count % 100) == 0) {
        scheduler_reap();
        cap_reaper();
    }

    if (cpu->schedule_count != 0 && (cpu->schedule_count % 1000) == 0) {
        if (cpu->denied_enqueue || cpu->denied_wake || cpu->denied_dispatch) {
            klog_warn("SCHED[%u]: schedule=%lu switch=%lu remote=%lu mig=%lu deny{enq=%lu wake=%lu disp=%lu} q{r=%u z=%u}",
                      sched_current_cpu_id(),
                      cpu->schedule_count,
                      cpu->switch_count,
                      cpu->remote_enqueue,
                      cpu->migrations,
                      cpu->denied_enqueue,
                      cpu->denied_wake,
                      cpu->denied_dispatch,
                      queue_depth(cpu->ready_queue_head),
                      queue_depth(cpu->zombie_queue_head));
        }
    }

    if (current->ticks_remaining == 0) {
        schedule();
    }
}
