# VMM Region Contracts: Final Product Contract Engine (Epoch III)

## Objective
Make virtual-memory operations explicit, auditable, and deterministic through a single contract engine that compiles intent and applies it with fail-closed behavior.

## Contract Model
- `vmm_region_contract_t` captures:
  - operation (`MAP` / `UNMAP`)
  - virtual start
  - physical start
  - length
  - PTE flags
  - owner identity/mode
  - trust level
  - result code
  - applied-page count
- `vmm_compiled_mapping_t` carries normalized execution intent and a result code.
- `vmm_contract_result_t` defines deterministic failure/success outcomes.

## Compile Phase
- `vmm_compile_region_contract(...)` validates:
  - operation legality
  - non-zero length
  - page alignment for virtual/physical start
  - page-aligned length
  - overflow-safe range checks
  - mode/policy constraints (user mappings must stay below kernel boundary)
  - operation-specific flag constraints (`UNMAP` requires zero `phys_start`/`pte_flags`)
- Output is `vmm_compiled_mapping_t` (normalized page-count mapping intent).

## Apply Phase
- `vmm_apply_compiled_mapping(...)` applies compiled intent deterministically:
  - preflight legality checks against current page-table state
  - per-page mapping through internal raw walker path
  - transactional rollback for partial map failures
  - preflight-backed unmap parity through the same contract engine
  - append every attempt (success/failure) to in-kernel recent contract log

## Compatibility Bridge
- Existing `vmm_map(...)` now routes through:
  1. contract creation
  2. contract compile
  3. compiled mapping apply
- `vmm_unmap_region(...)` routes unmap intent through the same compile/apply path.
- Existing callers remain source-compatible.

## Observability
- Recent contracts can be read via `vmm_read_recent_contracts(...)` for diagnostics/audit.
- Contract execution counters are exposed via `vmm_get_contract_metrics(...)`.
- Runtime gate is enforced via `vmm_contract_self_test()` and boot marker:
  - `[TEST] VMM contract engine: SUCCESS.`

## Current Limits
- Engine covers page-granular map/unmap contracts.
- Advanced lifecycle operations (`PROTECT`, ownership transfer, batched audit export) remain future expansion points.
