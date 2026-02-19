#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Initialize the kernel heap (kmalloc buckets).
 */
void kmalloc_init(void);

/**
 * @brief Allocate memory from the kernel heap.
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 */
void* kmalloc(size_t size);

/**
 * @brief Free memory back to the kernel heap.
 * @param ptr Pointer to the memory to free.
 */
void kfree(void* ptr);

/**
 * @brief Allocate memory from the kernel heap and zero it.
 */
void* kzalloc(size_t size);

#endif
