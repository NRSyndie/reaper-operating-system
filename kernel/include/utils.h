#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>
#include "cpu.h"

typedef volatile uint32_t spinlock_t;
typedef volatile uint32_t rwlock_t;

#define RWLOCK_WRITER_BIT (1U << 31)
#define RWLOCK_READER_MASK (RWLOCK_WRITER_BIT - 1U)

typedef struct {
    volatile uint32_t seq;
    spinlock_t writer_lock;
} seqlock_t;

typedef struct {
    volatile uint32_t readers;
    volatile uint32_t epoch;
    spinlock_t writer_lock;
} rcu_t;

static inline void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

static inline void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static inline void fast_zero(void* ptr, size_t size) {
    uint64_t* p = (uint64_t*)ptr;
    for (size_t i = 0; i < size / 8; i++) {
        p[i] = 0;
    }
}

/**
 * hyper_scrub: Hardware-accelerated memory purging.
 * Uses 'rep stosq' which is optimized on modern CPUs (ERMS).
 */
static inline void hyper_scrub(void* ptr, size_t size) {
    __asm__ volatile (
        "rep stosq"
        : "+D"(ptr), "+c"(size)
        : "a"(0)
        : "memory"
    );
}

/**
 * selective_cache_flush: Flush a range from the CPU caches.
 */
static inline void selective_cache_flush(void* ptr, size_t size) {
    uintptr_t start = (uintptr_t)ptr;
    for (uintptr_t addr = start; addr < start + size; addr += 64) {
        __asm__ volatile ("clflushopt (%0)" : : "r"(addr) : "memory");
    }
    __asm__ volatile ("sfence" : : : "memory");
}

/**
 * spinlock_irqsave: Disables interrupts and acquires the lock.
 * Returns the previous RFLAGS state.
 */
static inline uint64_t spinlock_irqsave(spinlock_t* lock) {
    uint64_t flags = read_rflags();
    cli();
    while (__sync_lock_test_and_set(lock, 1)) {
        while (*lock) {
            __asm__ volatile ("pause");
        }
    }
    return flags;
}

/**
 * spinlock_irqrestore: Releases the lock and restores previous interrupt state.
 */
static inline void spinlock_irqrestore(spinlock_t* lock, uint64_t flags) {
    __sync_lock_release(lock);
    write_rflags(flags);
}

// Simple atomic spinlock (Legacy compatibility for non-IRQ paths)
static inline void spinlock_acquire(volatile uint32_t* lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        while (*lock) {
            __asm__ volatile ("pause");
        }
    }
}

static inline void spinlock_release(volatile uint32_t* lock) {
    __sync_lock_release(lock);
}

static inline void barrier_compiler(void) {
    __asm__ volatile ("" : : : "memory");
}

static inline void barrier_mb(void) {
    __asm__ volatile ("mfence" : : : "memory");
}

static inline void barrier_rmb(void) {
    __asm__ volatile ("lfence" : : : "memory");
}

static inline void barrier_wmb(void) {
    __asm__ volatile ("sfence" : : : "memory");
}

static inline void rwlock_init(rwlock_t* lock) {
    if (!lock) return;
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

static inline void rwlock_read_lock(rwlock_t* lock) {
    uint32_t cur;
    if (!lock) return;
    for (;;) {
        cur = __atomic_load_n(lock, __ATOMIC_ACQUIRE);
        if (cur & RWLOCK_WRITER_BIT) {
            __asm__ volatile ("pause");
            continue;
        }
        if (__atomic_compare_exchange_n(lock,
                                        &cur,
                                        cur + 1U,
                                        false,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED)) {
            return;
        }
        __asm__ volatile ("pause");
    }
}

static inline void rwlock_read_unlock(rwlock_t* lock) {
    if (!lock) return;
    __atomic_fetch_sub(lock, 1U, __ATOMIC_ACQ_REL);
}

static inline void rwlock_write_lock(rwlock_t* lock) {
    uint32_t expected;
    if (!lock) return;
    for (;;) {
        expected = 0;
        if (__atomic_compare_exchange_n(lock,
                                        &expected,
                                        RWLOCK_WRITER_BIT,
                                        false,
                                        __ATOMIC_ACQ_REL,
                                        __ATOMIC_RELAXED)) {
            return;
        }
        __asm__ volatile ("pause");
    }
}

static inline void rwlock_write_unlock(rwlock_t* lock) {
    if (!lock) return;
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

static inline void seqlock_init(seqlock_t* lock) {
    if (!lock) return;
    __atomic_store_n(&lock->seq, 0, __ATOMIC_RELEASE);
    lock->writer_lock = 0;
}

static inline uint32_t seqlock_read_begin(seqlock_t* lock) {
    uint32_t seq;
    if (!lock) return 0;
    for (;;) {
        seq = __atomic_load_n(&lock->seq, __ATOMIC_ACQUIRE);
        if ((seq & 1U) == 0) {
            return seq;
        }
        __asm__ volatile ("pause");
    }
}

static inline bool seqlock_read_retry(seqlock_t* lock, uint32_t start_seq) {
    uint32_t end_seq;
    if (!lock) return true;
    barrier_compiler();
    end_seq = __atomic_load_n(&lock->seq, __ATOMIC_ACQUIRE);
    return (end_seq != start_seq) || ((end_seq & 1U) != 0);
}

static inline void seqlock_write_begin(seqlock_t* lock) {
    if (!lock) return;
    spinlock_acquire(&lock->writer_lock);
    __atomic_fetch_add(&lock->seq, 1U, __ATOMIC_ACQ_REL);
    barrier_wmb();
}

static inline void seqlock_write_end(seqlock_t* lock) {
    if (!lock) return;
    barrier_wmb();
    __atomic_fetch_add(&lock->seq, 1U, __ATOMIC_RELEASE);
    spinlock_release(&lock->writer_lock);
}

static inline void rcu_init(rcu_t* rcu) {
    if (!rcu) return;
    __atomic_store_n(&rcu->readers, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&rcu->epoch, 0, __ATOMIC_RELEASE);
    rcu->writer_lock = 0;
}

static inline void rcu_read_lock(rcu_t* rcu) {
    if (!rcu) return;
    __atomic_fetch_add(&rcu->readers, 1U, __ATOMIC_ACQ_REL);
    barrier_compiler();
}

static inline void rcu_read_unlock(rcu_t* rcu) {
    if (!rcu) return;
    barrier_compiler();
    __atomic_fetch_sub(&rcu->readers, 1U, __ATOMIC_ACQ_REL);
}

static inline void rcu_synchronize(rcu_t* rcu) {
    if (!rcu) return;
    spinlock_acquire(&rcu->writer_lock);
    __atomic_fetch_add(&rcu->epoch, 1U, __ATOMIC_ACQ_REL);
    while (__atomic_load_n(&rcu->readers, __ATOMIC_ACQUIRE) != 0U) {
        __asm__ volatile ("pause");
    }
    __atomic_fetch_add(&rcu->epoch, 1U, __ATOMIC_RELEASE);
    spinlock_release(&rcu->writer_lock);
}

#endif
