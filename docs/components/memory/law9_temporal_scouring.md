# Law 9: Temporal Scouring (PMM Epoch Hygiene)

This document defines the current Reaper-OS implementation of **Law 9** and how to validate it.

## 1. Purpose

Law 9 prevents cross-epoch memory residue by tying frame reuse to the global mode security epoch:

- if a frame was last touched in a different epoch, allocation uses aggressive scrub (`hyper_scrub`)
- if a frame was last touched in the same epoch, allocation still zeroes (`fast_zero`)

This guarantees every allocated frame is cleared while applying stronger scrubbing on epoch boundaries.

## 2. Source of Truth (Code Paths)

- PMM metadata epoch field: `kernel/include/pmm.h`
  - `struct frame_metadata::epoch`
- Global security epoch anchor: `kernel/include/mode_internal.h`
  - `struct mode_state::security_epoch`
- Epoch read API: `kernel/include/mode.h`
  - `mode_get_security_epoch()`
- Epoch increment event: `kernel/mode.c`
  - incremented on accepted `mode_request_transition(...)`
- Law 9 allocation enforcement: `kernel/pmm.c`
  - mismatch check: `pmm_needs_temporal_scour(...)`
  - mismatch path: `hyper_scrub(...)`
  - same-epoch path: `fast_zero(...)`
- Law 9 observability counters: `kernel/pmm.c` + `kernel/include/pmm.h`
  - `pmm_law9_stats(...)`

## 3. Enforcement Semantics

During `pmm_alloc(...)`:

1. `alloc_epoch = mode_get_security_epoch()`
2. compare `metadata[idx].epoch` with current epoch (low 8-bit compare)
3. if mismatch: run `hyper_scrub(page)`, increment `law9_temporal_scour_count`
4. else: run `fast_zero(page)`, increment `law9_same_epoch_zero_count`
5. stamp `metadata[idx].epoch = alloc_epoch`

During `pmm_free(...)`:

- frame is scrubbed/zeroed per existing color rules
- `metadata[idx].epoch` is updated to the current security epoch before returning to free list

During `pmm_transfer(...)`:

- ownership/color/state transfer updates `metadata[idx].epoch` to current security epoch

## 4. Runtime Markers and Metrics

- First observed temporal-scour allocation emits:
  - `[LAW9] Temporal scouring active (epoch mismatch path engaged).`
- Counters exported by `pmm_law9_stats(...)`:
  - `temporal_scours`: epoch mismatch allocations
  - `same_epoch_zeroes`: same-epoch allocations

Reference boot logs containing marker:

- `kernel/serial_matrix_run1.log`
- `kernel/serial_matrix_run2.log`
- `kernel/serial_matrix_run3.log`

## 5. Validation Procedure

From repo root:

1. Build images:
   - `make -C user`
   - `make -C kernel iso`
2. Boot:
   - `make -C kernel run`
3. Confirm marker in serial log:
   - `rg "\[LAW9\]" kernel/serial.log`

Optional repeated verification:

- inspect `kernel/serial_matrix_run*.log` for the same marker.

## 6. Design Constraints and Current Limits

- `frame_metadata` is fixed at 16 bytes by invariant, so per-frame epoch is currently `uint8_t`.
- Global mode epoch is `uint64_t`, but PMM comparison uses the low 8 bits when storing/loading frame epoch.
- Effective mismatch detection therefore wraps every 256 mode-epoch increments.

This is intentional for current metadata footprint constraints and must be considered when extending long-running epoch analytics.

## 7. Related Docs

- `docs/development_log/epoch_two_plan.md`
- `docs/components/modes/mode_api.md`
- `docs/development_log/TODO.rst`
