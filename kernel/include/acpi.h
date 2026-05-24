#ifndef REAPER_ACPI_H
#define REAPER_ACPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) acpi_rsdp_v1_t;

typedef struct {
    acpi_rsdp_v1_t v1;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_v2_t;

typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t entry[];
} __attribute__((packed)) acpi_rsdt_t;

typedef struct {
    acpi_sdt_header_t header;
    uint64_t entry[];
} __attribute__((packed)) acpi_xsdt_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t lapic_address;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed)) acpi_madt_t;

typedef struct {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) acpi_madt_entry_header_t;

#define ACPI_MADT_ENTRY_LAPIC   0
#define ACPI_MADT_ENTRY_IOAPIC  1

typedef struct {
    acpi_madt_entry_header_t header;
    uint8_t acpi_processor_uid;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_lapic_t;

typedef struct {
    acpi_madt_entry_header_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint32_t ioapic_address;
    uint32_t gsi_base;
} __attribute__((packed)) acpi_madt_ioapic_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t reserved;
    uint8_t preferred_pm_profile;
    uint16_t sci_int;
    uint32_t smi_cmd;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_cnt;
    uint32_t pm1a_evt_blk;
    uint32_t pm1b_evt_blk;
    uint32_t pm1a_cnt_blk;
    uint32_t pm1b_cnt_blk;
    uint32_t pm2_cnt_blk;
    uint32_t pm_tmr_blk;
    uint32_t gpe0_blk;
    uint32_t gpe1_blk;
    uint8_t pm1_evt_len;
    uint8_t pm1_cnt_len;
    uint8_t pm2_cnt_len;
    uint8_t pm_tmr_len;
    uint8_t gpe0_len;
    uint8_t gpe1_len;
    uint8_t gpe1_base;
    uint8_t cst_cnt;
    uint16_t p_lvl2_lat;
    uint16_t p_lvl3_lat;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alrm;
    uint8_t mon_alrm;
    uint8_t century;
    uint16_t iapc_boot_arch;
    uint8_t reserved2;
    uint32_t flags;
} __attribute__((packed)) acpi_fadt_t;

typedef struct {
    acpi_sdt_header_t header;
    uint32_t event_timer_block_id;
    struct {
        uint8_t address_space_id;
        uint8_t register_bit_width;
        uint8_t register_bit_offset;
        uint8_t reserved;
        uint64_t address;
    } __attribute__((packed)) address;
    uint8_t hpet_number;
    uint16_t main_counter_minimum_clock_tick;
    uint8_t page_protection_and_oem_attribute;
} __attribute__((packed)) acpi_hpet_t;

typedef struct {
    uint64_t base_address;
    uint16_t pci_segment_group_number;
    uint8_t start_bus_number;
    uint8_t end_bus_number;
    uint32_t reserved;
} __attribute__((packed)) acpi_mcfg_entry_t;

typedef struct {
    acpi_sdt_header_t header;
    uint64_t reserved;
    acpi_mcfg_entry_t entries[];
} __attribute__((packed)) acpi_mcfg_t;

typedef struct {
    acpi_sdt_header_t header;
    uint8_t host_address_width;
    uint8_t flags;
    uint8_t reserved[10];
    uint8_t entries[];
} __attribute__((packed)) acpi_dmar_t;

typedef struct {
    uint16_t type;
    uint16_t length;
} __attribute__((packed)) acpi_dmar_entry_header_t;

typedef struct {
    acpi_dmar_entry_header_t header;
    uint8_t flags;
    uint8_t reserved;
    uint16_t segment;
    uint64_t register_base;
    uint8_t device_scopes[];
} __attribute__((packed)) acpi_dmar_drhd_t;

typedef struct {
    uint8_t type;
    uint8_t length;
    uint16_t reserved;
    uint8_t enumeration_id;
    uint8_t start_bus_number;
    uint8_t path[];
} __attribute__((packed)) acpi_dmar_device_scope_t;

typedef struct {
    uint8_t type;
    uint8_t enumeration_id;
    uint8_t start_bus;
    uint8_t path_length;
    /* PCI path depth is bounded by 8 device/function hops, so 16 bytes covers the full encoded path. */
    uint8_t path[16];
} acpi_dmar_scope_info_t;

typedef struct {
    uint8_t flags;
    uint16_t segment;
    uint64_t register_base;
    uint32_t scope_index;
    uint32_t scope_count;
} acpi_dmar_unit_info_t;

#define ACPI_MAX_DMAR_UNITS          16
#define ACPI_MAX_DMAR_DEVICE_SCOPES  64

typedef struct {
    uint8_t host_address_width;
    uint8_t flags;
    acpi_dmar_unit_info_t units[ACPI_MAX_DMAR_UNITS];
    size_t unit_count;
    acpi_dmar_scope_info_t scopes[ACPI_MAX_DMAR_DEVICE_SCOPES];
    size_t scope_count;
} acpi_dmar_info_t;

void acpi_init(void);
void *acpi_find_table(const char *signature);
bool acpi_get_dmar_info(acpi_dmar_info_t *out);
void acpi_self_test(void);

#endif
