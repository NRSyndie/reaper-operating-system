# Final Redesign Strategy (One-Way Implementation)

## 1. Goal
Deliver a one-way redesign aligned to project vision, security, and performance goals, with deterministic closure gates that block release on any regression.

## 2. Execution Model
- Track R (Redesign): architecture and subsystem rewrites.
- Track S (Stability): regressions, test harness hardening, marker enforcement, and documentation parity.
- Merge policy: changes land only when both tracks remain green.

## 3. Phased Rollout
1. Contract Freeze
- Freeze `docs/components/final_product/reaper_product_contract_v1.md` as release authority.
- Freeze non-goals to prevent scope explosion.

2. Security-First Redesign
- Harden syscall boundary checks and capability enforcement paths.
- Expand deterministic negative probes for malformed and unauthorized operations.

3. Determinism and Stability
- Keep `verify_matrix` as baseline gate.
- Enforce Day 28-34 closure suites on all redesign candidates.

4. Release Lock
- Require 5 consecutive matrix passes before final declaration.
- Any forbidden marker or missing required marker resets release lock.

5. Cutover
- Remove temporary compatibility paths after lock success.
- Ratify final release only after docs/version/report synchronization.

## 4. Weaknesses and Threats (with Prevention)
1. Scope expansion risk
- Prevention: contract freeze + non-goal freeze + per-slice acceptance criteria.

2. Hidden security regressions
- Prevention: mandatory adversarial negative probes in syscall and capability paths.

3. Performance erosion from instrumentation
- Prevention: explicit budget markers and regression thresholds in gate runs.

4. ABI drift across shared/kernel/user
- Prevention: ABI/header parity checks plus build gate on all touched API changes.

5. Non-deterministic runtime behavior
- Prevention: repeated matrix runs (3 baseline + 5 release lock) and marker enforcement.

6. Documentation drift
- Prevention: release-blocking docs sync checks and checklist ratification.

## 5. Test Program (Release Blocking)
- Build/static: `make -C user`, `make -C kernel`, `make -C kernel iso`.
- Matrix determinism: `make -C kernel verify_matrix`.
- Security-critical closure suites:
  - `./tools/run_day28_closure_suite.sh --runs 3`
  - `./tools/run_day29_closure_suite.sh --runs 3`
  - `./tools/run_day30_closure_suite.sh --runs 3`
  - `./tools/run_day31_closure_suite.sh --runs 3`
  - `./tools/run_day32_closure_suite.sh --runs 3`
  - `./tools/run_day33_closure_suite.sh --runs 3`
  - `./tools/run_day34_closure_suite.sh --runs 3`
- Release lock: `./tools/run_final_release_gate.sh --lock-runs 5`.

## 6. Operator Command
From repo root, run:
- `make -C kernel verify_final_release`

## 7. Progress Note (2026-03-15)
- Slot 1 Step 3 baseline primitives landed in kernel runtime:
  - `rwlock`
  - `seqlock`
  - `rcu`
- Deterministic boot self-test markers were added and matrix revalidation passed (3/3).
- Documentation synchronization completed across report/roadmap/checklist/version artifacts for Day 79.
