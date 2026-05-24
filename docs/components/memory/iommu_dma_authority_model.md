# IOMMU / DMA Authority Model

## 1. Objective
Treat DMA as a governed authority surface rather than a driver convenience path. Devices must not gain memory reachability without explicit kernel-owned truth and explicit forensic visibility.

## 2. Core Invariants
- No ambient DMA exists.
- DMAR is the only firmware source of truth for Intel VT-d unit and device-scope topology.
- Firmware ambiguity is fail-closed.
- IOMMU runtime state is explicit: `UNINITIALIZED`, `INVENTORIED`, `DEGRADED`, `UNAVAILABLE`, `ENFORCED`.
- Userspace may never infer enforcement from boot success alone; enforcement state must be logged.
- Audit visibility is required for inventory, degraded mode, and topology rejection.

## 3. Authority Split
- The kernel owns:
  - DMAR truth extraction
  - IOMMU unit inventory
  - degraded/unavailable policy state
  - later VT-d programming and invalidation truth
- Userspace will later own:
  - policy requests for device delegation
  - higher-level meaning of which service should receive device authority
- No userspace component directly programs VT-d registers.

## 4. Day 86 Contract
Day 86 freezes truth and policy without claiming full enforcement.

- `acpi_init()` parses DMAR and exports canonical read-only inventory.
- `iommu_init()` validates inventory and classifies the runtime state.
- `iommu_self_test()` verifies:
  - pre-init state is not exposed as garbage
  - DMAR absence is explicit and reason-coded
  - valid inventory is explicit and bounded

## 5. Degraded Policy
- `IOMMU_STATE_UNAVAILABLE`
  - no DMAR table or no usable inventory source
  - the system continues booting, but no DMA-isolated security claim is made
- `IOMMU_STATE_DEGRADED`
  - DMAR present but invalid, ambiguous, or otherwise not trustworthy
  - the system continues booting, but later DMA authority work must fail closed
- `IOMMU_STATE_INVENTORIED`
  - topology truth established
  - enforcement intentionally deferred to the VT-d enablement slice
- `IOMMU_STATE_ENFORCED`
  - reserved for the later hardware activation slice

## 6. Audit Contract
Audit target encoding for IOMMU events:
- bits `63:48` = PCI segment
- bits `47:0` = unit index or degraded/reject detail

Day 86 emits:
- inventory event
- unit-discovered event(s)
- degraded event
- topology-reject event when ambiguity or invalid topology is detected

## 7. Non-Goals
- No broad identity domain.
- No silent compatibility DMA path.
- No interrupt remapping.
- No MSI/MSI-X work.
- No userspace device delegation path yet.
