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
#include "include/kmalloc.h"

/* Assembly context switch function */
extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp, void* old_ext, void* new_ext, uint8_t fpu_mode);

typedef struct {
    thread_t* current_thread;
    scheduler_envelope_t envelopes[MODE_KERNEL + 1];
    uint64_t schedule_count;
    uint64_t switch_count;
    uint64_t remote_enqueue;
    uint64_t migrations;
    uint64_t denied_enqueue;
    uint64_t denied_wake;
    uint64_t denied_dispatch;
    uint64_t denied_no_auth;
    uint64_t denied_mode_mismatch;
    uint64_t budget_exhaustions;
    uint64_t envelope_switches;
    uint64_t tick_count;
    uint64_t active_security_epoch;
    mode_id_t active_mode;
    spinlock_t runq_lock;
} scheduler_cpu_state_t;

static scheduler_cpu_state_t g_sched_states[SCHED_MAX_CPUS];
static volatile uint8_t g_sched_global_mode = MODE_CASUAL;
static volatile uint64_t g_sched_global_security_epoch = 1;
static volatile uint64_t g_sched_global_tick = 0;
static volatile uint8_t g_force_resched[SCHED_MAX_CPUS];
static const uint32_t SCHED_AUTH_MAGIC = 0x53415554u; /* SAUT */

enum {
    SCHED_EVENT_REVOKE_THREAD = 0x30,
    SCHED_EVENT_REVOKE_PROCESS = 0x31,
    SCHED_EVENT_BUDGET_EXHAUST = 0x32,
    SCHED_EVENT_AUTH_DENY = 0x33,
    SCHED_EVENT_MODE_DENY = 0x34,
    SCHED_EVENT_GHOST_FLUSH = 0x35
};

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

static inline void sched_request_resched(uint32_t cpu_id) {
    if (cpu_id >= SCHED_MAX_CPUS) cpu_id = 0;
    __atomic_store_n(&g_force_resched[cpu_id], 1, __ATOMIC_RELEASE);
}

static inline bool sched_consume_resched_request(uint32_t cpu_id) {
    uint8_t prev;
    if (cpu_id >= SCHED_MAX_CPUS) cpu_id = 0;
    prev = __atomic_exchange_n(&g_force_resched[cpu_id], 0, __ATOMIC_ACQ_REL);
    return prev != 0;
}

static inline scheduler_cpu_state_t* sched_cpu_state(void) {
    return sched_cpu_state_by_id(sched_current_cpu_id());
}

static inline bool sched_mode_valid(mode_id_t mode) {
    return mode >= MODE_VOID && mode <= MODE_KERNEL;
}

static inline mode_id_t sched_thread_mode(const thread_t* thread) {
    if (!thread || !thread->owner) return MODE_KERNEL;
    if (!sched_mode_valid(thread->owner->mode)) return MODE_KERNEL;
    return thread->owner->mode;
}

static inline scheduler_envelope_t* sched_envelope(scheduler_cpu_state_t* cpu, mode_id_t mode) {
    if (!cpu) return NULL;
    if (!sched_mode_valid(mode)) mode = MODE_CASUAL;
    return &cpu->envelopes[(uint8_t)mode];
}

static inline scheduler_envelope_t* sched_active_envelope(scheduler_cpu_state_t* cpu) {
    if (!cpu) return NULL;
    return sched_envelope(cpu, cpu->active_mode);
}

static inline uint32_t sched_select_target_cpu(thread_t* thread) {
    if (!thread) return 0;
    if (thread->last_cpu < SCHED_MAX_CPUS) return thread->last_cpu;
    return thread->tid % SCHED_MAX_CPUS;
}

static void sched_refresh_active_mode(scheduler_cpu_state_t* cpu) {
    mode_id_t desired = (mode_id_t)__atomic_load_n(&g_sched_global_mode, __ATOMIC_ACQUIRE);
    uint64_t epoch = __atomic_load_n(&g_sched_global_security_epoch, __ATOMIC_ACQUIRE);

    if (!sched_mode_valid(desired)) desired = MODE_CASUAL;

    if (cpu->active_mode != desired) {
        klog_info("SCHED_ENVELOPE_SWITCH cpu=%u from=%u to=%u epoch=%lu",
                  sched_current_cpu_id(),
                  cpu->active_mode,
                  desired,
                  epoch);
        cpu->active_mode = desired;
        cpu->envelope_switches++;
        if (desired == MODE_GHOST) {
            scheduler_envelope_t* ghost_env = sched_envelope(cpu, MODE_GHOST);
            if (ghost_env) {
                ghost_env->dispatch_count = 0;
                ghost_env->denied_mode = 0;
                ghost_env->denied_auth = 0;
                ghost_env->budget_exhaustions = 0;
            }
        }
    }
    cpu->active_security_epoch = epoch;
}

static bool scheduler_transition_valid(thread_state_t from, thread_state_t to) {
    if (from == to) return true;

    switch (from) {
        case THREAD_READY:
            return to == THREAD_RUNNING || to == THREAD_BLOCKED || to == THREAD_BLOCKED_AUTH ||
                   to == THREAD_SUSPENDED_MODE || to == THREAD_ZOMBIE;
        case THREAD_RUNNING:
            return to == THREAD_READY || to == THREAD_BLOCKED || to == THREAD_BLOCKED_AUTH ||
                   to == THREAD_SUSPENDED_MODE || to == THREAD_ZOMBIE;
        case THREAD_BLOCKED:
            return to == THREAD_READY || to == THREAD_BLOCKED_AUTH ||
                   to == THREAD_SUSPENDED_MODE || to == THREAD_ZOMBIE;
        case THREAD_BLOCKED_AUTH:
            return to == THREAD_READY || to == THREAD_BLOCKED ||
                   to == THREAD_SUSPENDED_MODE || to == THREAD_ZOMBIE;
        case THREAD_SUSPENDED_MODE:
            return to == THREAD_READY || to == THREAD_BLOCKED ||
                   to == THREAD_BLOCKED_AUTH || to == THREAD_ZOMBIE;
        case THREAD_ZOMBIE:
            return false;
        default:
            return false;
    }
}

