#ifndef IOMMU_H
#define IOMMU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IOMMU_MAX_UNITS 16
#define IOMMU_MAX_SCOPES 64

typedef enum {
    IOMMU_STATE_UNINITIALIZED = 0,
    IOMMU_STATE_INVENTORIED,
    IOMMU_STATE_DEGRADED,
    IOMMU_STATE_UNAVAILABLE,
    IOMMU_STATE_ENFORCED
} iommu_state_t;

typedef enum {
    IOMMU_DEGRADED_NONE = 0,
    IOMMU_DEGRADED_NO_DMAR,
    IOMMU_DEGRADED_INVALID_DMAR,
    IOMMU_DEGRADED_UNSUPPORTED_HARDWARE,
    IOMMU_DEGRADED_AMBIGUOUS_TOPOLOGY,
    IOMMU_DEGRADED_UNIMPLEMENTED_ENABLE
} iommu_degraded_reason_t;

typedef struct {
    uint8_t type;
    uint8_t enumeration_id;
    uint8_t start_bus;
    uint8_t path_length;
    /* PCI scope paths are encoded as device/function byte pairs, so 16 bytes covers the maximum 8-hop depth. */
    uint8_t path[16];
} iommu_device_scope_t;

typedef struct {
    uint16_t segment;
    uint64_t register_base;
    uint8_t flags;
    uint32_t scope_index;
    uint32_t scope_count;
} iommu_unit_t;

typedef struct {
    iommu_state_t state;
    iommu_degraded_reason_t degraded_reason;
    uint8_t host_address_width;
    uint8_t flags;
    iommu_unit_t units[IOMMU_MAX_UNITS];
    size_t unit_count;
    iommu_device_scope_t scopes[IOMMU_MAX_SCOPES];
    size_t scope_count;
} iommu_inventory_t;

void dmar_init(void);
void iommu_init(void);
/* Always writes deterministic state to out; before iommu_init() that state is IOMMU_STATE_UNINITIALIZED. */
bool iommu_get_inventory(iommu_inventory_t *out);
iommu_state_t iommu_get_state(void);
iommu_degraded_reason_t iommu_get_degraded_reason(void);
bool iommu_self_test(void);

#endif
