# Epoch III, Day 84: ACPI Layer 1/2 Foundation Closure

**Date:** April 29, 2026  
**Status:** DONE  
**Modules:** `kernel/acpi.c`, `kernel/include/acpi.h`, `kernel/limine_reqs.c`, `kernel/main.c`, `kernel/dmar.c`

## 1. Scope & Objective
Complete the foundational ACPI subsystem needed for later IOMMU, timer, PCIe, and topology work without introducing AML interpretation into the kernel.

## 2. What Was Implemented
- Added a boot-valid `acpi_init()` entry path and removed the previous ACPI panic stub.
- Kept Limine's RSDP handoff as the discovery source and validated the RSDP plus the selected root SDT checksum.
- Implemented `acpi_find_table(const char *signature)` as the public ACPI lookup API.
- Added static parsing for:
  - MADT: LAPIC and IOAPIC topology data
  - FADT: system capability flags and SCI/boot-arch fields
  - HPET: timer base address and block metadata
  - MCFG: PCIe configuration-space segment ranges
  - DMAR: DRHD units and device-scope capture for later VT-d work
- Logged ACPI discovery lines in the normal boot log.
- Added `acpi_self_test()` and wired it into the boot sequence.

## 3. Why It Was Added
- Later hardware security and device work depends on reliable firmware table discovery.
- ACPI table parsing belongs in one self-contained kernel module rather than being spread across early hardware bring-up code.
- AML interpretation is intentionally deferred so the kernel can consume only static firmware data for now.

## 4. Verification Evidence
- [PASS] `make -C kernel`
- [PASS] `make -C kernel run`
- [PASS] Serial log markers in `kernel/serial.log`:
  - `[ACPI] RSDP found at 0xffff8000000f5290`
  - `[ACPI] Found APIC at 0x1ffe1b7a`
  - `[TEST] ACPI Layer 1+2: SUCCESS.`

## 5. Known Limits / Follow-Up
- AML interpretation remains explicitly deferred.
- `iommu_init()` and `dma_map()` policy enforcement are still separate follow-on work.
- MCFG and DMAR data are parsed and retained, but downstream consumers remain minimal today.
