# PCID Subsystem API

The PCID subsystem provides mode-aware hardware context allocation and fail-closed CR3 switching for Reality isolation.

## 1. Allocator API

Defined in `kernel/include/pcid.h`.

### `void pcid_init(void)`
Initializes mode-partitioned allocator state and metrics.

### `uint16_t pcid_alloc(mode_id_t mode)`
Allocates a PCID inside the caller mode range.
- Returns a mode-scoped PCID on success.
- Returns `PCID_ERROR` on exhaustion/invalid mode.

### `void pcid_free(uint16_t pcid, mode_id_t mode)`
Releases a PCID back to the mode allocator.
- Validates range ownership.
- Performs `INVPCID` single-context scrub before reuse.

### `uint16_t pcid_get_mode_base(mode_id_t mode)`
Returns base PCID for the mode.

### `uint16_t pcid_get_mode_count(mode_id_t mode)`
Returns allocatable PCID count for the mode.

## 2. VMM Interface

Defined in `kernel/include/vmm.h`.

### `void vmm_switch(uint64_t pml4_phys, uint16_t pcid, mode_id_t mode)`
Fail-closed CR3 switch primitive.
- Panics on invalid PCID, mode, or unaligned PML4 address.
- Enforces mode->PCID range invariants.
- Preserves TLB (`NOFLUSH=1`) except mandatory flush scenarios (e.g., GHOST).

### `uint16_t vmm_get_current_pcid(void)`
Returns active CR3 PCID bits.

### `void invpcid_flush_single(uint16_t pcid, uint64_t addr)`
Flushes a single VA for one PCID.

### `void invpcid_flush_context(uint16_t pcid)`
Flushes all non-global entries for one PCID.

### `void invpcid_flush_all(void)`
Flushes all contexts.

## 3. Secure Transition Helpers

Defined in `kernel/include/mode.h`.

### `void mode_enter_secure_context(void)`
Switches kernel to `PCID_KERNEL_SECURE` for transition-critical operations.

### `void mode_exit_secure_context(void)`
Returns kernel to `PCID_KERNEL`.
