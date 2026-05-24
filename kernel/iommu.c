#include "include/iommu.h"
#include "include/acpi.h"
#include "include/audit.h"
#include "include/console.h"
#include "include/utils.h"

static iommu_inventory_t g_iommu_inventory;
static bool g_iommu_initialized;
static bool g_iommu_audit_emitted;

/*
 * IOMMU audit target encoding:
 *   bits 63:48 = PCI segment
 *   bits 47:0  = unit index or degraded reason / reject detail
 * This keeps Sentinel-side decoding stable without requiring table lookups.
 */
#define IOMMU_AUDIT_TARGET(segment, detail) \
    ((((uint64_t)(segment) & 0xFFFFULL) << 48) | ((uint64_t)(detail) & 0x0000FFFFFFFFFFFFULL))

static void iommu_set_state(iommu_state_t state, iommu_degraded_reason_t reason) {
    g_iommu_inventory.state = state;
    g_iommu_inventory.degraded_reason = reason;
}

static bool iommu_scope_eq(const iommu_device_scope_t *lhs, const iommu_device_scope_t *rhs) {
    if (!lhs || !rhs) {
        return false;
    }
    if (lhs->type != rhs->type ||
        lhs->enumeration_id != rhs->enumeration_id ||
        lhs->start_bus != rhs->start_bus ||
        lhs->path_length != rhs->path_length) {
        return false;
    }

    for (size_t i = 0; i < lhs->path_length; i++) {
        if (lhs->path[i] != rhs->path[i]) {
            return false;
        }
    }
    return true;
}

