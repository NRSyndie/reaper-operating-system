#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>
#include "cpu.h"

typedef volatile uint32_t spinlock_t;

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

#endif

