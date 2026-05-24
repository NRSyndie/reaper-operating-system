# Final Redesign Checklist (No-Return Release)

## 1. Contract and Scope Freeze
- [ ] Product contract approved: `docs/components/final_product/reaper_product_contract_v1.md`
- [ ] Non-goals frozen for this release window.
- [ ] Compatibility removal milestone defined.

## 2. Security Gates
- [ ] Syscall/capability negative probes pass.
- [ ] Required security markers present.
- [ ] Forbidden failure markers absent.

## 3. Determinism Gates
- [ ] `make -C kernel verify_matrix` passes.
- [ ] Day 28-34 closure suites pass.
- [ ] Release lock (5 runs) passes.

## 4. Performance and Stability Gates
- [ ] No new crash/hang in repeated headless boots.
- [ ] Existing budget markers remain within accepted thresholds.
- [ ] No evidence of marker drift across repeated runs.

## 5. Documentation and Traceability
- [ ] `docs/reports.md` synchronized.
- [ ] `docs/development_log/TODO.rst` synchronized.
- [ ] `docs/development_log/versions.rst` synchronized.
- [ ] `docs/development_log/release_checklist.md` synchronized.
- [ ] Evidence logs and commands recorded in day/final report.

## 6. Final Ratification
- [ ] `make -C kernel verify_final_release` succeeded.
- [ ] Final release candidate accepted without open blocker risks.

## 7. Latest Progress Note (2026-04-29)
- [x] Audit foundation hardening now uses vendored BLAKE3 with `RDRAND`-first root seeding.
- [x] ACPI Layer 1/2 discovery and static parsing landed with boot self-test evidence.
- [x] Architecture/roadmap/report/version docs synchronized with the current daemon model and verified implementation state.
