#include "include/dma.h"
#include "include/klog.h"
#include "include/kmalloc.h"

#define IOVA_START 0x10000000
#define IOVA_END   0x80000000

static iova_t next_iova = IOVA_START;

void dma_init() {
    klog(0, "DMA: Initializing IOVA manager...\n");
}

iova_t dma_map(uint16_t device_id, uint64_t phys_addr, size_t size) {
    iova_t iova = next_iova;
    next_iova += size;
    
    klog(0, "DMA: Mapping device 0x%x, phys 0x%lx, size 0x%lx -> iova 0x%lx\n", 
         device_id, phys_addr, size, iova);
    
    // IOMMU page table update logic goes here.
    return iova;
}

void dma_unmap(uint16_t device_id, iova_t iova, size_t size) {
    klog(0, "DMA: Unmapping device 0x%x, iova 0x%lx, size 0x%lx\n", 
         device_id, iova, size);
    
    // IOMMU page table update logic goes here.
}