static void ready_enqueue_locked(scheduler_envelope_t* env, thread_t* t) {
    t->next = NULL;
    if (!env->ready_queue_head) {
        env->ready_queue_head = t;
        env->ready_queue_tail = t;
        return;
    }

    env->ready_queue_tail->next = t;
    env->ready_queue_tail = t;
}

static void ready_enqueue_head_locked(scheduler_envelope_t* env, thread_t* t) {
    if (!env || !t) return;
    t->next = env->ready_queue_head;
    env->ready_queue_head = t;
    if (!env->ready_queue_tail) {
        env->ready_queue_tail = t;
    }
}

static thread_t* ready_dequeue_locked(scheduler_envelope_t* env) {
    thread_t* t = env->ready_queue_head;
    if (!t) return NULL;

    env->ready_queue_head = t->next;
    if (!env->ready_queue_head) {
        env->ready_queue_tail = NULL;
    }

    t->next = NULL;
    return t;
}

static bool ready_remove_locked(scheduler_envelope_t* env, thread_t* target) {
    thread_t* prev = NULL;
    thread_t* cur;
    if (!env || !target) return false;

    cur = env->ready_queue_head;
    while (cur) {
        if (cur == target) {
            if (prev) prev->next = cur->next;
            else env->ready_queue_head = cur->next;
            if (env->ready_queue_tail == cur) env->ready_queue_tail = prev;
            cur->next = NULL;
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

static void zombie_enqueue_locked(scheduler_envelope_t* env, thread_t* t) {
    t->next = NULL;
    if (!env->zombie_queue_head) {
        env->zombie_queue_head = t;
        env->zombie_queue_tail = t;
        return;
    }

    env->zombie_queue_tail->next = t;
    env->zombie_queue_tail = t;
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

static bool scheduler_auth_allowed(thread_t* target, mode_id_t active_mode) {
    process_t* owner;
    cap_identity_t* root_ident;
    cap_identity_t* thread_ident;
    sched_auth_obj_t* root_auth;
    sched_auth_obj_t* thread_auth;

    if (!target) return false;
    if (!target->sched_auth_required) return true;
    owner = target->owner;
    if (!owner) return false;

    root_ident = cap_lookup(owner->cspace, owner->sched_auth_root_slot);
    if (!root_ident || root_ident->type != CAP_TYPE_SCHED_AUTH_ROOT) {
        owner->sched_auth_root_valid = false;
        target->sched_auth_valid = false;
        return false;
    }

    root_auth = (sched_auth_obj_t*)root_ident->object_ptr;
    if (!root_auth || root_auth->magic != SCHED_AUTH_MAGIC || root_auth->kind != SCHED_AUTH_ROOT) {
        owner->sched_auth_root_valid = false;
        target->sched_auth_valid = false;
        return false;
    }
    owner->sched_auth_root_valid = true;
    owner->max_total_budget = root_auth->max_total_budget;
    owner->refill_period_ticks = root_auth->refill_period_ticks;

    thread_ident = cap_lookup(owner->cspace, target->sched_auth_slot);
    if (!thread_ident || thread_ident->type != CAP_TYPE_SCHED_AUTH_THREAD) {
        target->sched_auth_valid = false;
        return false;
    }
    thread_auth = (sched_auth_obj_t*)thread_ident->object_ptr;
    if (!thread_auth || thread_auth->magic != SCHED_AUTH_MAGIC || thread_auth->kind != SCHED_AUTH_THREAD) {
        target->sched_auth_valid = false;
        return false;
    }
    if (thread_auth->root != root_auth) {
        target->sched_auth_valid = false;
        return false;
    }
    target->sched_auth_valid = true;
    target->sched_auth_mode = thread_auth->mode_binding;
    return target->sched_auth_mode == (uint8_t)active_mode;
}

static bool scheduler_lease_valid(thread_t* thread, mode_id_t active_mode, uint64_t security_epoch) {
    if (!thread) return false;
    if (thread->owner && thread->owner->mode == MODE_KERNEL) return true;
    
    /* Hot-path cache check */
    if (thread->lease_validated_epoch == security_epoch) {
        return thread->lease_valid_cache;
    }

    bool valid = true;

    /* 1. Epoch check: Lease must not be stale */
    if (thread->lease.epoch < security_epoch) {
        valid = false;
        klog_debug("ENTRY_REJECT_EPOCH tid=%u lease_epoch=%lu current_epoch=%lu", 
                   thread->tid, thread->lease.epoch, security_epoch);
    }

    /* 2. Mode check: Lease must match active reality */
    if (valid && !(thread->lease.mode_mask & (1 << (uint8_t)active_mode))) {
        valid = false;
        klog_debug("ENTRY_REJECT_MODE tid=%u lease_mask=0x%x active_mode=%u",
                   thread->tid, thread->lease.mode_mask, (unsigned)active_mode);
    }

    /* Update cache */
    thread->lease_validated_epoch = security_epoch;
    thread->lease_valid_cache = valid;

    return valid;
}

static uint64_t process_budget_try_consume(process_t* proc, uint64_t request) {
    uint64_t current;
    uint64_t consume;
    uint64_t updated;

    if (!proc || request == 0) return 0;

    while (1) {
        current = __atomic_load_n(&proc->remaining_process_budget, __ATOMIC_ACQUIRE);
        if (current == 0) return 0;
        consume = (request < current) ? request : current;
        updated = current - consume;
        if (__atomic_compare_exchange_n(&proc->remaining_process_budget,
                                        &current,
                                        updated,
                                        false,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            return consume;
        }
    }
}

static void scheduler_refill_process_budget(process_t* proc, uint64_t now_tick) {
    uint64_t observed_last;
    uint64_t refills;
    uint64_t grant;
    uint64_t period;
    uint64_t target_last;
    uint64_t current_budget;
    uint64_t next_budget;

    if (!proc) return;
    if (!proc->sched_auth_root_valid) return;
    if (proc->max_total_budget == 0) return;
    period = proc->refill_period_ticks;
    if (period < SCHED_MIN_REFILL_PERIOD) period = SCHED_MIN_REFILL_PERIOD;

    observed_last = __atomic_load_n(&proc->last_process_refill, __ATOMIC_ACQUIRE);
    if (observed_last == 0) {
        uint64_t zero = 0;
        if (__atomic_compare_exchange_n(&proc->last_process_refill,
                                        &zero,
                                        now_tick,
                                        false,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            uint64_t cur = __atomic_load_n(&proc->remaining_process_budget, __ATOMIC_ACQUIRE);
            if (cur == 0) {
                __atomic_store_n(&proc->remaining_process_budget, proc->max_total_budget, __ATOMIC_RELEASE);
            }
        }
        return;
    }
    if (now_tick <= observed_last) return;

    while (1) {
        observed_last = __atomic_load_n(&proc->last_process_refill, __ATOMIC_ACQUIRE);
        if (now_tick <= observed_last) return;
        refills = (now_tick - observed_last) / period;
        if (refills == 0) return;
        target_last = observed_last + (refills * period);
        if (__atomic_compare_exchange_n(&proc->last_process_refill,
                                        &observed_last,
                                        target_last,
                                        false,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            break;
        }
    }

    grant = refills * proc->max_total_budget;
    while (1) {
        current_budget = __atomic_load_n(&proc->remaining_process_budget, __ATOMIC_ACQUIRE);
        if (current_budget + grant < current_budget) {
            next_budget = proc->max_total_budget;
        } else {
            next_budget = current_budget + grant;
            if (next_budget > proc->max_total_budget) {
                next_budget = proc->max_total_budget;
            }
        }
        if (__atomic_compare_exchange_n(&proc->remaining_process_budget,
                                        &current_budget,
                                        next_budget,
                                        false,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_ACQUIRE)) {
            break;
        }
    }
}

static void scheduler_refill_budget(thread_t* thread, uint64_t now_tick) {
    uint64_t elapsed;
    uint64_t refills;
    uint64_t grant;
    uint32_t period;

    if (!thread) return;
    if (thread->max_slice == 0) thread->max_slice = DEFAULT_QUANTUM;
    if (thread->max_accumulated == 0) thread->max_accumulated = SCHED_DEFAULT_MAX_ACCUMULATED;
    scheduler_refill_process_budget(thread->owner, now_tick);

    period = thread->refill_period_ticks;
    if (period < SCHED_MIN_REFILL_PERIOD) {
        period = SCHED_MIN_REFILL_PERIOD;
        thread->refill_period_ticks = period;
    }

    if (thread->last_thread_refill == 0) {
        thread->last_thread_refill = now_tick;
        if (thread->remaining_thread_budget == 0) {
            thread->remaining_thread_budget = thread->max_slice;
            if (thread->owner) {
                uint64_t proc_budget = __atomic_load_n(&thread->owner->remaining_process_budget, __ATOMIC_ACQUIRE);
                if (proc_budget < thread->remaining_thread_budget) {
                    thread->remaining_thread_budget = proc_budget;
                }
            }
        }
        return;
    }

    if (now_tick <= thread->last_thread_refill) return;

    elapsed = now_tick - thread->last_thread_refill;
    refills = elapsed / period;
    if (refills == 0) return;

    grant = refills * (uint64_t)thread->max_slice;
    if (thread->remaining_thread_budget + grant < thread->remaining_thread_budget) {
        thread->remaining_thread_budget = thread->max_accumulated;
    } else {
        thread->remaining_thread_budget += grant;
        if (thread->remaining_thread_budget > thread->max_accumulated) {
            thread->remaining_thread_budget = thread->max_accumulated;
        }
        if (thread->owner) {
            uint64_t proc_budget = __atomic_load_n(&thread->owner->remaining_process_budget, __ATOMIC_ACQUIRE);
            if (thread->remaining_thread_budget > proc_budget) {
                thread->remaining_thread_budget = proc_budget;
            }
        }
    }
    thread->last_thread_refill += refills * period;
}

static uint32_t queue_depth(thread_t* head) {
    uint32_t depth = 0;
    while (head) {
        depth++;
        head = head->next;
    }
    return depth;
}

static thread_t* scheduler_pick_next_weighted_locked(scheduler_envelope_t* env) {
    uint32_t depth;
    thread_t* next;
    if (!env) return NULL;

    depth = queue_depth(env->ready_queue_head);
    while (depth-- > 0) {
        next = ready_dequeue_locked(env);
        if (!next) return NULL;
        if (next->state != THREAD_READY) {
            continue;
        }

        if (next->sched_tokens == 0) {
            next->sched_tokens = next->sched_weight ? next->sched_weight : 1;
        }

        if (next->sched_tokens > 0) {
            next->sched_tokens--;
            return next;
        }

        ready_enqueue_locked(env, next);
    }
    return NULL;
}

static thread_t* scheduler_try_same_mode_steal(mode_id_t mode, uint32_t local_cpu_id) {
    for (uint32_t donor_id = 0; donor_id < SCHED_MAX_CPUS; donor_id++) {
        scheduler_cpu_state_t* donor;
        scheduler_envelope_t* donor_env;
        uint64_t dflags;
        thread_t* stolen;
        if (donor_id == local_cpu_id) continue;

        donor = sched_cpu_state_by_id(donor_id);
        dflags = spinlock_irqsave(&donor->runq_lock);
        donor_env = sched_envelope(donor, mode);
        stolen = scheduler_pick_next_weighted_locked(donor_env);
        if (stolen) {
            spinlock_irqrestore(&donor->runq_lock, dflags);
            return stolen;
        }
        spinlock_irqrestore(&donor->runq_lock, dflags);
    }
    return NULL;
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
    scheduler_envelope_t* env;
    uint64_t flags;

    if (!out) return;

    flags = spinlock_irqsave(&cpu->runq_lock);
    sched_refresh_active_mode(cpu);
    env = sched_active_envelope(cpu);
    out->schedule_count = cpu->schedule_count;
    out->switch_count = cpu->switch_count;
    out->remote_enqueue = cpu->remote_enqueue;
    out->migrations = cpu->migrations;
    out->denied_enqueue = cpu->denied_enqueue;
    out->denied_wake = cpu->denied_wake;
    out->denied_dispatch = cpu->denied_dispatch;
    out->denied_no_auth = cpu->denied_no_auth;
    out->denied_mode_mismatch = cpu->denied_mode_mismatch;
    out->budget_exhaustions = cpu->budget_exhaustions;
    out->envelope_switches = cpu->envelope_switches;
    out->active_security_epoch = cpu->active_security_epoch;
    out->cpu_id = cpu_get_id();
    out->active_mode = (uint32_t)cpu->active_mode;
    out->ready_depth = env ? queue_depth(env->ready_queue_head) : 0;
    out->zombie_depth = env ? queue_depth(env->zombie_queue_head) : 0;
    spinlock_irqrestore(&cpu->runq_lock, flags);
}

uint64_t scheduler_get_global_tick(void) {
    return __atomic_load_n(&g_sched_global_tick, __ATOMIC_ACQUIRE);
}

void scheduler_init(void) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    mode_id_t mode_now = mode_get_current();
    memset(g_sched_states, 0, sizeof(g_sched_states));

    memset(&boot_process, 0, sizeof(boot_process));
    memset(&boot_thread, 0, sizeof(boot_thread));

    for (uint32_t i = 0; i < SCHED_MAX_CPUS; i++) {
        for (uint32_t m = MODE_VOID; m <= MODE_KERNEL; m++) {
            g_sched_states[i].envelopes[m].mode = (mode_id_t)m;
        }
        g_sched_states[i].active_mode = sched_mode_valid(mode_now) ? mode_now : MODE_CASUAL;
        g_sched_states[i].active_security_epoch = mode_get_security_epoch();
    }

    __atomic_store_n(&g_sched_global_mode, (uint8_t)(sched_mode_valid(mode_now) ? mode_now : MODE_CASUAL), __ATOMIC_RELEASE);
    __atomic_store_n(&g_sched_global_security_epoch, mode_get_security_epoch(), __ATOMIC_RELEASE);

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
    boot_thread.remaining_thread_budget = SCHED_DEFAULT_MAX_ACCUMULATED;
    boot_thread.max_slice = DEFAULT_QUANTUM;
    boot_thread.refill_period_ticks = DEFAULT_QUANTUM;
    boot_thread.max_accumulated = SCHED_DEFAULT_MAX_ACCUMULATED;
    boot_thread.last_thread_refill = 1;
    boot_thread.sched_weight = 1;
    boot_thread.sched_tokens = 0;
    boot_thread.sched_auth_required = false;
    boot_thread.sched_auth_valid = true;
    boot_thread.sched_auth_mode = MODE_KERNEL;

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

void scheduler_on_mode_transition(mode_id_t from_mode, mode_id_t to_mode, uint64_t security_epoch) {
    (void)from_mode;
    if (!sched_mode_valid(to_mode)) return;

    __atomic_store_n(&g_sched_global_mode, (uint8_t)to_mode, __ATOMIC_RELEASE);
    __atomic_store_n(&g_sched_global_security_epoch, security_epoch, __ATOMIC_RELEASE);
}

static uint8_t sched_mode_to_cap_mask(mode_id_t mode) {
    switch (mode) {
        case MODE_VOID: return CAP_MODE_VOID;
        case MODE_CASUAL: return CAP_MODE_CASUAL;
        case MODE_SECURE: return CAP_MODE_SECURE;
        case MODE_LOCKDOWN: return CAP_MODE_LOCKDOWN;
        case MODE_GHOST: return CAP_MODE_GHOST;
        case MODE_KERNEL: return CAP_MODE_ALL;
        default: return 0;
    }
}

int scheduler_mint_root_auth(process_t* proc,
                             uint32_t dst_slot,
                             mode_id_t mode_binding,
                             uint64_t max_total_budget,
                             uint64_t refill_period_ticks,
                             uint64_t max_accumulated) {
    sched_auth_obj_t* auth;
    cap_identity_t* ident;
    uint8_t mode_mask;

    if (!proc || !proc->cspace) return -1;
    if (!sched_mode_valid(mode_binding) || mode_binding == MODE_KERNEL) return -1;
    if (max_total_budget == 0 || refill_period_ticks == 0 || max_accumulated == 0) return -1;
    mode_mask = sched_mode_to_cap_mask(mode_binding);
    if (mode_mask == 0) return -1;

    auth = (sched_auth_obj_t*)kzalloc(sizeof(*auth));
    if (!auth) return -1;
    auth->magic = SCHED_AUTH_MAGIC;
    auth->kind = SCHED_AUTH_ROOT;
    auth->mode_binding = (uint8_t)mode_binding;
    auth->max_total_budget = max_total_budget;
    auth->refill_period_ticks = refill_period_ticks;
    auth->max_accumulated = max_accumulated;
    auth->owner_process = proc;

    ident = cap_identity_create((uint64_t)auth,
                                CAP_TYPE_SCHED_AUTH_ROOT,
                                CAP_RIGHT_READ | CAP_RIGHT_GRANT,
                                0,
                                mode_mask);
    if (!ident) {
        kfree(auth);
        return -1;
    }

    if (cap_insert(proc->cspace, dst_slot, ident) != 0) {
        cap_identity_free(ident);
        return -1;
    }

    proc->sched_auth_root_slot = dst_slot;
    proc->sched_auth_root_valid = true;
    proc->max_total_budget = max_total_budget;
    proc->refill_period_ticks = refill_period_ticks;
    __atomic_store_n(&proc->remaining_process_budget, max_total_budget, __ATOMIC_RELEASE);
    proc->last_process_refill = 0;
    return 0;
}

int scheduler_derive_thread_auth(process_t* proc,
                                 thread_t* thread,
                                 uint32_t root_slot,
                                 uint32_t dst_slot,
                                 uint32_t max_slice,
                                 uint32_t weight,
                                 uint64_t local_max_accumulated) {
    cap_identity_t* root_ident;
    cap_identity_t* thread_ident;
    sched_auth_obj_t* root_auth;
    sched_auth_obj_t* derived;
    uint8_t mode_mask;

    if (!proc || !thread || !proc->cspace) return -1;
    if (max_slice == 0 || weight == 0 || local_max_accumulated == 0) return -1;

    root_ident = cap_lookup(proc->cspace, root_slot);
    if (!root_ident || root_ident->type != CAP_TYPE_SCHED_AUTH_ROOT) return -1;
    root_auth = (sched_auth_obj_t*)root_ident->object_ptr;
    if (!root_auth || root_auth->magic != SCHED_AUTH_MAGIC || root_auth->kind != SCHED_AUTH_ROOT) return -1;

    if (max_slice > root_auth->max_total_budget) return -1;
    if (local_max_accumulated > root_auth->max_accumulated) return -1;

    derived = (sched_auth_obj_t*)kzalloc(sizeof(*derived));
    if (!derived) return -1;
    derived->magic = SCHED_AUTH_MAGIC;
    derived->kind = SCHED_AUTH_THREAD;
    derived->mode_binding = root_auth->mode_binding;
    derived->max_total_budget = root_auth->max_total_budget;
    derived->refill_period_ticks = root_auth->refill_period_ticks;
    derived->max_accumulated = root_auth->max_accumulated;
    derived->max_slice = max_slice;
    derived->weight = weight;
    derived->local_max_accumulated = local_max_accumulated;
    derived->root = root_auth;
    derived->owner_process = proc;
    derived->bound_thread = thread;

    mode_mask = sched_mode_to_cap_mask((mode_id_t)root_auth->mode_binding);
    thread_ident = cap_identity_create((uint64_t)derived,
                                       CAP_TYPE_SCHED_AUTH_THREAD,
                                       CAP_RIGHT_READ,
                                       0,
                                       mode_mask);
    if (!thread_ident) {
        kfree(derived);
        return -1;
    }

    if (cap_insert(proc->cspace, dst_slot, thread_ident) != 0) {
        cap_identity_free(thread_ident);
        return -1;
    }

    thread->sched_auth_slot = (uint16_t)dst_slot;
    thread->sched_auth_mode = root_auth->mode_binding;
    thread->sched_weight = (uint8_t)(weight > 255 ? 255 : weight);
    thread->sched_tokens = 0;
    thread->max_slice = max_slice;
    thread->max_accumulated = local_max_accumulated;
    thread->refill_period_ticks = (uint32_t)root_auth->refill_period_ticks;
    thread->remaining_thread_budget = max_slice;
    thread->sched_auth_required = true;
    thread->sched_auth_valid = true;
    return 0;
}

void scheduler_revoke_thread_immediate(thread_t* thread) {
    uint64_t flags;
    if (!thread) return;

    thread->sched_auth_valid = false;
    thread->sched_tokens = 0;
    mode_log_sched_event(thread->owner ? thread->owner->pid : 0,
                         SCHED_EVENT_REVOKE_THREAD,
                         thread->tid,
                         FATE_RESULT_REJECTED);

    for (uint32_t c = 0; c < SCHED_MAX_CPUS; c++) {
        scheduler_cpu_state_t* cpu = sched_cpu_state_by_id(c);
        for (uint32_t m = MODE_VOID; m <= MODE_KERNEL; m++) {
            scheduler_envelope_t* env = &cpu->envelopes[m];
            flags = spinlock_irqsave(&cpu->runq_lock);
            (void)ready_remove_locked(env, thread);
            if (thread->state == THREAD_READY || thread->state == THREAD_RUNNING) {
                scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
                thread->ticks_remaining = 0;
            }
            if (cpu->current_thread == thread) {
                sched_request_resched(c);
            }
            spinlock_irqrestore(&cpu->runq_lock, flags);
        }
    }

    if (scheduler_get_current() == thread) {
        schedule();
    }
}

void scheduler_revoke_process_immediate(process_t* proc) {
    uint64_t flags;
    if (!proc) return;

    proc->sched_auth_root_valid = false;
    __atomic_store_n(&proc->remaining_process_budget, 0, __ATOMIC_RELEASE);
    mode_log_sched_event(proc->pid,
                         SCHED_EVENT_REVOKE_PROCESS,
                         proc->sched_auth_root_slot,
                         FATE_RESULT_REJECTED);

    for (uint32_t c = 0; c < SCHED_MAX_CPUS; c++) {
        scheduler_cpu_state_t* cpu = sched_cpu_state_by_id(c);
        for (uint32_t m = MODE_VOID; m <= MODE_KERNEL; m++) {
            scheduler_envelope_t* env = &cpu->envelopes[m];
            thread_t* cur;
            thread_t* prev = NULL;
            flags = spinlock_irqsave(&cpu->runq_lock);

            cur = env->ready_queue_head;
            while (cur) {
                thread_t* next = cur->next;
                if (cur->owner == proc) {
                    if (prev) prev->next = next;
                    else env->ready_queue_head = next;
                    if (env->ready_queue_tail == cur) env->ready_queue_tail = prev;
                    cur->next = NULL;
                    cur->sched_auth_valid = false;
                    if (cur->state == THREAD_READY || cur->state == THREAD_RUNNING) {
                        scheduler_set_state(cur, THREAD_BLOCKED_AUTH);
                        cur->ticks_remaining = 0;
                    }
                } else {
                    prev = cur;
                }
                cur = next;
            }

            if (cpu->current_thread && cpu->current_thread->owner == proc) {
                cpu->current_thread->sched_auth_valid = false;
                if (cpu->current_thread->state == THREAD_RUNNING) {
                    scheduler_set_state(cpu->current_thread, THREAD_BLOCKED_AUTH);
                    cpu->current_thread->ticks_remaining = 0;
                }
                sched_request_resched(c);
            }
            spinlock_irqrestore(&cpu->runq_lock, flags);
        }
    }

    if (scheduler_get_current() && scheduler_get_current()->owner == proc) {
        schedule();
    }
}

void scheduler_add(thread_t* thread) {
    scheduler_cpu_state_t* local_cpu = sched_cpu_state();
    uint32_t local_cpu_id = sched_current_cpu_id();
    uint32_t target_cpu_id = sched_select_target_cpu(thread);
    scheduler_cpu_state_t* target_cpu = sched_cpu_state_by_id(target_cpu_id);
    scheduler_envelope_t* target_env;
    thread_t* actor = local_cpu->current_thread;
    mode_id_t thread_mode;
    uint64_t flags;

    if (!thread) return;
    thread_mode = sched_thread_mode(thread);

    flags = spinlock_irqsave(&target_cpu->runq_lock);
    sched_refresh_active_mode(target_cpu);
    target_env = sched_envelope(target_cpu, thread_mode);

    if (thread->state == THREAD_ZOMBIE) {
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        kpanic("SCHEDULER: attempted to enqueue zombie TID=%u", thread->tid);
    }

    if (thread->sched_class != SCHED_CLASS_SYSTEM && !thread->sched_auth_required) {
        target_cpu->denied_no_auth++;
        target_env->denied_auth++;
        mode_log_sched_event(thread->owner ? thread->owner->pid : 0,
                             SCHED_EVENT_AUTH_DENY,
                             thread->tid,
                             FATE_RESULT_REJECTED);
        scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    scheduler_refill_budget(thread, __atomic_load_n(&g_sched_global_tick, __ATOMIC_ACQUIRE));
    if (!scheduler_auth_allowed(thread, target_cpu->active_mode)) {
        target_cpu->denied_no_auth++;
        target_env->denied_auth++;
        mode_log_sched_event(thread->owner ? thread->owner->pid : 0,
                             SCHED_EVENT_AUTH_DENY,
                             thread->tid,
                             FATE_RESULT_REJECTED);
        scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    if (!scheduler_lease_valid(thread, target_cpu->active_mode, target_cpu->active_security_epoch)) {
        target_cpu->denied_no_auth++;
        target_env->denied_auth++;
        // Emit markers for matrix
        kprintf("%s tid=%u\n", ENTRY_REJECT_EPOCH, thread->tid);
        scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    if (thread->owner &&
        __atomic_load_n(&thread->owner->remaining_process_budget, __ATOMIC_ACQUIRE) == 0 &&
        thread->sched_class != SCHED_CLASS_SYSTEM) {
        target_cpu->budget_exhaustions++;
        target_env->budget_exhaustions++;
        scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    if (thread_mode != target_cpu->active_mode && thread_mode != MODE_KERNEL) {
        target_cpu->denied_mode_mismatch++;
        target_env->denied_mode++;
        mode_log_sched_event(thread->owner ? thread->owner->pid : 0,
                             SCHED_EVENT_MODE_DENY,
                             thread->tid,
                             FATE_RESULT_REJECTED);
        scheduler_set_state(thread, THREAD_SUSPENDED_MODE);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    if (thread->remaining_thread_budget == 0 && thread->sched_class != SCHED_CLASS_SYSTEM) {
        target_cpu->budget_exhaustions++;
        target_env->budget_exhaustions++;
        scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    if (!scheduler_mode_allowed(thread, actor)) {
        target_cpu->denied_enqueue++;
        scheduler_set_state(thread, THREAD_BLOCKED);
        spinlock_irqrestore(&target_cpu->runq_lock, flags);
        return;
    }

    scheduler_set_state(thread, THREAD_READY);
    thread->last_cpu = target_cpu_id;
    ready_enqueue_locked(target_env, thread);

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
    scheduler_envelope_t* target_env;
    thread_t* actor = local_cpu->current_thread;
    mode_id_t thread_mode;
    uint64_t flags;

    if (!thread) return;
    thread_mode = sched_thread_mode(thread);

    flags = spinlock_irqsave(&target_cpu->runq_lock);
    sched_refresh_active_mode(target_cpu);
    target_env = sched_envelope(target_cpu, thread_mode);

    if (thread->state != THREAD_ZOMBIE) {
        scheduler_refill_budget(thread, __atomic_load_n(&g_sched_global_tick, __ATOMIC_ACQUIRE));

        if (!scheduler_auth_allowed(thread, target_cpu->active_mode)) {
            target_cpu->denied_no_auth++;
            target_env->denied_auth++;
            mode_log_sched_event(thread->owner ? thread->owner->pid : 0,
                                 SCHED_EVENT_AUTH_DENY,
                                 thread->tid,
                                 FATE_RESULT_REJECTED);
            scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
            spinlock_irqrestore(&target_cpu->runq_lock, flags);
            return;
        }

        if (!scheduler_lease_valid(thread, target_cpu->active_mode, target_cpu->active_security_epoch)) {
            target_cpu->denied_no_auth++;
            target_env->denied_auth++;
            if (thread->lease.epoch < target_cpu->active_security_epoch) {
                kprintf("%s tid=%u\n", ENTRY_REJECT_EPOCH, thread->tid);
            } else {
                kprintf("%s tid=%u\n", ENTRY_REJECT_MODE, thread->tid);
            }
            scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
            spinlock_irqrestore(&target_cpu->runq_lock, flags);
            return;
        }

        if (thread->owner &&
            __atomic_load_n(&thread->owner->remaining_process_budget, __ATOMIC_ACQUIRE) == 0 &&
            thread->sched_class != SCHED_CLASS_SYSTEM) {
            target_cpu->budget_exhaustions++;
            target_env->budget_exhaustions++;
            scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
            spinlock_irqrestore(&target_cpu->runq_lock, flags);
            return;
        }

        if (thread_mode != target_cpu->active_mode && thread_mode != MODE_KERNEL) {
            target_cpu->denied_mode_mismatch++;
            target_env->denied_mode++;
            mode_log_sched_event(thread->owner ? thread->owner->pid : 0,
                                 SCHED_EVENT_MODE_DENY,
                                 thread->tid,
                                 FATE_RESULT_REJECTED);
            scheduler_set_state(thread, THREAD_SUSPENDED_MODE);
            spinlock_irqrestore(&target_cpu->runq_lock, flags);
            return;
        }

        if (thread->remaining_thread_budget == 0 && thread->sched_class != SCHED_CLASS_SYSTEM) {
            target_cpu->budget_exhaustions++;
            target_env->budget_exhaustions++;
            scheduler_set_state(thread, THREAD_BLOCKED_AUTH);
            spinlock_irqrestore(&target_cpu->runq_lock, flags);
            return;
        }

        if (!scheduler_mode_allowed(thread, actor)) {
            target_cpu->denied_wake++;
            scheduler_set_state(thread, THREAD_BLOCKED);
            spinlock_irqrestore(&target_cpu->runq_lock, flags);
            return;
        }

        scheduler_set_state(thread, THREAD_READY);
        thread->last_cpu = target_cpu_id;
        ready_enqueue_locked(target_env, thread);
    }

    spinlock_irqrestore(&target_cpu->runq_lock, flags);

    if (target_cpu_id != local_cpu_id) {
        local_cpu->remote_enqueue++;
    }
}

void scheduler_add_zombie(thread_t* thread) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    scheduler_envelope_t* env;
    mode_id_t thread_mode;
    uint64_t flags;

    if (!thread) return;
    thread_mode = sched_thread_mode(thread);

    flags = spinlock_irqsave(&cpu->runq_lock);
    env = sched_envelope(cpu, thread_mode);
    scheduler_set_state(thread, THREAD_ZOMBIE);
    zombie_enqueue_locked(env, thread);
    spinlock_irqrestore(&cpu->runq_lock, flags);
}

void scheduler_reap(void) {
    scheduler_cpu_state_t* cpu = sched_cpu_state();
    scheduler_envelope_t* env;
    thread_t* to_destroy[SCHED_REAP_BUDGET];
    uint32_t destroy_count = 0;
    uint64_t flags = spinlock_irqsave(&cpu->runq_lock);
    sched_refresh_active_mode(cpu);
    env = sched_active_envelope(cpu);

    while (env && destroy_count < SCHED_REAP_BUDGET && env->zombie_queue_head) {
        thread_t* victim = env->zombie_queue_head;
        env->zombie_queue_head = victim->next;
        if (!env->zombie_queue_head) {
            env->zombie_queue_tail = NULL;
        }

        victim->next = NULL;

        if (victim == cpu->current_thread) {
            zombie_enqueue_locked(env, victim);
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
    scheduler_envelope_t* env;
    thread_t* old;
    thread_t* next;
    thread_t* stolen;
    mode_id_t next_mode;
    uint64_t grant;
    uint64_t flags = read_rflags();
    uint32_t local_cpu_id = sched_current_cpu_id();

    cli();
    spinlock_acquire(&cpu->runq_lock);
    sched_refresh_active_mode(cpu);
    env = sched_active_envelope(cpu);

    cpu->schedule_count++;

retry_pick:
    sched_refresh_active_mode(cpu);
    env = sched_active_envelope(cpu);
    old = cpu->current_thread;

    if (!env || !env->ready_queue_head) {
        spinlock_release(&cpu->runq_lock);

        stolen = scheduler_try_same_mode_steal(cpu->active_mode, local_cpu_id);
        if (stolen) {
            uint64_t relock = spinlock_irqsave(&cpu->runq_lock);
            stolen->last_cpu = local_cpu_id;
            ready_enqueue_locked(sched_active_envelope(cpu), stolen);
            cpu->migrations++;
            cpu->remote_enqueue++;
            spinlock_irqrestore(&cpu->runq_lock, relock);
            spinlock_acquire(&cpu->runq_lock);
            goto retry_pick;
        }

        if (old && old->state == THREAD_RUNNING) {
            write_rflags(flags);
            return;
        }

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

    next = scheduler_pick_next_weighted_locked(env);

    while (next && !scheduler_mode_allowed(next, old)) {
        cpu->denied_dispatch++;
        scheduler_set_state(next, THREAD_BLOCKED);
        next = scheduler_pick_next_weighted_locked(env);
    }

    while (next) {
        next_mode = sched_thread_mode(next);
        scheduler_refill_budget(next, __atomic_load_n(&g_sched_global_tick, __ATOMIC_ACQUIRE));

        if (!scheduler_auth_allowed(next, cpu->active_mode)) {
            cpu->denied_no_auth++;
            env->denied_auth++;
            scheduler_set_state(next, THREAD_BLOCKED_AUTH);
        } else if (!scheduler_lease_valid(next, cpu->active_mode, cpu->active_security_epoch)) {
            cpu->denied_no_auth++;
            env->denied_auth++;
            if (next->lease.epoch < cpu->active_security_epoch) {
                kprintf("%s tid=%u\n", ENTRY_REJECT_EPOCH, next->tid);
            } else {
                kprintf("%s tid=%u\n", ENTRY_REJECT_MODE, next->tid);
            }
            scheduler_set_state(next, THREAD_BLOCKED_AUTH);
        } else if (next_mode != cpu->active_mode && next_mode != MODE_KERNEL) {
            cpu->denied_mode_mismatch++;
            env->denied_mode++;
            scheduler_set_state(next, THREAD_SUSPENDED_MODE);
        } else if (next->remaining_thread_budget == 0 && next->sched_class != SCHED_CLASS_SYSTEM) {
            cpu->budget_exhaustions++;
            env->budget_exhaustions++;
            klog_debug("SCHED_BUDGET_EXHAUSTED tid=%u mode=%u", next->tid, next_mode);
            scheduler_set_state(next, THREAD_BLOCKED_AUTH);
        } else if (next->owner &&
                   __atomic_load_n(&next->owner->remaining_process_budget, __ATOMIC_ACQUIRE) == 0 &&
                   next->sched_class != SCHED_CLASS_SYSTEM) {
            cpu->budget_exhaustions++;
            env->budget_exhaustions++;
            klog_debug("SCHED_BUDGET_EXHAUSTED tid=%u mode=%u", next->tid, next_mode);
            scheduler_set_state(next, THREAD_BLOCKED_AUTH);
        } else {
            break;
        }

        next = scheduler_pick_next_weighted_locked(env);
    }

    if (!next) {
        goto retry_pick;
    }

    if (old && old->state == THREAD_RUNNING && old != next) {
        scheduler_set_state(old, THREAD_READY);
        if (old->sched_tokens > 0) {
            ready_enqueue_head_locked(sched_envelope(cpu, sched_thread_mode(old)), old);
        } else {
            ready_enqueue_locked(sched_envelope(cpu, sched_thread_mode(old)), old);
        }
    }

    scheduler_set_state(next, THREAD_RUNNING);
    grant = DEFAULT_QUANTUM;
    if (next->max_slice > 0 && grant > next->max_slice) grant = next->max_slice;
    if (next->sched_class != SCHED_CLASS_SYSTEM) {
        if (next->remaining_thread_budget < grant) grant = next->remaining_thread_budget;
        if (next->owner) {
            grant = process_budget_try_consume(next->owner, grant);
        }
        if (grant == 0) {
            mode_log_sched_event(next->owner ? next->owner->pid : 0,
                                 SCHED_EVENT_BUDGET_EXHAUST,
                                 0,
                                 FATE_RESULT_REJECTED);
            scheduler_set_state(next, THREAD_BLOCKED_AUTH);
            goto retry_pick;
        }
        next->remaining_thread_budget -= grant;
    }
    next->ticks_remaining = (uint32_t)grant;
    next->last_cpu = cpu_get_id();
    cpu->current_thread = next;
    env->dispatch_count++;

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
        if (next->owner->mode != cpu->active_mode && next->owner->mode != MODE_KERNEL) {
            kpanic("SCHED_PCID_VIOLATION cpu=%u tid=%u mode=%u active=%u",
                   sched_current_cpu_id(), next->tid, next->owner->mode, cpu->active_mode);
        }
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
    scheduler_envelope_t* env;
    thread_t* current = cpu->current_thread;
    bool force_resched = false;
    uint32_t cpu_id = sched_current_cpu_id();

    if (!current) return;
    cpu->tick_count++;
    __atomic_add_fetch(&g_sched_global_tick, 1, __ATOMIC_ACQ_REL);
    sched_refresh_active_mode(cpu);
    env = sched_active_envelope(cpu);
    if (sched_consume_resched_request(cpu_id)) {
        force_resched = true;
    }

    if (current->sched_class != SCHED_CLASS_SYSTEM && current->sched_auth_required) {
        if (!current->sched_auth_valid ||
            !current->owner ||
            !current->owner->sched_auth_root_valid ||
            __atomic_load_n(&current->owner->remaining_process_budget, __ATOMIC_ACQUIRE) == 0) {
            current->ticks_remaining = 0;
            force_resched = true;
            mode_log_sched_event(current->owner ? current->owner->pid : 0,
                                 SCHED_EVENT_AUTH_DENY,
                                 current->tid,
                                 FATE_RESULT_REJECTED);
        }
    }

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
                      env ? queue_depth(env->ready_queue_head) : 0,
                      env ? queue_depth(env->zombie_queue_head) : 0);
        }
    }

    if (current->ticks_remaining == 0 || force_resched) {
        schedule();
    }
}

bool scheduler_self_test_deterministic_rr(void) {
    scheduler_envelope_t env;
    thread_t a, b;
    thread_t* p;
    int seq[6] = {0};

    memset(&env, 0, sizeof(env));
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.tid = 101; a.state = THREAD_READY; a.sched_weight = 1; a.sched_tokens = 0;
    b.tid = 102; b.state = THREAD_READY; b.sched_weight = 1; b.sched_tokens = 0;
    ready_enqueue_locked(&env, &a);
    ready_enqueue_locked(&env, &b);

    for (int i = 0; i < 4; i++) {
        p = scheduler_pick_next_weighted_locked(&env);
        if (!p) return false;
        seq[i] = (int)p->tid;
        if (p->sched_tokens > 0) ready_enqueue_head_locked(&env, p);
        else ready_enqueue_locked(&env, p);
    }
    if (!(seq[0] == 101 && seq[1] == 102 && seq[2] == 101 && seq[3] == 102)) return false;

    memset(&env, 0, sizeof(env));
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.tid = 201; a.state = THREAD_READY; a.sched_weight = 2; a.sched_tokens = 0;
    b.tid = 202; b.state = THREAD_READY; b.sched_weight = 1; b.sched_tokens = 0;
    ready_enqueue_locked(&env, &a);
    ready_enqueue_locked(&env, &b);

    for (int i = 0; i < 3; i++) {
        p = scheduler_pick_next_weighted_locked(&env);
        if (!p) return false;
        seq[i] = (int)p->tid;
        if (p->sched_tokens > 0) ready_enqueue_head_locked(&env, p);
        else ready_enqueue_locked(&env, p);
    }
    return (seq[0] == 201 && seq[1] == 201 && seq[2] == 202);
}

bool scheduler_self_test_atomic_budget(void) {
    process_t proc;
    uint64_t c1, c2, c3;
    memset(&proc, 0, sizeof(proc));
    proc.remaining_process_budget = 3;
    proc.max_total_budget = 3;

    c1 = process_budget_try_consume(&proc, 2);
    c2 = process_budget_try_consume(&proc, 2);
    c3 = process_budget_try_consume(&proc, 1);

    if (c1 != 2) return false;
    if (c2 != 1) return false;
    if (c3 != 0) return false;
    return __atomic_load_n(&proc.remaining_process_budget, __ATOMIC_ACQUIRE) == 0;
}
