# Epoch III, Day 86: DMA Authority Contract + DMAR Truth Freeze

**Date:** April 29, 2026  
**Status:** DONE  
**Modules:** `kernel/iommu.c`, `kernel/include/iommu.h`, `kernel/acpi.c`, `kernel/include/acpi.h`, `kernel/include/audit.h`, `kernel/main.c`

## 1. Scope & Objective
Freeze the kernel truth model for DMA/IOMMU work before enabling VT-d. Day 86 establishes canonical DMAR inventory, explicit IOMMU runtime state, degraded-mode policy, and audit visibility.

## 2. What Was Implemented
- Added a canonical `iommu_inventory_t` model with explicit state and degraded-reason enums.
- Made ACPI the only DMAR parser and exported read-only DMAR inventory through `acpi_get_dmar_info(...)`.
- Implemented `iommu_init()` to:
  - classify missing DMAR as explicit unavailable state
  - reject invalid or ambiguous inventory as degraded state
  - accept valid DMAR topology as inventoried state
- Added `iommu_self_test()` and wired it into the boot sequence.
- Added audit event types for IOMMU inventory, degraded state, unit discovery, and topology rejection.
- Documented the DMA authority model and the IOMMU audit target encoding.

## 3. Why It Was Added
- Hardware isolation work is not aligned with the project vision unless it is capability-minded, auditable, and fail-closed.
- This slice freezes the truth and policy surface first so later VT-d programming does not invent authority semantics ad hoc.
- Explicit degraded/unavailable states prevent false claims of protection on unsupported or ambiguous platforms.

## 4. Verification Evidence
- [PASS] `make -C kernel clean && make -C kernel`
- [PASS] `make -C kernel run`
- [PASS] Serial log markers in `kernel/serial.log`:
  - `[IOMMU] Inventory start`
  - explicit `[IOMMU] State: ...`
  - `[TEST] IOMMU Inventory Contract: SUCCESS.`
  - `[TEST] IOMMU Degraded Policy Contract: SUCCESS.`

## 5. Known Limits / Follow-Up
- VT-d translation enablement remains deferred to the next slice.
- `dma_map()` / `dma_unmap()` lifecycle contracts remain future work.
- Device-domain attachment and invalidation policy remain future work.