static bool iommu_validate_inventory(void) {
    if (g_iommu_inventory.unit_count == 0) {
        iommu_set_state(IOMMU_STATE_DEGRADED, IOMMU_DEGRADED_INVALID_DMAR);
        kprintf("[IOMMU-FAIL] DMAR invalid\n");
        return false;
    }

    for (size_t i = 0; i < g_iommu_inventory.unit_count; i++) {
        iommu_unit_t *unit = &g_iommu_inventory.units[i];
        if (unit->register_base == 0) {
            iommu_set_state(IOMMU_STATE_DEGRADED, IOMMU_DEGRADED_INVALID_DMAR);
            kprintf("[IOMMU-FAIL] DMAR invalid\n");
            return false;
        }
        if ((size_t)unit->scope_index + (size_t)unit->scope_count > g_iommu_inventory.scope_count) {
            iommu_set_state(IOMMU_STATE_DEGRADED, IOMMU_DEGRADED_INVALID_DMAR);
            kprintf("[IOMMU-FAIL] DMAR invalid\n");
            return false;
        }

        for (size_t j = i + 1; j < g_iommu_inventory.unit_count; j++) {
            iommu_unit_t *other = &g_iommu_inventory.units[j];
            if (unit->segment == other->segment && unit->register_base == other->register_base) {
                iommu_set_state(IOMMU_STATE_DEGRADED, IOMMU_DEGRADED_AMBIGUOUS_TOPOLOGY);
                kprintf("[IOMMU-FAIL] ambiguous topology\n");
                return false;
            }
        }
    }

    for (size_t i = 0; i < g_iommu_inventory.unit_count; i++) {
        iommu_unit_t *unit = &g_iommu_inventory.units[i];
        for (size_t j = i + 1; j < g_iommu_inventory.unit_count; j++) {
            iommu_unit_t *other = &g_iommu_inventory.units[j];
            for (size_t a = 0; a < unit->scope_count; a++) {
                iommu_device_scope_t *lhs = &g_iommu_inventory.scopes[unit->scope_index + a];
                for (size_t b = 0; b < other->scope_count; b++) {
                    iommu_device_scope_t *rhs = &g_iommu_inventory.scopes[other->scope_index + b];
                    if (iommu_scope_eq(lhs, rhs)) {
                        iommu_set_state(IOMMU_STATE_DEGRADED, IOMMU_DEGRADED_AMBIGUOUS_TOPOLOGY);
                        kprintf("[IOMMU-FAIL] ambiguous topology\n");
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

static void iommu_emit_audit_once(void) {
    audit_meta_t meta = {0};

    if (g_iommu_audit_emitted) {
        return;
    }
    g_iommu_audit_emitted = true;

    if (g_iommu_inventory.state == IOMMU_STATE_INVENTORIED) {
        audit_strike(AUDIT_EVENT_IOMMU_INVENTORY,
                     AUDIT_RESULT_OK,
                     IOMMU_AUDIT_TARGET(0, g_iommu_inventory.unit_count),
                     meta);
        for (size_t i = 0; i < g_iommu_inventory.unit_count; i++) {
            iommu_unit_t *unit = &g_iommu_inventory.units[i];
            audit_strike(AUDIT_EVENT_IOMMU_UNIT_DISCOVERED,
                         AUDIT_RESULT_OK,
                         IOMMU_AUDIT_TARGET(unit->segment, i),
                         meta);
        }
        return;
    }

    if (g_iommu_inventory.state == IOMMU_STATE_DEGRADED ||
        g_iommu_inventory.state == IOMMU_STATE_UNAVAILABLE) {
        audit_strike(AUDIT_EVENT_IOMMU_DEGRADED,
                     AUDIT_RESULT_DENIED,
                     IOMMU_AUDIT_TARGET(0, g_iommu_inventory.degraded_reason),
                     meta);
        if (g_iommu_inventory.degraded_reason == IOMMU_DEGRADED_AMBIGUOUS_TOPOLOGY ||
            g_iommu_inventory.degraded_reason == IOMMU_DEGRADED_INVALID_DMAR) {
            audit_strike(AUDIT_EVENT_IOMMU_TOPOLOGY_REJECT,
                         AUDIT_RESULT_ERROR,
                         IOMMU_AUDIT_TARGET(0, g_iommu_inventory.degraded_reason),
                         meta);
        }
    }
}

void iommu_init(void) {
    acpi_dmar_info_t dmar_info;
    bool dmar_present;

    fast_zero(&g_iommu_inventory, sizeof(g_iommu_inventory));
    g_iommu_audit_emitted = false;
    iommu_set_state(IOMMU_STATE_UNINITIALIZED, IOMMU_DEGRADED_NONE);

    kprintf("[IOMMU] Inventory start\n");

    dmar_present = acpi_find_table("DMAR") != NULL;
    if (!dmar_present) {
        iommu_set_state(IOMMU_STATE_UNAVAILABLE, IOMMU_DEGRADED_NO_DMAR);
        kprintf("[IOMMU-FAIL] DMAR missing\n");
        kprintf("[IOMMU] State: UNAVAILABLE reason=%u\n", (unsigned)g_iommu_inventory.degraded_reason);
        g_iommu_initialized = true;
        return;
    }

    if (!acpi_get_dmar_info(&dmar_info)) {
        iommu_set_state(IOMMU_STATE_DEGRADED, IOMMU_DEGRADED_INVALID_DMAR);
        kprintf("[IOMMU-FAIL] DMAR invalid\n");
        kprintf("[IOMMU] State: DEGRADED reason=%u\n", (unsigned)g_iommu_inventory.degraded_reason);
        g_iommu_initialized = true;
        return;
    }

    g_iommu_inventory.host_address_width = dmar_info.host_address_width;
    g_iommu_inventory.flags = dmar_info.flags;
    g_iommu_inventory.unit_count = dmar_info.unit_count;
    g_iommu_inventory.scope_count = dmar_info.scope_count;
    memcpy(g_iommu_inventory.units, dmar_info.units, sizeof(dmar_info.units));
    memcpy(g_iommu_inventory.scopes, dmar_info.scopes, sizeof(dmar_info.scopes));

    kprintf("[IOMMU] DMAR units=%u scopes=%u haw=%u\n",
            (unsigned)g_iommu_inventory.unit_count,
            (unsigned)g_iommu_inventory.scope_count,
            (unsigned)g_iommu_inventory.host_address_width);

    if (!iommu_validate_inventory()) {
        kprintf("[IOMMU] State: DEGRADED reason=%u\n", (unsigned)g_iommu_inventory.degraded_reason);
        g_iommu_initialized = true;
        return;
    }

    for (size_t i = 0; i < g_iommu_inventory.unit_count; i++) {
        iommu_unit_t *unit = &g_iommu_inventory.units[i];
        kprintf("[IOMMU] Unit %u segment=%u base=0x%lx scopes=%u\n",
                (unsigned)i,
                (unsigned)unit->segment,
                unit->register_base,
                (unsigned)unit->scope_count);
    }

    iommu_set_state(IOMMU_STATE_INVENTORIED, IOMMU_DEGRADED_NONE);
    kprintf("[IOMMU] State: INVENTORIED\n");
    g_iommu_initialized = true;
}

bool iommu_get_inventory(iommu_inventory_t *out) {
    if (!out) {
        return false;
    }

    fast_zero(out, sizeof(*out));
    memcpy(out, &g_iommu_inventory, sizeof(g_iommu_inventory));
    return true;
}

iommu_state_t iommu_get_state(void) {
    return g_iommu_inventory.state;
}

iommu_degraded_reason_t iommu_get_degraded_reason(void) {
    return g_iommu_inventory.degraded_reason;
}

bool iommu_self_test(void) {
    iommu_inventory_t snapshot;

    if (!iommu_get_inventory(&snapshot)) {
        kprintf("[IOMMU-FAIL] inventory unavailable\n");
        return false;
    }

    if (!g_iommu_initialized || snapshot.state == IOMMU_STATE_UNINITIALIZED) {
        kprintf("[IOMMU-FAIL] inventory unavailable\n");
        return false;
    }

    iommu_emit_audit_once();

    if (snapshot.state == IOMMU_STATE_INVENTORIED) {
        if (snapshot.unit_count == 0) {
            kprintf("[IOMMU-FAIL] inventory unavailable\n");
            return false;
        }
        kprintf("[TEST] IOMMU Inventory Contract: SUCCESS.\n");
        kprintf("[TEST] IOMMU Degraded Policy Contract: SUCCESS.\n");
        return true;
    }

    if (snapshot.state == IOMMU_STATE_DEGRADED || snapshot.state == IOMMU_STATE_UNAVAILABLE) {
        if (snapshot.degraded_reason == IOMMU_DEGRADED_NONE) {
            kprintf("[IOMMU-FAIL] inventory unavailable\n");
            return false;
        }
        kprintf("[TEST] IOMMU Inventory Contract: SUCCESS.\n");
        kprintf("[TEST] IOMMU Degraded Policy Contract: SUCCESS.\n");
        return true;
    }

    kprintf("[IOMMU-FAIL] inventory unavailable\n");
    return false;
}
