# PCID Subsystem Design (Reality Binding)

PCID colorization binds hardware context IDs to system Realities (Modes) so TLB state cannot bleed across mode boundaries.

## 1. Address Space Layout

| Range | Owner |
| :--- | :--- |
| `0` | `PCID_KERNEL` |
| `1-256` | `MODE_CASUAL` |
| `257-512` | `MODE_SECURE` |
| `513-768` | `MODE_LOCKDOWN` |
| `769-1024` | `MODE_GHOST` |
| `4095` | `PCID_KERNEL_SECURE` |

All ranges are non-overlapping and enforced in `vmm_switch(...)`.

## 2. Allocation & Reuse

- Per-mode bitmap allocators provide deterministic bounded allocation.
- Free path validates mode ownership and range membership.
- Free path performs `INVPCID` single-context scrub before bitmap release.
- Reuse metrics and high-water marks are tracked for visibility.

## 3. Switch Semantics

`vmm_switch(pml4, pcid, mode)` is fail-closed:

- panic on invalid PCID
- panic on invalid mode
- panic on unaligned PML4 input
- panic on mode/PCID mismatch

`NOFLUSH=1` is used by default for performance. `MODE_GHOST` enforces `NOFLUSH=0` for mandatory flush semantics.

## 4. Transition Security

Mode transitions route sensitive flush/bleach work through secure kernel context:

- `mode_enter_secure_context()` (`PCID_KERNEL_SECURE`)
- transition-critical operations
- `mode_exit_secure_context()` (`PCID_KERNEL`)

This prevents sensitive transition logic from sharing normal kernel TLB context.

## 5. Operational Evidence

Day 25 closure is governed by deterministic markers:

- `[TEST] Day 25 PCID Partition Contract: SUCCESS.`
- `[TEST] Day 25 TLB Scrub Contract: SUCCESS.`
- `[TEST] Day 25 Secure Context Contract: SUCCESS.`

Forbidden marker:

- `[DAY25-FAIL]`
