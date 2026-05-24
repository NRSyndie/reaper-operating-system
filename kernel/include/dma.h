#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t iova_t;

void dma_init();
iova_t dma_map(uint16_t device_id, uint64_t phys_addr, size_t size);
void dma_unmap(uint16_t device_id, iova_t iova, size_t size);

#endif
