#ifndef REAPER_SCHEDULER_H
#define REAPER_SCHEDULER_H

#include "thread.h"

#define DEFAULT_QUANTUM 10

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
 * scheduler_get_current: Returns the soul currently inhabiting the CPU.
 */
thread_t* scheduler_get_current(void);

#endif /* REAPER_SCHEDULER_H */
