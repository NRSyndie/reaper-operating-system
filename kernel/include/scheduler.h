#ifndef REAPER_SCHEDULER_H
#define REAPER_SCHEDULER_H

#include "thread.h"
#include "mode.h"

#define SCHED_MAX_CPUS 4
#define DEFAULT_QUANTUM 10
#define SCHED_REAP_BUDGET 16
#define SCHED_MIN_REFILL_PERIOD 1
#define SCHED_DEFAULT_MAX_ACCUMULATED (DEFAULT_QUANTUM * 4U)

typedef enum {
    SCHED_CLASS_NORMAL = 0,
    SCHED_CLASS_SYSTEM = 1,
} sched_class_t;

typedef enum {
    SCHED_AUTH_ROOT = 1,
    SCHED_AUTH_THREAD = 2
} sched_auth_kind_t;

typedef struct sched_auth_obj {
    uint32_t magic;
    uint8_t kind;
    uint8_t mode_binding;
    uint16_t reserved;

    /* Root fields */
    uint64_t max_total_budget;
    uint64_t refill_period_ticks;
    uint64_t max_accumulated;

    /* Derived thread fields */
    uint32_t max_slice;
    uint32_t weight;
    uint64_t local_max_accumulated;
    struct sched_auth_obj* root;
    process_t* owner_process;
    struct thread* bound_thread;
} sched_auth_obj_t;

typedef struct {
    mode_id_t mode;
    thread_t* ready_queue_head;
    thread_t* ready_queue_tail;
    thread_t* zombie_queue_head;
    thread_t* zombie_queue_tail;
    uint64_t dispatch_count;
    uint64_t denied_mode;
    uint64_t denied_auth;
    uint64_t budget_exhaustions;
} scheduler_envelope_t;

typedef struct {
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
    uint64_t active_security_epoch;
    uint32_t cpu_id;
    uint32_t active_mode;
    uint32_t ready_depth;
    uint32_t zombie_depth;
} sched_metrics_t;

/**
 * scheduler_init: Initialize the Law of Time.
 */
void scheduler_init(void);

/**
 * timer_init: Configure the system pulse.
 */
void timer_init(uint32_t hz);

/**
 * timer_handler: The periodic pulse logic.
 */
void timer_handler(void);

/**
 * scheduler_add: Add a Soul to the Ready Queue.
 */
void scheduler_add(thread_t* thread);

/**
 * scheduler_block: Move a Soul to blocked state.
 */
void scheduler_block(thread_t* thread);

/**
 * scheduler_wake: Transition a blocked soul to ready and enqueue.
 */
void scheduler_wake(thread_t* thread);

/**
 * scheduler_add_zombie: Add a Soul to the Reaper Queue.
 */
void scheduler_add_zombie(thread_t* thread);

/**
 * scheduler_reap: Process the Reaper Queue and destroy dead souls.
 */
void scheduler_reap(void);

/**
 * schedule: The core mechanism for choosing the next thread.
 */
void schedule(void);

/**
 * scheduler_set_state: Validated state transition helper.
 */
void scheduler_set_state(thread_t* thread, thread_state_t new_state);

/**
 * scheduler_get_metrics: Snapshot per-CPU scheduler metrics.
 */
void scheduler_get_metrics(sched_metrics_t* out);
uint64_t scheduler_get_global_tick(void);

/**
 * scheduler_get_current: Returns the soul currently inhabiting the CPU.
 */
thread_t* scheduler_get_current(void);

/**
 * scheduler_on_mode_transition: Notify scheduler of global mode transition.
 * This is lock-free and safe to call while mode transition lock is held.
 */
void scheduler_on_mode_transition(mode_id_t from_mode, mode_id_t to_mode, uint64_t security_epoch);

int scheduler_mint_root_auth(process_t* proc,
                             uint32_t dst_slot,
                             mode_id_t mode_binding,
                             uint64_t max_total_budget,
                             uint64_t refill_period_ticks,
                             uint64_t max_accumulated);
int scheduler_derive_thread_auth(process_t* proc,
                                 thread_t* thread,
                                 uint32_t root_slot,
                                 uint32_t dst_slot,
                                 uint32_t max_slice,
                                 uint32_t weight,
                                 uint64_t local_max_accumulated);
void scheduler_revoke_thread_immediate(thread_t* thread);
void scheduler_revoke_process_immediate(process_t* proc);
bool scheduler_self_test_deterministic_rr(void);
bool scheduler_self_test_atomic_budget(void);

#endif /* REAPER_SCHEDULER_H */
