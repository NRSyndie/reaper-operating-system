#ifndef REAPER_ELF_H
#define REAPER_ELF_H

#include <stdint.h>
#include <stdbool.h>

#define ELF_MAGIC 0x464C457F /* "\x7FELF" in little endian */

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;

/* ELF Header */
typedef struct {
    uint8_t  e_ident[16];   /* Magic number and other info */
    uint16_t e_type;        /* Object file type */
    uint16_t e_machine;     /* Architecture */
    uint32_t e_version;     /* Object file version */
    uint64_t e_entry;       /* Entry point virtual address */
    uint64_t e_phoff;       /* Program header table file offset */
    uint64_t e_shoff;       /* Section header table file offset */
    uint32_t e_flags;       /* Processor-specific flags */
    uint16_t e_ehsize;      /* ELF header size in bytes */
    uint16_t e_phentsize;   /* Program header table entry size */
    uint16_t e_phnum;       /* Program header table entry count */
    uint16_t e_shentsize;   /* Section header table entry size */
    uint16_t e_shnum;       /* Section header table entry count */
    uint16_t e_shstrndx;    /* Section header string table index */
} Elf64_Ehdr;

/* Program Header */
typedef struct {
    uint32_t p_type;        /* Segment type */
    uint32_t p_flags;       /* Segment flags */
    uint64_t p_offset;      /* Segment file offset */
    uint64_t p_vaddr;       /* Segment virtual address */
    uint64_t p_paddr;       /* Segment physical address */
    uint64_t p_filesz;      /* Segment size in file */
    uint64_t p_memsz;       /* Segment size in memory */
    uint64_t p_align;       /* Segment alignment */
} Elf64_Phdr;

/* Program Header Types */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

/* Program Header Flags */
#define PF_X       1        /* Executable */
#define PF_W       2        /* Writable */
#define PF_R       4        /* Readable */

/* Public API */

#include "process.h"

/**
 * elf_load: Parse and map an ELF64 executable into a target World.
 * Returns 0 on success, or negative error code.
 * entry_point is populated with the ELF entry address.
 */
int elf_load(void* file_data, process_t* proc, uint64_t* entry_point);


#endif /* REAPER_ELF_H */
