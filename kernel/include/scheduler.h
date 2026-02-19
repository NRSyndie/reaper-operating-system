#ifndef REAPER_SCHEDULER_H
#define REAPER_SCHEDULER_H

#include "thread.h"

#define SCHED_MAX_CPUS 4
#define DEFAULT_QUANTUM 10
#define SCHED_REAP_BUDGET 16

typedef enum {
    SCHED_CLASS_NORMAL = 0,
    SCHED_CLASS_SYSTEM = 1,
} sched_class_t;

typedef struct {
    uint64_t schedule_count;
    uint64_t switch_count;
    uint64_t remote_enqueue;
    uint64_t migrations;
    uint64_t denied_enqueue;
    uint64_t denied_wake;
    uint64_t denied_dispatch;
    uint32_t cpu_id;
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

/**
 * scheduler_get_current: Returns the soul currently inhabiting the CPU.
 */
thread_t* scheduler_get_current(void);

#endif /* REAPER_SCHEDULER_H */
