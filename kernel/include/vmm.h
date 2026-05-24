#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

// x86_64 Page Table Entry Flags
#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_PWT      (1ULL << 3)
#define VMM_PCD      (1ULL << 4)
#define VMM_ACCESSED (1ULL << 5)
#define VMM_DIRTY    (1ULL << 6)
#define VMM_HUGE     (1ULL << 7)
#define VMM_GLOBAL   (1ULL << 8)

/* SOFTWARE BITS (9-11 available) */
#define VMM_DEMAND   (1ULL << 9)
#define VMM_COW      (1ULL << 10)
#define PTE_DOORBELL (1ULL << 11)

#define VMM_NX       (1ULL << 63)

// Standard Page Permissions
#define PAGE_USER_CODE (VMM_PRESENT | VMM_USER)
#define PAGE_USER_DATA (VMM_PRESENT | VMM_WRITABLE | VMM_USER | VMM_NX)
#define PAGE_USER_DATA_DEMAND (VMM_WRITABLE | VMM_USER | VMM_DEMAND)

// Kernel Boundary
#define KERNEL_VIRT_BASE 0xffffffff80000000

typedef uint64_t pt_entry_t;

typedef enum {
    VMM_CONTRACT_OP_MAP = 0,
    VMM_CONTRACT_OP_UNMAP = 1
} vmm_contract_op_t;

typedef enum {
    VMM_CONTRACT_TRUST_VERIFIED = 0,
    VMM_CONTRACT_TRUST_UNVERIFIED = 1
} vmm_contract_trust_t;

typedef enum {
    VMM_CONTRACT_RESULT_NONE = 0,
    VMM_CONTRACT_RESULT_OK,
    VMM_CONTRACT_RESULT_ERR_NULL,
    VMM_CONTRACT_RESULT_ERR_OP,
    VMM_CONTRACT_RESULT_ERR_LENGTH,
    VMM_CONTRACT_RESULT_ERR_ALIGNMENT,
    VMM_CONTRACT_RESULT_ERR_OVERFLOW,
    VMM_CONTRACT_RESULT_ERR_FLAGS,
    VMM_CONTRACT_RESULT_ERR_POLICY,
    VMM_CONTRACT_RESULT_ERR_ALREADY_MAPPED,
    VMM_CONTRACT_RESULT_ERR_NOT_MAPPED,
    VMM_CONTRACT_RESULT_ERR_WALK,
    VMM_CONTRACT_RESULT_ERR_ALLOC,
    VMM_CONTRACT_RESULT_ERR_ROLLBACK
} vmm_contract_result_t;

typedef struct {
    vmm_contract_op_t op;
    uint64_t virt_start;
    uint64_t phys_start;
    uint64_t length;
    uint64_t pte_flags;
    uint64_t owner_pid;
    uint8_t owner_mode;
    vmm_contract_trust_t trust;
    vmm_contract_result_t result;
    uint64_t applied_pages;
} vmm_region_contract_t;

typedef struct {
    vmm_contract_op_t op;
    uint64_t virt_start;
    uint64_t phys_start;
    uint64_t page_count;
    uint64_t pte_flags;
    uint64_t owner_pid;
    uint8_t owner_mode;
    vmm_contract_trust_t trust;
    vmm_contract_result_t result;
} vmm_compiled_mapping_t;

typedef struct {
    uint64_t compile_ok;
    uint64_t compile_fail;
    uint64_t apply_ok;
    uint64_t apply_fail;
    uint64_t rollback_attempts;
    uint64_t rollback_failures;
} vmm_contract_metrics_t;

typedef struct {
    uint64_t total_mappings;
    uint64_t total_unmappings;
    uint64_t page_faults;

    // PCID Metrics
    uint16_t max_pcid_casual;
    uint16_t max_pcid_secure;
    uint16_t max_pcid_lockdown;
    uint16_t max_pcid_ghost;
    uint64_t pcid_switches;
    uint64_t pcid_switch_flushes;
    uint64_t pcid_switch_rejects;
} vmm_statistics_t;

extern vmm_statistics_t vmm_stats;

bool vmm_handle_fault(uint64_t vaddr, uint64_t error_code);

/**
 * @brief Initialize the Virtual Memory Manager.
 * Creates the kernel's master page tables and maps the HHDM and Kernel ELF.
 */
void vmm_init(void);

#include "process.h"

/**
 * @brief Write a raw entry into a Page Table.
 * @param table_phys Physical address of the table (PML4, PDPT, PD, PT).
 * @param index Index (0-511).
 * @param value The 64-bit entry value (Physical Address | Flags).
 */
void vmm_set_entry(uint64_t table_phys, uint64_t index, uint64_t value);

/**
 * @brief Map a physical frame to a virtual address.
 * Uses the process's spare_pt_pool for page table bricks.
 */
bool vmm_map(process_t* proc, uint64_t virt, uint64_t phys, uint64_t flags);
bool vmm_compile_region_contract(const vmm_region_contract_t* contract, vmm_compiled_mapping_t* compiled);
bool vmm_apply_compiled_mapping(process_t* proc, const vmm_compiled_mapping_t* compiled);
int vmm_read_recent_contracts(vmm_region_contract_t* out, size_t max_count);
bool vmm_unmap_region(process_t* proc, uint64_t virt, uint64_t length);
bool vmm_get_contract_metrics(vmm_contract_metrics_t* out_metrics);
bool vmm_contract_self_test(void);

/**
 * @brief Unmap a virtual address and invalidate TLB.
 */
void vmm_unmap(pt_entry_t* pml4, uint64_t virt);

/**
 * @brief Translate a virtual address to its physical counterpart.
 */
uint64_t vmm_virt_to_phys(pt_entry_t* pml4, uint64_t virt);

/**
 * @brief Activate a new address space by loading CR3.
 */
void vmm_switch(uint64_t pml4_phys, uint16_t pcid, mode_id_t mode);

/**
 * @brief Get the currently active PCID from CR3.
 */
uint16_t vmm_get_current_pcid(void);

/**
 * @brief Invalidate a single TLB entry for a specific PCID.
 */
void invpcid_flush_single(uint16_t pcid, uint64_t addr);

/**
 * @brief Invalidate all TLB entries for a specific PCID.
 */
void invpcid_flush_context(uint16_t pcid);

/**
 * @brief Invalidate all TLB entries (Global flush).
 */
void invpcid_flush_all(void);

/**
 * @brief Create a copy of the current PML4.
 */
uint64_t vmm_fork_pml4(void);

/**
 * @brief Annihilate a PML4 and all its user-space mappings.
 */
void vmm_destroy_pml4(uint64_t pml4_phys);

/**
 * @brief [DEBUG] Check and log the EFER MSR state (specifically NXE).
 */
void vmm_check_efer(void);

/**
 * @brief [DEBUG] Manually walk the page tables for a VA and log permissions at each level.
 */
void vmm_dump_page_walk(uint64_t pml4_phys, uint64_t virt);

#endif
