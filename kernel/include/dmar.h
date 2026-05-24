#ifndef DMAR_H
#define DMAR_H

#include <stdint.h>

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
    uint8_t host_address_width;
    uint8_t flags;
    uint8_t reserved[10];
} __attribute__((packed)) dmar_header_t;

typedef struct {
    uint16_t type;
    uint16_t length;
} __attribute__((packed)) dmar_entry_header_t;

typedef struct {
    dmar_entry_header_t header;
    uint8_t flags;
    uint8_t reserved;
    uint16_t segment;
    uint64_t address;
} __attribute__((packed)) dmar_drhd_t;

#define DMAR_TYPE_DRHD 0

#endif
