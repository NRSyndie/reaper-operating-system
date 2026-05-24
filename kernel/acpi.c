#include "include/acpi.h"
#include "include/console.h"
#include "include/limine.h"
#include "include/utils.h"

extern struct limine_rsdp_request rsdp_request;

#define ACPI_MAX_CPUS         256
#define ACPI_MAX_IOAPICS      16
#define ACPI_MAX_MCFG_ENTRIES 16

typedef struct {
    uint8_t apic_id;
    uint8_t acpi_uid;
    uint32_t flags;
} acpi_cpu_info_t;

typedef struct {
    uint8_t ioapic_id;
    uint32_t ioapic_address;
    uint32_t gsi_base;
} acpi_ioapic_info_t;

typedef struct {
    uint64_t lapic_address;
    uint32_t flags;
    acpi_cpu_info_t cpus[ACPI_MAX_CPUS];
    size_t cpu_count;
    acpi_ioapic_info_t ioapics[ACPI_MAX_IOAPICS];
    size_t ioapic_count;
} acpi_madt_info_t;

typedef struct {
    uint16_t sci_int;
    uint16_t iapc_boot_arch;
    uint32_t flags;
    uint32_t dsdt;
} acpi_fadt_info_t;

typedef struct {
    uint64_t address;
    uint32_t event_timer_block_id;
    uint16_t minimum_tick;
    uint8_t number;
} acpi_hpet_info_t;

typedef struct {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t start_bus;
    uint8_t end_bus;
} acpi_mcfg_info_t;

static void *g_rsdp;
static acpi_sdt_header_t *g_root_sdt;
static bool g_use_xsdt;
static bool g_logged_aml_deferred;
static bool g_dmar_present;
static bool g_dmar_parse_valid;

static acpi_madt_info_t g_madt_info;
static acpi_fadt_info_t g_fadt_info;
static acpi_hpet_info_t g_hpet_info;
static acpi_mcfg_info_t g_mcfg_info[ACPI_MAX_MCFG_ENTRIES];
static size_t g_mcfg_count;
static acpi_dmar_info_t g_dmar_info;

static bool acpi_checksum_ok(const void *ptr, size_t length) {
    const uint8_t *bytes = (const uint8_t *)ptr;
    uint8_t sum = 0;

    for (size_t i = 0; i < length; i++) {
        sum = (uint8_t)(sum + bytes[i]);
    }
    return sum == 0;
}

