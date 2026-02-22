#ifndef REAPER_THREAD_H
#define REAPER_THREAD_H

#include <stdint.h>
#include <stdbool.h>
#include "process.h"

typedef enum {
    THREAD_READY,    /* In ready queue */
    THREAD_RUNNING,  /* Currently executing */
    THREAD_BLOCKED,  /* Waiting for event */
    THREAD_BLOCKED_AUTH, /* Blocked due to missing/invalid scheduling authority */
    THREAD_SUSPENDED_MODE, /* Blocked because its mode envelope is inactive */
    THREAD_ZOMBIE    /* Exited, awaiting cleanup */
} thread_state_t;

/**
 * saved_context_t
 * The minimal register set needed to preserve a thread's fate.
 * Registers like RAX, RCX, RDX are scratch and not saved during switch
 * unless we are doing a full interrupt-style save.
 */
typedef struct {
    uint64_t r15, r14, r13, r12, rbp, rbx, rip, rsp;
} __attribute__((packed)) saved_context_t;

/**
 * thread_t (The Soul)
 * A single execution stream within a World.
 */
typedef struct thread {
    uint64_t kernel_stack_top; /* Pointer to the top of the private K-stack */
    uint64_t rsp;              /* Saved stack pointer (points to saved_context_t) */
    uint64_t stack_canary;     /* Magic value to detect stack overflow */
    
    thread_state_t state;
    process_t* owner;          /* The World this soul belongs to */
    
    uint32_t tid;              /* Thread ID */
    uint32_t ticks_remaining;  /* Current quantum */
    uint32_t last_cpu;         /* Last CPU that executed this thread */
    uint8_t sched_class;       /* scheduler class (sched_class_t) */

    /* Envelope scheduler budget contract */
    uint64_t remaining_thread_budget;
    uint64_t max_accumulated;
    uint64_t last_thread_refill;
    uint32_t max_slice;
    uint32_t refill_period_ticks;
    uint16_t sched_auth_slot;
    uint8_t sched_weight;
    uint8_t sched_tokens;
    uint8_t sched_auth_mode;
    bool sched_auth_required;
    bool sched_auth_valid;
    
    struct thread* next;       /* Intrusive list pointer for scheduler */
    struct thread* wait_next;  /* Intrusive list pointer for IPC wait queues */

    /* IPC State */
    uint64_t ipc_payload[4];   /* Register-only data transfer (a0, a1, a2, a3) */

    /* Extended State (SSE/FPU/XSAVE) */
    void* extended_state;      /* Pointer to a 64-byte aligned buffer (usually 1 frame) */
} thread_t;

/**
 * thread_create: Spawn a new Soul within a World.
 */
thread_t* thread_create(process_t* owner, void (*entry)(void));

/**
 * thread_destroy: Annihilate a Soul.
 */
void thread_destroy(thread_t* thread);

/**
 * thread_yield: Voluntarily give up the CPU.
 */
void thread_yield(void);

/**
 * thread_exit: Terminate the current thread (enters ZOMBIE state).
 */
void thread_exit(void);

#endif /* REAPER_THREAD_H */
