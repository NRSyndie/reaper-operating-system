#include "include/elf.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/klog.h"
#include "include/utils.h"
#include "include/console.h"

static bool check_headers(Elf64_Ehdr* header) {
    if (*(uint32_t*)header->e_ident != ELF_MAGIC) {
        kprintf("[DAY18-FAIL] invalid elf magic\n");
        kprintf("ELF: Invalid Magic\n");
        return false;
    }
    if (header->e_ident[4] != 2) { // ELFCLASS64
        kprintf("[DAY18-FAIL] elf is not 64-bit\n");
        kprintf("ELF: Not 64-bit\n");
        return false;
    }
    if (header->e_ident[5] != 1) { // ELFDATA2LSB
        kprintf("[DAY18-FAIL] elf is not little-endian\n");
        kprintf("ELF: Not Little Endian\n");
        return false;
    }
    if (header->e_machine != 0x3E) { // x86_64
        kprintf("[DAY18-FAIL] elf machine mismatch\n");
        kprintf("ELF: Not x86_64\n");
        return false;
    }
    return true;
}

int elf_load(void* file_data, process_t* proc, uint64_t* entry_point) {
    Elf64_Ehdr* header = (Elf64_Ehdr*)file_data;
    bool loaded_segment = false;

    if (!check_headers(header)) {
        return -1;
    }
    kprintf("[TEST] Day 18 ELF Header Validation: SUCCESS.\n");

    *entry_point = header->e_entry;
    
    uint8_t* file_bytes = (uint8_t*)file_data;
    Elf64_Phdr* ph_table = (Elf64_Phdr*)(file_bytes + header->e_phoff);

    for (int i = 0; i < header->e_phnum; i++) {
        Elf64_Phdr* ph = &ph_table[i];

        if (ph->p_type != PT_LOAD) {
            continue;
        }

        if (ph->p_memsz == 0) {
            continue;
        }
        loaded_segment = true;

        // Determine VMM Flags
        uint64_t flags = VMM_PRESENT | VMM_USER;
        if (ph->p_flags & PF_W) flags |= VMM_WRITABLE;
        if (!(ph->p_flags & PF_X)) flags |= VMM_NX;

        // Align range to page boundaries
        uint64_t start_addr = ph->p_vaddr;
        uint64_t end_addr = start_addr + ph->p_memsz;
        
        uint64_t start_page = start_addr & ~0xFFF;
        uint64_t end_page = (end_addr + 0xFFF) & ~0xFFF;

        // Iterate pages
        for (uint64_t vaddr = start_page; vaddr < end_page; vaddr += 4096) {
            // Allocate Frame
            uint64_t phys = pmm_alloc(COLOR_CASUAL, proc->pid); // Code/Data is Casual for now
            if (phys == 0) {
                kprintf("[DAY18-FAIL] oom during elf segment load\n");
                kpanic("ELF: OOM during load");
                return -1;
            }

            // Map it
            if (!vmm_map(proc, vaddr, phys, flags)) {
                kprintf("[DAY18-FAIL] elf segment map failed\n");
                kpanic("ELF: Map failed (Law 2 violation?)");
                return -1;
            }

            // Calculate Data to Copy
            // Intersection of [vaddr, vaddr+4096] AND [start_addr, start_addr+filesz]
            
            uint64_t file_end_addr = start_addr + ph->p_filesz;
            
            uint64_t copy_start = (vaddr < start_addr) ? start_addr : vaddr;
            uint64_t copy_end = (vaddr + 4096 > file_end_addr) ? file_end_addr : (vaddr + 4096);
            
            if (copy_start < copy_end) {
                uint64_t offset_in_file = ph->p_offset + (copy_start - start_addr);
                uint64_t offset_in_page = copy_start - vaddr;
                uint64_t copy_len = copy_end - copy_start;
                
                void* dst_ptr = (void*)((uint64_t)pmm_phys_to_virt(phys) + offset_in_page);
                void* src_ptr = (void*)(file_bytes + offset_in_file);
                
                memcpy(dst_ptr, src_ptr, copy_len);
            }
            
            // Note: Pages are already zeroed by pmm_alloc, so BSS regions are handled automatically.
        }
    }

    if (!loaded_segment) {
        kprintf("[DAY18-FAIL] no loadable elf segments\n");
        return -1;
    }

    kprintf("[TEST] Day 18 ELF Loader Contract: SUCCESS.\n");

    return 0;
}