static bool acpi_sig_eq(const char *lhs, const char *rhs, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

static void acpi_log_table(const char *signature, const void *table) {
    if (!table) {
        return;
    }
    kprintf("[ACPI] Found %s at 0x%lx\n", signature, (uintptr_t)table);
}

static void acpi_log_aml_deferred(void) {
    if (g_logged_aml_deferred) {
        return;
    }
    g_logged_aml_deferred = true;
    kprintf("[ACPI] AML required, deferred\n");
}

void *acpi_find_table(const char *signature) {
    if (!g_root_sdt || !signature) {
        return NULL;
    }

    size_t entry_count;
    if (g_use_xsdt) {
        acpi_xsdt_t *xsdt = (acpi_xsdt_t *)g_root_sdt;
        entry_count = (g_root_sdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint64_t);
        for (size_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *table = (acpi_sdt_header_t *)(uintptr_t)xsdt->entry[i];
            if (!table) {
                continue;
            }
            if (!acpi_sig_eq(table->signature, signature, 4)) {
                continue;
            }
            if (acpi_checksum_ok(table, table->length)) {
                return table;
            }
        }
    } else {
        acpi_rsdt_t *rsdt = (acpi_rsdt_t *)g_root_sdt;
        entry_count = (g_root_sdt->length - sizeof(acpi_sdt_header_t)) / sizeof(uint32_t);
        for (size_t i = 0; i < entry_count; i++) {
            acpi_sdt_header_t *table = (acpi_sdt_header_t *)(uintptr_t)rsdt->entry[i];
            if (!table) {
                continue;
            }
            if (!acpi_sig_eq(table->signature, signature, 4)) {
                continue;
            }
            if (acpi_checksum_ok(table, table->length)) {
                return table;
            }
        }
    }

    return NULL;
}

static void acpi_parse_madt(acpi_madt_t *madt) {
    if (!madt) {
        return;
    }

    fast_zero(&g_madt_info, sizeof(g_madt_info));
    g_madt_info.lapic_address = madt->lapic_address;
    g_madt_info.flags = madt->flags;

    uint8_t *ptr = madt->entries;
    uint8_t *end = ((uint8_t *)madt) + madt->header.length;

    while (ptr + sizeof(acpi_madt_entry_header_t) <= end) {
        acpi_madt_entry_header_t *entry = (acpi_madt_entry_header_t *)ptr;
        if (entry->length < sizeof(acpi_madt_entry_header_t) || ptr + entry->length > end) {
            break;
        }

        if (entry->type == ACPI_MADT_ENTRY_LAPIC &&
            entry->length >= sizeof(acpi_madt_lapic_t) &&
            g_madt_info.cpu_count < ACPI_MAX_CPUS) {
            acpi_madt_lapic_t *lapic = (acpi_madt_lapic_t *)entry;
            acpi_cpu_info_t *cpu = &g_madt_info.cpus[g_madt_info.cpu_count++];
            cpu->apic_id = lapic->apic_id;
            cpu->acpi_uid = lapic->acpi_processor_uid;
            cpu->flags = lapic->flags;
        } else if (entry->type == ACPI_MADT_ENTRY_IOAPIC &&
                   entry->length >= sizeof(acpi_madt_ioapic_t) &&
                   g_madt_info.ioapic_count < ACPI_MAX_IOAPICS) {
            acpi_madt_ioapic_t *ioapic = (acpi_madt_ioapic_t *)entry;
            acpi_ioapic_info_t *info = &g_madt_info.ioapics[g_madt_info.ioapic_count++];
            info->ioapic_id = ioapic->ioapic_id;
            info->ioapic_address = ioapic->ioapic_address;
            info->gsi_base = ioapic->gsi_base;
        }

        ptr += entry->length;
    }
}

static void acpi_parse_fadt(acpi_fadt_t *fadt) {
    if (!fadt) {
        return;
    }

    fast_zero(&g_fadt_info, sizeof(g_fadt_info));
    g_fadt_info.sci_int = fadt->sci_int;
    g_fadt_info.iapc_boot_arch = fadt->iapc_boot_arch;
    g_fadt_info.flags = fadt->flags;
    g_fadt_info.dsdt = fadt->dsdt;

    if (fadt->dsdt != 0U) {
        acpi_log_aml_deferred();
    }
}

static void acpi_parse_hpet(acpi_hpet_t *hpet) {
    if (!hpet) {
        return;
    }

    fast_zero(&g_hpet_info, sizeof(g_hpet_info));
    g_hpet_info.address = hpet->address.address;
    g_hpet_info.event_timer_block_id = hpet->event_timer_block_id;
    g_hpet_info.minimum_tick = hpet->main_counter_minimum_clock_tick;
    g_hpet_info.number = hpet->hpet_number;
}

static void acpi_parse_mcfg(acpi_mcfg_t *mcfg) {
    if (!mcfg) {
        return;
    }

    g_mcfg_count = 0;

    size_t entry_count = 0;
    if (mcfg->header.length >= sizeof(acpi_mcfg_t)) {
        entry_count = (mcfg->header.length - sizeof(acpi_mcfg_t)) / sizeof(acpi_mcfg_entry_t);
    }

    for (size_t i = 0; i < entry_count && g_mcfg_count < ACPI_MAX_MCFG_ENTRIES; i++) {
        g_mcfg_info[g_mcfg_count].base_address = mcfg->entries[i].base_address;
        g_mcfg_info[g_mcfg_count].segment_group = mcfg->entries[i].pci_segment_group_number;
        g_mcfg_info[g_mcfg_count].start_bus = mcfg->entries[i].start_bus_number;
        g_mcfg_info[g_mcfg_count].end_bus = mcfg->entries[i].end_bus_number;
        g_mcfg_count++;
    }
}

static void acpi_parse_dmar(acpi_dmar_t *dmar) {
    if (!dmar) {
        return;
    }

    g_dmar_present = true;
    g_dmar_parse_valid = true;
    fast_zero(&g_dmar_info, sizeof(g_dmar_info));
    g_dmar_info.host_address_width = dmar->host_address_width;
    g_dmar_info.flags = dmar->flags;

    uint8_t *ptr = dmar->entries;
    uint8_t *end = ((uint8_t *)dmar) + dmar->header.length;

    while (ptr + sizeof(acpi_dmar_entry_header_t) <= end) {
        acpi_dmar_entry_header_t *entry = (acpi_dmar_entry_header_t *)ptr;
        if (entry->length < sizeof(acpi_dmar_entry_header_t) || ptr + entry->length > end) {
            g_dmar_parse_valid = false;
            break;
        }

        if (entry->type == 0 &&
            entry->length >= sizeof(acpi_dmar_drhd_t) &&
            g_dmar_info.unit_count < ACPI_MAX_DMAR_UNITS) {
            acpi_dmar_drhd_t *drhd = (acpi_dmar_drhd_t *)entry;
            acpi_dmar_unit_info_t *unit = &g_dmar_info.units[g_dmar_info.unit_count++];
            unit->flags = drhd->flags;
            unit->segment = drhd->segment;
            unit->register_base = drhd->register_base;
            unit->scope_index = (uint32_t)g_dmar_info.scope_count;
            unit->scope_count = 0;

            uint8_t *scope_ptr = drhd->device_scopes;
            uint8_t *scope_end = ptr + entry->length;
            while (scope_ptr + sizeof(acpi_dmar_device_scope_t) <= scope_end) {
                acpi_dmar_device_scope_t *scope = (acpi_dmar_device_scope_t *)scope_ptr;
                if (scope->length < sizeof(acpi_dmar_device_scope_t) || scope_ptr + scope->length > scope_end) {
                    g_dmar_parse_valid = false;
                    break;
                }
                if (g_dmar_info.scope_count >= ACPI_MAX_DMAR_DEVICE_SCOPES) {
                    g_dmar_parse_valid = false;
                    break;
                }

                acpi_dmar_scope_info_t *scope_info = &g_dmar_info.scopes[g_dmar_info.scope_count++];
                size_t path_len = scope->length - sizeof(acpi_dmar_device_scope_t);
                if (path_len > sizeof(scope_info->path)) {
                    path_len = sizeof(scope_info->path);
                }

                scope_info->type = scope->type;
                scope_info->enumeration_id = scope->enumeration_id;
                scope_info->start_bus = scope->start_bus_number;
                scope_info->path_length = (uint8_t)path_len;
                if (path_len > 0) {
                    memcpy(scope_info->path, scope->path, path_len);
                }
                unit->scope_count++;
                scope_ptr += scope->length;
            }
        }

        ptr += entry->length;
    }
}

bool acpi_get_dmar_info(acpi_dmar_info_t *out) {
    if (!out) {
        return false;
    }

    fast_zero(out, sizeof(*out));
    if (!g_dmar_present || !g_dmar_parse_valid) {
        return false;
    }

    memcpy(out, &g_dmar_info, sizeof(g_dmar_info));
    return true;
}

void acpi_init(void) {
    acpi_rsdp_v1_t *rsdp_v1;

    g_rsdp = NULL;
    g_root_sdt = NULL;
    g_use_xsdt = false;
    g_logged_aml_deferred = false;
    g_dmar_present = false;
    g_dmar_parse_valid = false;
    fast_zero(&g_madt_info, sizeof(g_madt_info));
    fast_zero(&g_fadt_info, sizeof(g_fadt_info));
    fast_zero(&g_hpet_info, sizeof(g_hpet_info));
    fast_zero(&g_mcfg_info, sizeof(g_mcfg_info));
    g_mcfg_count = 0;
    fast_zero(&g_dmar_info, sizeof(g_dmar_info));

    if (!rsdp_request.response || !rsdp_request.response->address) {
        kprintf("[ACPI] RSDP request failed\n");
        return;
    }

    g_rsdp = rsdp_request.response->address;
    rsdp_v1 = (acpi_rsdp_v1_t *)g_rsdp;
    if (!acpi_sig_eq(rsdp_v1->signature, "RSD PTR ", 8)) {
        kprintf("[ACPI] RSDP signature invalid\n");
        return;
    }
    if (!acpi_checksum_ok(rsdp_v1, 20)) {
        kprintf("[ACPI] RSDP checksum failed\n");
        return;
    }

    kprintf("[ACPI] RSDP found at 0x%lx\n", (uintptr_t)g_rsdp);

    if (rsdp_v1->revision >= 2) {
        acpi_rsdp_v2_t *rsdp_v2 = (acpi_rsdp_v2_t *)g_rsdp;
        if (rsdp_v2->length >= sizeof(acpi_rsdp_v2_t) &&
            acpi_checksum_ok(rsdp_v2, rsdp_v2->length) &&
            rsdp_v2->xsdt_address != 0) {
            g_root_sdt = (acpi_sdt_header_t *)(uintptr_t)rsdp_v2->xsdt_address;
            g_use_xsdt = true;
        }
    }

    if (!g_root_sdt && rsdp_v1->rsdt_address != 0U) {
        g_root_sdt = (acpi_sdt_header_t *)(uintptr_t)rsdp_v1->rsdt_address;
        g_use_xsdt = false;
    }

    if (!g_root_sdt) {
        kprintf("[ACPI] No root SDT available\n");
        return;
    }
    if (!acpi_checksum_ok(g_root_sdt, g_root_sdt->length)) {
        kprintf("[ACPI] Root SDT checksum failed\n");
        g_root_sdt = NULL;
        return;
    }

    acpi_madt_t *madt = (acpi_madt_t *)acpi_find_table("APIC");
    acpi_fadt_t *fadt = (acpi_fadt_t *)acpi_find_table("FACP");
    acpi_hpet_t *hpet = (acpi_hpet_t *)acpi_find_table("HPET");
    acpi_mcfg_t *mcfg = (acpi_mcfg_t *)acpi_find_table("MCFG");
    acpi_dmar_t *dmar = (acpi_dmar_t *)acpi_find_table("DMAR");

    acpi_log_table("APIC", madt);
    acpi_log_table("FACP", fadt);
    acpi_log_table("HPET", hpet);
    acpi_log_table("MCFG", mcfg);
    acpi_log_table("DMAR", dmar);

    acpi_parse_madt(madt);
    acpi_parse_fadt(fadt);
    acpi_parse_hpet(hpet);
    acpi_parse_mcfg(mcfg);
    acpi_parse_dmar(dmar);
}

void acpi_self_test(void) {
    if (acpi_find_table("APIC") != NULL) {
        kprintf("[TEST] ACPI Layer 1+2: SUCCESS.\n");
    } else {
        kprintf("[TEST] ACPI Layer 1+2: FAILED (MADT not found).\n");
    }
}
