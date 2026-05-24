# Day 30 Closure Contract (Final Product)

This document freezes closure criteria for **Epoch II, Day 30: Fate Strings Rejection Auditing**.

## 1. Scope

Day 30 closure covers:

- Fate record `result_code` evidence for accepted and rejected transitions
- unified hash-chain integrity across accepted/rejected transition events
- deterministic runtime evidence and fail-closed marker gates for rejection auditing
- kernel-owned Day 30 attestation evidence (`[LAW2_ATTEST] day=30`)
- deterministic reject-reason coverage for core denial classes (`EDGE_ILLEGAL`, `AUTH_REQUIRED`, `SPECIAL_KEY_REQUIRED`)
- bounded Day 30 attestation scan latency budget evidence

## 2. Vision Alignment Contract

- Fate Strings remain an immutable audit substrate for both allowed and blocked state changes.
- Illegal transition attempts are visible to auditors, not silently dropped.
- Day 30 behavior is closure-gated by repeatable runtime evidence.

## 3. Security Contract

- Rejected transition attempts must be persisted in Fate history.
- Hash-chain integrity must remain valid while rejected evidence is included.
- Day 30 regressions emit explicit `[DAY30-FAIL]` markers and block closure.
- Day 30 attestation must include full reject-class coverage bitmask (`LAW2_DAY30_REASON_MASK_REQUIRED`).

## 4. Performance Contract

- Day 30 checks add bounded Fate-read validation probes only.
- No unbounded loops/retry logic are introduced by Day 30 closure logic.
- Repeat-run closure suite remains deterministic across runs.
- Day 30 attestation scan latency must remain within budget (`day30_reject_scan_cycles <= day30_perf_budget_cycles`).

## 5. Runtime Evidence Markers

Required markers:

- `[TEST] Day 30 Rejection Auditing Contract: SUCCESS.`
- `[TEST] Day 30 Fate Result-Code Contract: SUCCESS.`
- `[TEST] Day 30 Rejected Evidence Contract: SUCCESS.`
- `[TEST] Day 30 Reason Coverage Contract: SUCCESS.`
- `[TEST] Day 30 Performance Budget Contract: SUCCESS.`
- line beginning with `[LAW2_ATTEST] day=30 result=PASS`

Forbidden markers:

- `[DAY30-FAIL]`
- any line beginning with `[LAW2_ATTEST] day=30 result=FAIL`

## 6. Enforcement Points

- Fate append/history logic: `kernel/mode.c`
- kernel attestation records: `kernel/mode.c` (`FATE_RECORD_ATTEST`)
- runtime rejection-auditing probes + markers: `user/paradigm/main.c`
- closure gates: `tools/run_law2_fate_matrix.sh`, `tools/run_day30_closure_suite.sh`

## 7. Exit Criteria

Day 30 is closure-ratified only when all are true:

- `make -C user` passes
- `make -C kernel` passes
- `make -C kernel iso` passes
- `make -C kernel verify_matrix` passes with Day 30 required markers and no Day 30 forbidden markers
- `./tools/run_day30_closure_suite.sh` passes repeat-run closure checks
- docs/version/checklists are synchronized with concrete evidence
