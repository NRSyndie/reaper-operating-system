# Day 86: DMA Authority Contract + DMAR Truth Freeze

## 1. Contract Freeze
- [x] DMA/IOMMU authority model documented.
- [x] Degraded/unavailable/enforced state model defined.
- [x] No-ambient-DMA invariant documented.

## 2. Kernel Truth Surface
- [x] ACPI exports canonical DMAR inventory.
- [x] `iommu_init()` validates DMAR inventory and emits explicit state markers.
- [x] Pre-init inventory state is deterministic (`IOMMU_STATE_UNINITIALIZED`).

## 3. Fail-Closed Policy
- [x] Missing DMAR produces explicit unavailable state.
- [x] Invalid or ambiguous topology produces explicit degraded state.
- [x] No silent fallback path remains.

## 4. Audit Surface
- [x] IOMMU inventory/degraded/topology event types added.
- [x] Audit target encoding documented for segment/detail decoding.

## 5. Verification
- [x] `make -C kernel clean && make -C kernel`
- [x] `make -C kernel run`
- [x] Serial markers captured for IOMMU state and self-test results.
