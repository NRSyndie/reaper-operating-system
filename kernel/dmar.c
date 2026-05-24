#include "include/acpi.h"
#include "include/console.h"
#include "include/dmar.h"
void dmar_init(void) {
    dmar_header_t *dmar = (dmar_header_t *)acpi_find_table("DMAR");
    if (!dmar) {
        kprintf("DMAR: Not found\n");
        return;
    }

    kprintf("DMAR: Found at %p, length %u\n", dmar, dmar->length);
}
