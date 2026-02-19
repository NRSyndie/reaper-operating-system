# PMM Zoned-Policy to Buddy Migration Strategy (Epoch III)

## Goal
Transition from frame-only allocation to a policy-aware zoned allocator, with buddy-style multi-order allocation as the target backend.

## Why
- Improve fragmentation behavior and future large-block support.
- Express security intent at allocation call sites (zone + trust + order).
- Preserve deterministic, auditable behavior while performance scales.

## Current Groundwork (Day 44)
- Added policy allocation interface in PMM:
  - `pmm_alloc_ex(const pmm_alloc_policy_t*)`
  - `pmm_free_ex(uint64_t, uint8_t order)`
- Added policy dimensions:
  - zone preference (`DMA`, `NORMAL`, `HIGH`, `ANY`)
  - trust level (`VERIFIED`, `UNVERIFIED`, `ANY`)
  - allocation order (currently compatibility-limited to order `0`)
- Kept compatibility:
  - existing `pmm_alloc/pmm_free` behavior unchanged for current callers
  - policy path currently delegates to frame allocator with strict fail-closed guards

## Migration Phases
1. **Policy Surface (current)**
   - expose zone/trust/order contract without breaking existing ABI/users.
2. **Zone Inventory**
   - classify memmap ranges into zones with deterministic boundaries.
3. **Buddy Backend**
   - introduce multi-order free structures and coalescing.
4. **Policy Enforcement**
   - enforce trust-tier and zone constraints against buddy allocation paths.
5. **Caller Migration**
   - move VMM/slab/kmalloc high-risk call sites to `pmm_alloc_ex`.
6. **Legacy Path Retirement**
   - keep wrappers for compatibility, but make policy path authoritative.

## Safety Constraints
- Fail closed on unsupported order/policy combinations.
- Keep strict profile trust guardrails active.
- Preserve existing PMM/fate/law2 runtime markers while adding new policy markers.

## Validation Gates
- `make -C user`
- `make -C kernel`
- `make -C kernel iso`
- `make -C kernel verify_matrix`
- serial markers for policy failures must be explicit and reason-coded.
