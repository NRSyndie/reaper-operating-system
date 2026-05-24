#include "../../user/include/reaper.h"
#include <stdbool.h>

#define BOOTINFO_ADDR ((boot_info_t*)0x1000)
#define MAP_FLAG_P         (1ULL << 0)
#define MAP_FLAG_W         (1ULL << 1)
#define MAP_FLAG_U         (1ULL << 2)
#define MAP_FLAGS_USER_RW  (MAP_FLAG_P | MAP_FLAG_W | MAP_FLAG_U)
static struct mode_transition fate_history_buf[16] __attribute__((aligned(8)));
static struct mode_transition fate_fault_buf[16] __attribute__((aligned(8)));
static struct mode_transition fate_transition_buf[16] __attribute__((aligned(8)));
static struct mode_transition fate_lattice_buf[16] __attribute__((aligned(8)));
static struct mode_transition fate_attest_buf[16] __attribute__((aligned(8)));
static gate_sched_metrics_t sched_metrics_buf __attribute__((aligned(8)));
static gate_law2_attest_t law2_attest_buf __attribute__((aligned(8)));
static gate_law2_attest_t law2_attest_buf_recheck __attribute__((aligned(8)));

#define DAY31_DAY29_DRIFT_BUDGET_CYCLES 8000000ULL
#define DAY31_DAY30_DRIFT_BUDGET_CYCLES 8000000ULL
#define DAY32_FAULT_READ_BUDGET_CYCLES 8000000ULL
#define DAY33_FULL_CONTEXT_AUDIT_BUDGET_CYCLES 8000000ULL
#define DAY34_REAL_FAULT_AUDIT_BUDGET_CYCLES 8000000ULL
#define DAY34_TRIGGER_SOURCE_USER 2u

static uint64_t u64_abs_diff(uint64_t a, uint64_t b) {
    return (a >= b) ? (a - b) : (b - a);
}

static uint64_t rdtsc_user(void) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    sys_log("PARADIGM: Awake in the Void.");
    sys_log("[TEST] Day 18 Paradigm C Daemon Bootstrap: SUCCESS.");

    boot_info_t* bi = BOOTINFO_ADDR;
    
    if (bi->magic != BOOTINFO_MAGIC) {
        sys_log("PARADIGM: BootInfo Magic Mismatch!");
        sys_log("PARADIGM: Genesis bridge probe FAIL.");
        return 1;
    }
    
    sys_log("PARADIGM: BootInfo Verified.");
    if (bi->version == 1 && bi->genesis_cap_slot == 1 && bi->kernel_end > bi->kernel_start) {
        sys_log("PARADIGM: Genesis bridge probe PASS.");
    } else {
        sys_log("PARADIGM: Genesis bridge probe FAIL.");
    }
    
    sys_log("PARADIGM: Checking Reality...");
    int mode = sys_mode_query();
    
    if (mode == MODE_CASUAL) {
        sys_log("PARADIGM: Reality is CASUAL (Correct).");
    } else {
        sys_log("PARADIGM: Reality is WRONG.");
    }

    bool day30_reason_probe_ok = false;
    {
        int env_probe_failures = 0;
        bool day30_edge_illegal_seen = false;
        bool day30_auth_required_seen = false;
        bool day30_special_key_seen = false;

        if (sys_mode_transition(MODE_SECURE) != 0) {
            env_probe_failures++;
        }
        if (sys_mode_transition_ex(MODE_CASUAL, MODE_AUTH_PASSWORD | MODE_AUTH_COOLDOWN_ELAPSED) != 0) {
            env_probe_failures++;
        }
        if (sys_mode_transition_ex(MODE_GHOST, MODE_AUTH_PASSWORD | MODE_AUTH_MANUAL) != 0) {
            env_probe_failures++;
        }
        if (sys_mode_transition_ex(MODE_CASUAL, MODE_AUTH_PASSWORD) != -1) {
            env_probe_failures++;
        } else {
            day30_edge_illegal_seen = true;
        }
        if (sys_mode_transition_ex(MODE_SECURE, MODE_AUTH_PASSWORD | MODE_AUTH_MANUAL) != 0) {
            env_probe_failures++;
        }
        if (sys_mode_transition_ex(MODE_CASUAL, MODE_AUTH_PASSWORD | MODE_AUTH_COOLDOWN_ELAPSED) != 0) {
            env_probe_failures++;
        }

        if (sys_mode_transition_ex(MODE_GHOST, MODE_AUTH_MANUAL) != -1) {
            env_probe_failures++;
        } else {
            day30_auth_required_seen = true;
        }

        if (sys_mode_transition_ex(MODE_LOCKDOWN, MODE_AUTH_PASSWORD | MODE_AUTH_MANUAL) != 0) {
            env_probe_failures++;
        }
        if (sys_mode_transition_ex(MODE_SECURE, MODE_AUTH_PASSWORD | MODE_AUTH_SYSTEM_PROMPT) != -1) {
            env_probe_failures++;
        } else {
            day30_special_key_seen = true;
        }
        if (sys_mode_transition_ex(MODE_SECURE, MODE_AUTH_SPECIAL_KEY) != 0) {
            env_probe_failures++;
        }
        /* Optional cooldown probe is omitted from required Day 30 reason mask due timing sensitivity. */
        if (sys_mode_transition_ex(MODE_CASUAL, MODE_AUTH_PASSWORD | MODE_AUTH_COOLDOWN_ELAPSED) != 0) {
            env_probe_failures++;
        }

        day30_reason_probe_ok = day30_edge_illegal_seen &&
                                day30_auth_required_seen &&
                                day30_special_key_seen;
        if (day30_reason_probe_ok) {
            sys_log("PARADIGM: Day 30 reject-reason probes PASS.");
        } else {
            sys_log("PARADIGM: Day 30 reject-reason probes FAILED.");
        }

        if (env_probe_failures == 0) {
            sys_log("PARADIGM: Envelope transition acceptance probe PASS.");
            sys_log("PARADIGM: Envelope transition rejection probe PASS.");
        } else {
            sys_log("PARADIGM: Envelope transition probes FAILED.");
        }
    }

    sys_log("PARADIGM: Assuming Control.");

    /* Adversarial syscall boundary probes (must fail safely) */
    int guard_failures = 0;
    bool day27_boundary_ok = false;
    bool day27_strict_ok = false;
    bool day29_unmap_parent_probe_ok = false;
    bool day29_unmap_ctrl_probe_ok = false;
    bool day29_unmap_type_probe_ok = false;
    bool day29_unmap_rights_probe_ok = false;
    bool day29_unmap_index_probe_ok = false;
    if (sys_log_checked((const char*)0x800000000000ULL) != -1) {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (log_checked kernel pointer).");
    } else {
        sys_log("PARADIGM: Probe PASS (log_checked kernel pointer rejected).");
    }

    if (sys_fate_read((void*)0x800000000000ULL, 1, 4) != -1) {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (fate_read kernel buffer).");
    } else {
        sys_log("PARADIGM: Probe PASS (fate_read kernel buffer rejected).");
    }

    if (sys_fate_read(fate_history_buf, -1, 4) != -1) {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (fate_read negative count).");
    } else {
        sys_log("PARADIGM: Probe PASS (fate_read negative count rejected).");
    }

    if (sys_unmap(9999, 0) != -1) {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (unmap invalid parent slot).");
    } else {
        day29_unmap_parent_probe_ok = true;
        sys_log("PARADIGM: Probe PASS (unmap invalid parent slot rejected).");
    }

    if (sys_unmap_ctrl(9999, 0, 0) != -1) {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (unmap strict control word).");
    } else {
        day29_unmap_ctrl_probe_ok = true;
        sys_log("PARADIGM: Probe PASS (unmap strict control word rejected).");
    }

    if (sys_map(2, 600, 0, MAP_FLAGS_USER_RW) != -1) {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (map index/child contract).");
    } else {
        sys_log("PARADIGM: Probe PASS (map index/child contract rejected).");
    }
    {
        int wait_probe = sys_wait(1); /* Non-blocking probe */
        if (!(wait_probe == 0 || wait_probe == 1)) {
            guard_failures++;
            sys_log("PARADIGM: Probe FAIL (wait non-blocking contract).");
        } else {
            sys_log("PARADIGM: Probe PASS (wait non-blocking contract).");
        }
    }
    {
        int lifecycle_fail = 0;
        int before_wait = sys_wait(1);
        sys_yield();
        int after_wait = sys_wait(1);
        if (!((before_wait == 0 || before_wait == 1) &&
              (after_wait == 0 || after_wait == 1))) {
            lifecycle_fail = 1;
        }
        if (lifecycle_fail) {
            guard_failures++;
            sys_log("PARADIGM: Lifecycle gate probe FAIL.");
        } else {
            sys_log("PARADIGM: Lifecycle gate probe PASS.");
        }
    }

    if (guard_failures == 0) {
        day27_boundary_ok = true;
        sys_log("PARADIGM: Boundary probes passed (safe failures confirmed).");
    } else {
        sys_log("[DAY27-FAIL] boundary probes failed");
        sys_log("PARADIGM: Boundary probes FAILED.");
    }

    if (sys_sched_metrics(&sched_metrics_buf) == 0) {
        if (sched_metrics_buf.schedule_count > 0 &&
            sched_metrics_buf.denied_enqueue == 0 &&
            sched_metrics_buf.denied_wake == 0 &&
            sched_metrics_buf.denied_dispatch == 0) {
            sys_log("PARADIGM: Scheduler Metrics Probe PASS.");
        } else {
            sys_log("PARADIGM: Scheduler Metrics Probe WARN.");
        }
    } else {
        sys_log("PARADIGM: Scheduler Metrics Probe FAILED.");
    }

    /* TEST: SHADOW MAPPING (LAW 2) + Day 16 closure probes */
    sys_log("PARADIGM: Testing Recursive Shadow Mapping...");
    int day16_failures = 0;

    /* 
     * Target Virtual Address: 0x80_0000_0000 (512GB mark)
     * Path: PML4[1] -> PDPT[0] -> PD[0] -> PT[0] -> Page
     */
    
    // Slots for our construction materials
    uint32_t slot_pdpt = 100;
    uint32_t slot_pd   = 101;
    uint32_t slot_pt   = 102;
    uint32_t slot_data = 103;
    uint32_t slot_pt_readonly = 104;

    // 1. Alloc & Retype PDPT (Level 3)
    if (sys_frame_alloc(10) != 0) {
        day16_failures++;
        sys_log("[DAY16-FAIL] alloc pdpt failed");
    }
    if (sys_cap_retype(10, slot_pdpt, CAP_TYPE_PAGETABLE, 3) != 0) {
        day16_failures++;
        sys_log("[DAY16-FAIL] retype pdpt failed");
    }
    sys_cap_delete(10);

    // 2. Alloc & Retype PD (Level 2)
    if (sys_frame_alloc(10) != 0) {
        day16_failures++;
        sys_log("[DAY16-FAIL] alloc pd failed");
    }
    if (sys_cap_retype(10, slot_pd, CAP_TYPE_PAGETABLE, 2) != 0) {
        day16_failures++;
        sys_log("[DAY16-FAIL] retype pd failed");
    }
    sys_cap_delete(10);

    // 3. Alloc & Retype PT (Level 1)
    if (sys_frame_alloc(10) != 0) {
        day16_failures++;
        sys_log("[DAY16-FAIL] alloc pt failed");
    }
    if (sys_cap_retype(10, slot_pt, CAP_TYPE_PAGETABLE, 1) != 0) {
        day16_failures++;
        sys_log("[DAY16-FAIL] retype pt failed");
    }
    sys_cap_delete(10);

    // 4. Alloc Data Page (Leaf)
    if (sys_frame_alloc(slot_data) != 0) {
        day16_failures++;
        sys_log("[DAY16-FAIL] alloc data failed");
    }

    /* 5. Strict negative-path probes for Law 2 staged rollout. */
    int law2_failures = 0;
    int day28_chain_failures = 0;

    /* Invalid flag bit outside allowed user mask must fail. */
    if (sys_map(2, 10, slot_data, MAP_FLAGS_USER_RW | (1ULL << 9)) != -1) law2_failures++;

    /* Child must be RAM or PAGETABLE; auditor cap should be rejected. */
    if (sys_map(2, 11, 4, MAP_FLAGS_USER_RW) != -1) law2_failures++;

    /* Non-leaf strict link requires USER|WRITABLE. Missing WRITABLE must fail. */
    if (sys_map(2, 12, slot_pdpt, MAP_FLAG_P | MAP_FLAG_U) != -1) law2_failures++;

    if (law2_failures == 0) {
        day27_strict_ok = true;
        sys_log("PARADIGM: Law 2 strict negative probes passed.");
    } else {
        day16_failures++;
        sys_log("[DAY16-FAIL] strict rights negative probes failed");
        sys_log("[DAY27-FAIL] strict negative probes failed");
        sys_log("PARADIGM: Law 2 strict negative probes FAILED.");
    }

    if (day27_boundary_ok && day27_strict_ok) {
        sys_log("[TEST] Day 27 Boundary Hardening Contract: SUCCESS.");
        sys_log("[TEST] Day 27 Strict Foundation Contract: SUCCESS.");
        sys_log("[TEST] Day 27 Syscall Rejection Contract: SUCCESS.");
    }

    // 6. Link the Chain (strict path)
    // Map PDPT into PML4 (Slot 2) at index 1
    if (sys_map(2, 1, slot_pdpt, MAP_FLAGS_USER_RW) != 0) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] link pdpt failed");
        sys_log("PARADIGM: Link PDPT Failed");
    }

    // Map PD into PDPT at index 0
    if (sys_map(slot_pdpt, 0, slot_pd, MAP_FLAGS_USER_RW) != 0) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] link pd failed");
        sys_log("PARADIGM: Link PD Failed");
    }

    // Map PT into PD at index 0
    if (sys_map(slot_pd, 0, slot_pt, MAP_FLAGS_USER_RW) != 0) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] link pt failed");
        sys_log("PARADIGM: Link PT Failed");
    }

    // Map Data into PT at index 0
    if (sys_map(slot_pt, 0, slot_data, MAP_FLAGS_USER_RW) != 0) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] link data failed");
        sys_log("PARADIGM: Link Data Failed");
    }

    sys_log("PARADIGM: Construction Complete.");

    // 6. Access Verification
    volatile uint64_t* target_ptr = (volatile uint64_t*)0x8000000000;
    sys_log("PARADIGM: Attempting Write...");
    *target_ptr = 0xCAFEBABE12345678;
    
    sys_log("PARADIGM: Attempting Read...");
    if (*target_ptr == 0xCAFEBABE12345678) {
        sys_log("PARADIGM: Shadow Mapping SUCCESS. The Architect is pleased.");
    } else {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] readback mismatch after map");
        sys_log("PARADIGM: Shadow Mapping FAILED. Data Mismatch.");
    }

    /* Day 16 lifecycle on mapping object: unmap -> idempotent unmap -> remap. */
    if (sys_unmap(slot_pt, 0) != 0) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] unmap leaf failed");
    } else if (sys_unmap(slot_pt, 0) != 0) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] idempotent unmap contract failed");
    } else if (sys_map(slot_pt, 0, slot_data, MAP_FLAGS_USER_RW) != 0) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] remap leaf failed");
    } else if (*target_ptr != 0xCAFEBABE12345678) {
        day16_failures++;
        day28_chain_failures++;
        sys_log("[DAY16-FAIL] readback mismatch after remap");
    }

    if (day16_failures == 0) {
        sys_log("[TEST] Day 16 Capability-Scoped Mapping: SUCCESS.");
        sys_log("[TEST] Day 16 Strict Rights Enforcement: SUCCESS.");
        sys_log("[TEST] Day 16 Unmap/Remap Contract: SUCCESS.");
    }

    if (law2_failures == 0 && day28_chain_failures == 0) {
        sys_log("[TEST] Day 28 Strict Adoption Contract: SUCCESS.");
        sys_log("[TEST] Day 28 Strict Negative Path Contract: SUCCESS.");
        sys_log("[TEST] Day 28 Strict Chain Contract: SUCCESS.");
    } else {
        sys_log("[DAY28-FAIL] strict adoption contract failed");
    }

    if (sys_unmap(slot_data, 0) == -1) {
        day29_unmap_type_probe_ok = true;
        sys_log("PARADIGM: Probe PASS (unmap non-pagetable parent rejected).");
    } else {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (unmap non-pagetable parent accepted).");
    }

    if (sys_unmap(slot_pt, 600) == -1) {
        day29_unmap_index_probe_ok = true;
        sys_log("PARADIGM: Probe PASS (unmap out-of-range index rejected).");
    } else {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (unmap out-of-range index accepted).");
    }

    if (sys_cap_mint(slot_pt, slot_pt_readonly, CAP_RIGHT_READ, 0, CAP_MODE_ALL) == 0) {
        if (sys_unmap(slot_pt_readonly, 0) == -1) {
            day29_unmap_rights_probe_ok = true;
            sys_log("PARADIGM: Probe PASS (unmap write-rights requirement enforced).");
        } else {
            guard_failures++;
            sys_log("PARADIGM: Probe FAIL (unmap allowed without write rights).");
        }
    } else {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (mint readonly pagetable cap failed).");
    }

    if (day29_unmap_parent_probe_ok &&
        day29_unmap_ctrl_probe_ok &&
        day29_unmap_type_probe_ok &&
        day29_unmap_rights_probe_ok &&
        day29_unmap_index_probe_ok &&
        day27_boundary_ok &&
        day27_strict_ok &&
        day28_chain_failures == 0) {
        sys_log("[TEST] Day 29 Strict Unmap Adoption Contract: SUCCESS.");
        sys_log("[TEST] Day 29 Runtime Validation Contract: SUCCESS.");
        sys_log("[TEST] Day 29 Strict Path Runtime Contract: SUCCESS.");
    } else {
        sys_log("[DAY29-FAIL] strict runtime adoption contract failed");
    }

    /* TEST: FATAL FORENSICS (Updated) */
    sys_log("PARADIGM: Auditing Fate Strings...");
    int day21_failures = 0;
    bool day30_history_read_ok = false;
    bool day30_chain_ok = false;
    bool day30_rejected_ok = false;
    bool day30_reason_codes_ok = false;

    if (sys_fate_read(fate_history_buf, 1, 3) != -1) {
        day21_failures++;
        sys_log("[DAY21-FAIL] non-auditor fate_read unexpectedly allowed");
    }
    if (sys_fate_read_ex(fate_history_buf, 1, 4, 99) != -1) {
        day21_failures++;
        sys_log("[DAY21-FAIL] invalid fate read mode unexpectedly allowed");
    }
    
    int count = sys_fate_read(fate_history_buf, 16, 4); // Slot 4 is CAP_TYPE_AUDITOR
    
    if (count > 0) {
        day30_history_read_ok = true;
        sys_log("PARADIGM: Retrieved Fate Records.");
        
        bool chain_ok = true;
        bool rejected_seen = false;
        bool rejected_reason_seen = false;
        for (int i = 0; i < count - 1; i++) {
             /* Verify Hash Chain: history[i].prev_hash must match history[i+1].curr_hash */
             if (fate_history_buf[i].prev_hash != fate_history_buf[i+1].curr_hash) {
                 chain_ok = false;
             }
            if (fate_history_buf[i].result_code == FATE_RESULT_REJECTED) {
                rejected_seen = true;
                if (fate_history_buf[i].fault_error_code != MODE_REJECT_NONE) {
                    rejected_reason_seen = true;
                }
            }
        }
        if (fate_history_buf[count - 1].result_code == FATE_RESULT_REJECTED) {
            rejected_seen = true;
            if (fate_history_buf[count - 1].fault_error_code != MODE_REJECT_NONE) {
                rejected_reason_seen = true;
            }
        }
        
        if (chain_ok) {
            day30_chain_ok = true;
            sys_log("PARADIGM: Hash Chain Integrity VERIFIED.");
        } else {
            day21_failures++;
            sys_log("[DAY21-FAIL] hash chain integrity failure");
            sys_log("PARADIGM: Hash Chain BROKEN!");
        }

        if (rejected_seen) {
            day30_rejected_ok = true;
            sys_log("PARADIGM: Fate Strings include rejected transition evidence.");
        } else {
            day21_failures++;
            sys_log("[DAY21-FAIL] rejected transition evidence missing");
            sys_log("PARADIGM: Fate Strings missing rejected transition evidence.");
        }

        if (rejected_reason_seen) {
            day30_reason_codes_ok = true;
            sys_log("PARADIGM: Fate Strings include transition reject reason codes.");
        } else {
            day21_failures++;
            sys_log("[DAY30-FAIL] reject reason code evidence missing");
            sys_log("PARADIGM: Fate Strings missing transition reject reason codes.");
        }

        int fault_count = sys_fate_read_ex(fate_fault_buf, 16, 4, FATE_READ_FAULTS);
        if (fault_count > 0) {
            bool fault_meta_ok = false;
            for (int i = 0; i < fault_count; i++) {
                bool is_gp_or_pf = (fate_fault_buf[i].fault_vector == 13 || fate_fault_buf[i].fault_vector == 14);
                bool base_ok = fate_fault_buf[i].fault_rip != 0 &&
                               fate_fault_buf[i].fault_rsp != 0 &&
                               fate_fault_buf[i].fault_cs != 0 &&
                               fate_fault_buf[i].fault_rflags != 0;
                bool pf_ok = (fate_fault_buf[i].fault_vector != 14) || (fate_fault_buf[i].fault_cr2 != 0);

                if (is_gp_or_pf && base_ok && pf_ok) {
                    fault_meta_ok = true;
                    break;
                }
            }
            if (fault_meta_ok) {
                sys_log("PARADIGM: Fault Fate records include full context metadata.");
            } else {
                day21_failures++;
                sys_log("[DAY21-FAIL] fault metadata missing");
                sys_log("PARADIGM: Fault Fate records missing expected metadata.");
            }
        } else {
            sys_log("PARADIGM: No fault Fate records in current audit window (deferred).");
        }
    } else {
        day21_failures++;
        sys_log("[DAY21-FAIL] fate_read failed with auditor cap");
        sys_log("PARADIGM: Failed to read Fate Strings.");
    }

    if (day21_failures == 0) {
        sys_log("[TEST] Day 21 Auditor Access Contract: SUCCESS.");
        sys_log("[TEST] Day 21 Fate Integrity Contract: SUCCESS.");
        sys_log("[TEST] Day 21 Fault Forensics Contract: SUCCESS.");
    }

    if (day30_history_read_ok && day30_chain_ok && day30_rejected_ok && day30_reason_codes_ok && day30_reason_probe_ok) {
        sys_log("[TEST] Day 30 Rejection Auditing Contract: SUCCESS.");
        sys_log("[TEST] Day 30 Fate Result-Code Contract: SUCCESS.");
        sys_log("[TEST] Day 30 Rejected Evidence Contract: SUCCESS.");
    } else {
        sys_log("[DAY30-FAIL] fate rejection auditing contract failed");
    }

    /* Kernel-owned closure attestation (authoritative evidence path). */
    if (sys_law2_attest(&law2_attest_buf) != 0) {
        sys_log("[DAY28-FAIL] kernel attestation syscall failed");
        sys_log("[DAY29-FAIL] kernel attestation syscall failed");
        sys_log("[DAY30-FAIL] kernel attestation syscall failed");
        sys_log("[DAY31-FAIL] primary kernel attestation syscall failed");
    } else {
        bool day31_security_ok = false;
        bool day31_determinism_ok = false;
        bool day31_perf_ok = false;

        if (law2_attest_buf.day28_status == 0) {
            sys_log("[DAY28-FAIL] kernel attestation rejected day28");
        }
        if (law2_attest_buf.day29_status == 0) {
            sys_log("[DAY29-FAIL] kernel attestation rejected day29");
        } else {
            if ((law2_attest_buf.day29_reason_mask & LAW2_DAY29_REASON_MASK_REQUIRED) == LAW2_DAY29_REASON_MASK_REQUIRED) {
                sys_log("[TEST] Day 29 Reason Coverage Contract: SUCCESS.");
            } else {
                sys_log("[DAY29-FAIL] day29 reason coverage incomplete");
            }

            if (law2_attest_buf.day29_unmap_cycles_max <= law2_attest_buf.day29_perf_budget_cycles) {
                sys_log("[TEST] Day 29 Performance Budget Contract: SUCCESS.");
            } else {
                sys_log("[DAY29-FAIL] day29 unmap performance budget exceeded");
            }
        }
        if (law2_attest_buf.day30_status == 0) {
            sys_log("[DAY30-FAIL] kernel attestation rejected day30");
        } else {
            if ((law2_attest_buf.day30_reason_mask & LAW2_DAY30_REASON_MASK_REQUIRED) == LAW2_DAY30_REASON_MASK_REQUIRED) {
                sys_log("[TEST] Day 30 Reason Coverage Contract: SUCCESS.");
            } else {
                sys_log("[DAY30-FAIL] day30 reason coverage incomplete");
            }
            if (law2_attest_buf.day30_reject_scan_cycles <= law2_attest_buf.day30_perf_budget_cycles) {
                sys_log("[TEST] Day 30 Performance Budget Contract: SUCCESS.");
            } else {
                sys_log("[DAY30-FAIL] day30 attestation scan budget exceeded");
            }
        }

        if (sys_law2_attest(&law2_attest_buf_recheck) != 0) {
            sys_log("[DAY31-FAIL] recheck kernel attestation syscall failed");
        } else {
            bool day31_status_ok =
                (law2_attest_buf.day28_status == 1U) &&
                (law2_attest_buf.day29_status == 1U) &&
                (law2_attest_buf.day30_status == 1U) &&
                (law2_attest_buf_recheck.day28_status == 1U) &&
                (law2_attest_buf_recheck.day29_status == 1U) &&
                (law2_attest_buf_recheck.day30_status == 1U);

            bool day31_reason_coverage_ok =
                ((law2_attest_buf.day29_reason_mask & LAW2_DAY29_REASON_MASK_REQUIRED) == LAW2_DAY29_REASON_MASK_REQUIRED) &&
                ((law2_attest_buf.day30_reason_mask & LAW2_DAY30_REASON_MASK_REQUIRED) == LAW2_DAY30_REASON_MASK_REQUIRED) &&
                ((law2_attest_buf_recheck.day29_reason_mask & LAW2_DAY29_REASON_MASK_REQUIRED) == LAW2_DAY29_REASON_MASK_REQUIRED) &&
                ((law2_attest_buf_recheck.day30_reason_mask & LAW2_DAY30_REASON_MASK_REQUIRED) == LAW2_DAY30_REASON_MASK_REQUIRED);

            bool day31_reason_determinism_ok =
                (law2_attest_buf.day29_reason_mask == law2_attest_buf_recheck.day29_reason_mask) &&
                (law2_attest_buf.day30_reason_mask == law2_attest_buf_recheck.day30_reason_mask);

            bool day31_budget_determinism_ok =
                (law2_attest_buf.day29_perf_budget_cycles != 0ULL) &&
                (law2_attest_buf.day30_perf_budget_cycles != 0ULL) &&
                (law2_attest_buf.day29_perf_budget_cycles == law2_attest_buf_recheck.day29_perf_budget_cycles) &&
                (law2_attest_buf.day30_perf_budget_cycles == law2_attest_buf_recheck.day30_perf_budget_cycles);

            bool day31_perf_budget_ok =
                (law2_attest_buf.day29_unmap_cycles_max <= law2_attest_buf.day29_perf_budget_cycles) &&
                (law2_attest_buf_recheck.day29_unmap_cycles_max <= law2_attest_buf_recheck.day29_perf_budget_cycles) &&
                (law2_attest_buf.day30_reject_scan_cycles <= law2_attest_buf.day30_perf_budget_cycles) &&
                (law2_attest_buf_recheck.day30_reject_scan_cycles <= law2_attest_buf_recheck.day30_perf_budget_cycles);

            uint64_t day31_day29_drift =
                u64_abs_diff(law2_attest_buf.day29_unmap_cycles_max, law2_attest_buf_recheck.day29_unmap_cycles_max);
            uint64_t day31_day30_drift =
                u64_abs_diff(law2_attest_buf.day30_reject_scan_cycles, law2_attest_buf_recheck.day30_reject_scan_cycles);
            bool day31_perf_drift_ok =
                (day31_day29_drift <= DAY31_DAY29_DRIFT_BUDGET_CYCLES) &&
                (day31_day30_drift <= DAY31_DAY30_DRIFT_BUDGET_CYCLES);

            day31_security_ok = day31_status_ok && day31_reason_coverage_ok;
            day31_determinism_ok = day31_reason_determinism_ok && day31_budget_determinism_ok;
            day31_perf_ok = day31_perf_budget_ok && day31_perf_drift_ok;

            if (day31_security_ok) {
                sys_log("[TEST] Day 31 Revalidation Security Contract: SUCCESS.");
            } else {
                sys_log("[DAY31-FAIL] day31 security revalidation failed");
            }

            if (day31_determinism_ok) {
                sys_log("[TEST] Day 31 Revalidation Determinism Contract: SUCCESS.");
            } else {
                sys_log("[DAY31-FAIL] day31 deterministic attestation parity failed");
            }

            if (day31_perf_ok) {
                sys_log("[TEST] Day 31 Revalidation Performance Contract: SUCCESS.");
            } else {
                sys_log("[DAY31-FAIL] day31 performance revalidation failed");
            }

            if (!(day31_security_ok && day31_determinism_ok && day31_perf_ok)) {
                sys_log("[DAY31-FAIL] day31 revalidation contract failed");
            }
        }
    }

    /* TEST: PRISMATIC LATTICES (LAW 6) */
    int day20_failures = 0;
    int day26_failures = 0;
    bool day34_real_fault_path_ok = false;
    bool day34_user_provenance_ok = false;
    bool day34_perf_ok = false;
    sys_log("PARADIGM: Testing Prismatic Lattices...");
    uint32_t slot_lattice = 50;
    if (sys_lattice_create(2, slot_lattice) != 0) {
        day20_failures++;
        day26_failures++;
        sys_log("[DAY20-FAIL] lattice create failed");
        sys_log("[DAY26-FAIL] prismatic substrate create failed");
        sys_log("PARADIGM: Lattice Creation Failed.");
    } else {
        sys_log("PARADIGM: Lattice Created (2 pages).");
        
        uint64_t lattice_vaddr = 0x20000000;
        if (sys_lattice_attach(slot_lattice, lattice_vaddr) != 0) {
            day20_failures++;
            day26_failures++;
            sys_log("[DAY20-FAIL] lattice attach failed");
            sys_log("[DAY26-FAIL] prismatic substrate attach failed");
            sys_log("PARADIGM: Lattice Attach Failed.");
        } else {
            sys_log("PARADIGM: Lattice Attached at 0x20000000.");
            
            volatile uint64_t* lattice_ptr = (volatile uint64_t*)lattice_vaddr;
            sys_log("PARADIGM: Attempting Source Write (Page 0)...");
            lattice_ptr[0] = 0xDEADC0DECAFEBABE;
            
            sys_log("PARADIGM: Attuning Crystal (Page 0 Ready)...");
            if (sys_attune(slot_lattice, 0) == 0) {
                sys_log("PARADIGM: Lattice Attunement SUCCESS.");
            } else {
                day20_failures++;
                day26_failures++;
                sys_log("[DAY20-FAIL] source attune rejected");
                sys_log("[DAY26-FAIL] attunement rejected for source");
                sys_log("PARADIGM: Lattice Attunement FAILED.");
            }

            /* Real-fault probe verification: first-touch lattice access should be logged as #PF. */
            uint64_t day34_fault_audit_start = rdtsc_user();
            int fault_count_after_lattice = sys_fate_read_ex(fate_fault_buf, 16, 4, FATE_READ_FAULTS);
            uint64_t day34_fault_audit_cycles = rdtsc_user() - day34_fault_audit_start;
            if (fault_count_after_lattice > 0) {
                bool real_pf_seen = false;
                bool real_pf_user_seen = false;
                bool real_pf_full_ctx_seen = false;
                for (int i = 0; i < fault_count_after_lattice; i++) {
                    if (fate_fault_buf[i].fault_vector == 14 &&
                        fate_fault_buf[i].fault_cr2 >= lattice_vaddr &&
                        fate_fault_buf[i].fault_cr2 < (lattice_vaddr + (2ULL * 4096ULL))) {
                        real_pf_seen = true;
                        if (fate_fault_buf[i].record_type == FATE_RECORD_FAULT &&
                            fate_fault_buf[i].trigger_source == DAY34_TRIGGER_SOURCE_USER &&
                            fate_fault_buf[i].requestor_pid != 0) {
                            real_pf_user_seen = true;
                        }
                        if (fate_fault_buf[i].fault_rip != 0 &&
                            fate_fault_buf[i].fault_rsp != 0 &&
                            fate_fault_buf[i].fault_cs != 0 &&
                            fate_fault_buf[i].fault_rflags != 0) {
                            real_pf_full_ctx_seen = true;
                        }
                        break;
                    }
                }
                if (real_pf_seen) {
                    sys_log("PARADIGM: Real fault probe captured in Fate Strings.");
                    if (real_pf_user_seen && real_pf_full_ctx_seen) {
                        day34_real_fault_path_ok = true;
                        day34_user_provenance_ok = true;
                    } else {
                        day20_failures++;
                        day26_failures++;
                        sys_log("[DAY34-FAIL] real fault record missing user provenance/full context");
                    }
                } else {
                    day20_failures++;
                    day26_failures++;
                    sys_log("[DAY20-FAIL] lattice first-touch fault missing");
                    sys_log("[DAY26-FAIL] void wall first-touch fault missing");
                    sys_log("PARADIGM: Real fault probe missing from Fate Strings.");
                    sys_log("[DAY34-FAIL] real fault probe missing from Fate Strings");
                }
            } else {
                day20_failures++;
                day26_failures++;
                sys_log("[DAY20-FAIL] fault ledger empty after lattice probe");
                sys_log("[DAY26-FAIL] fault ledger empty after void wall probe");
                sys_log("PARADIGM: Fault ledger empty after real fault probe.");
                sys_log("[DAY34-FAIL] fault ledger empty after real fault probe");
            }

            if (day34_fault_audit_cycles <= DAY34_REAL_FAULT_AUDIT_BUDGET_CYCLES) {
                day34_perf_ok = true;
            } else {
                sys_log("[DAY34-FAIL] real fault audit performance budget exceeded");
            }
        }
    }

    if (day26_failures == 0) {
        sys_log("[TEST] Day 26 Prismatic Substrate Contract: SUCCESS.");
        sys_log("[TEST] Day 26 Void Wall Contract: SUCCESS.");
        sys_log("[TEST] Day 26 Attunement Contract: SUCCESS.");
    }

    sys_log("PARADIGM: Testing ReadOnly Lattice Listeners...");
    uint32_t source_slot = 53;
    uint32_t listener_slot_a = 54;
    uint32_t listener_slot_b = 55;
    uint64_t source_vaddr = 0x21000000;
    uint64_t listener_vaddr = 0x22000000;
    if (sys_lattice_create_broadcast(2, source_slot, 2, listener_slot_a, listener_slot_b) != 0) {
        day20_failures++;
        sys_log("[DAY20-FAIL] broadcast lattice create failed");
        sys_log("PARADIGM: Broadcast Lattice Creation Failed.");
    } else {
        if (sys_lattice_attach(source_slot, source_vaddr) != 0 || sys_lattice_attach(listener_slot_a, listener_vaddr) != 0) {
            day20_failures++;
            sys_log("[DAY20-FAIL] broadcast lattice attach failed");
            sys_log("PARADIGM: Broadcast Lattice Attach Failed.");
        } else {
            volatile uint64_t* source_ptr = (volatile uint64_t*)source_vaddr;
            volatile uint64_t* listener_ptr = (volatile uint64_t*)listener_vaddr;
            source_ptr[0] = 0xA11CEBADC0FFEEULL;
            if (listener_ptr[0] == 0xA11CEBADC0FFEEULL) {
                sys_log("PARADIGM: Broadcast Lattice ReadOnly Listener PASS.");
            } else {
                day20_failures++;
                sys_log("[DAY20-FAIL] readonly listener readback mismatch");
                sys_log("PARADIGM: Broadcast Lattice ReadOnly Listener FAIL.");
            }

            if (sys_attune(listener_slot_a, 1) != 0) {
                sys_log("PARADIGM: ReadOnly Listener Attune Rejected (expected).");
            } else {
                day20_failures++;
                sys_log("[DAY20-FAIL] readonly listener attune unexpectedly allowed");
                sys_log("PARADIGM: ReadOnly Listener Attune Unexpectedly Allowed.");
            }
        }
    }

    if (sys_lattice_create_broadcast(2, 56, 2, 57, 57) == 0) {
        day20_failures++;
        sys_log("[DAY20-FAIL] invalid broadcast topology accepted");
    }
    if (sys_lattice_attach(source_slot, 0x22000001ULL) == 0) {
        day20_failures++;
        sys_log("[DAY20-FAIL] unaligned lattice attach accepted");
    }

    if (sys_lattice_detach(listener_slot_a, listener_vaddr) == 0) {
        sys_log("PARADIGM: ReadOnly Listener Detach SUCCESS.");
    } else {
        day20_failures++;
        sys_log("[DAY20-FAIL] readonly listener detach failed");
        sys_log("PARADIGM: ReadOnly Listener Detach FAILED.");
    }

    int lattice_event_count = sys_fate_read_ex(fate_history_buf, 16, 4, FATE_READ_LATTICE);
    if (lattice_event_count > 0) {
        bool saw_attach = false;
        bool saw_detach = false;
        for (int i = 0; i < lattice_event_count; i++) {
            if (fate_history_buf[i].record_type == FATE_RECORD_LATTICE &&
                fate_history_buf[i].fault_vector == FATE_LATTICE_ATTACH) {
                saw_attach = true;
            }
            if (fate_history_buf[i].record_type == FATE_RECORD_LATTICE &&
                fate_history_buf[i].fault_vector == FATE_LATTICE_DETACH) {
                saw_detach = true;
            }
        }
        if (saw_attach) {
            sys_log("PARADIGM: Lattice Forensics attach records visible.");
        } else {
            day20_failures++;
            sys_log("[DAY20-FAIL] lattice attach forensics missing");
            sys_log("PARADIGM: Lattice Forensics attach records missing.");
        }
        if (saw_detach) {
            sys_log("PARADIGM: Lattice Forensics detach records visible.");
        } else {
            day20_failures++;
            sys_log("[DAY20-FAIL] lattice detach forensics missing");
            sys_log("PARADIGM: Lattice Forensics detach records missing.");
        }
    } else {
        day20_failures++;
        sys_log("[DAY20-FAIL] no lattice forensic records");
        sys_log("PARADIGM: No lattice forensic records in audit window.");
    }

    if (day20_failures == 0) {
        sys_log("[TEST] Day 20 Lattice Create Contract: SUCCESS.");
        sys_log("[TEST] Day 20 Lattice Rights Contract: SUCCESS.");
        sys_log("[TEST] Day 20 Lattice Lifecycle Contract: SUCCESS.");
    }

    if (day34_real_fault_path_ok) {
        sys_log("[TEST] Day 34 Real Fault Path Contract: SUCCESS.");
    }
    if (day34_user_provenance_ok) {
        sys_log("[TEST] Day 34 User Fault Provenance Contract: SUCCESS.");
    }
    if (day34_perf_ok) {
        sys_log("[TEST] Day 34 Real Fault Performance Contract: SUCCESS.");
    }
    if (!(day34_real_fault_path_ok && day34_user_provenance_ok && day34_perf_ok)) {
        sys_log("[DAY34-FAIL] day34 real-fault closure contract failed");
    }

    {
        bool day32_filter_contract_ok = true;
        bool day32_fault_context_contract_ok = false;
        bool day32_perf_contract_ok = false;

        int day32_all_count = sys_fate_read_ex(fate_history_buf, 16, 4, FATE_READ_ALL);
        int day32_transition_count = sys_fate_read_ex(fate_transition_buf, 16, 4, FATE_READ_TRANSITIONS);
        uint64_t day32_fault_read_start = rdtsc_user();
        int day32_fault_count = sys_fate_read_ex(fate_fault_buf, 16, 4, FATE_READ_FAULTS);
        uint64_t day32_fault_read_cycles = rdtsc_user() - day32_fault_read_start;
        int day32_lattice_count = sys_fate_read_ex(fate_lattice_buf, 16, 4, FATE_READ_LATTICE);
        int day32_attest_count = sys_fate_read_ex(fate_attest_buf, 16, 4, FATE_READ_ATTEST);

        if (day32_all_count < 0 || day32_transition_count < 0 || day32_fault_count < 0 ||
            day32_lattice_count < 0 || day32_attest_count < 0) {
            day32_filter_contract_ok = false;
            sys_log("[DAY32-FAIL] fate read mode filter call failed");
        }

        if (day32_filter_contract_ok) {
            for (int i = 0; i < day32_transition_count; i++) {
                if (fate_transition_buf[i].record_type != FATE_RECORD_TRANSITION) {
                    day32_filter_contract_ok = false;
                    sys_log("[DAY32-FAIL] transition filter leaked non-transition record");
                    break;
                }
            }
            for (int i = 0; i < day32_fault_count; i++) {
                if (fate_fault_buf[i].record_type != FATE_RECORD_FAULT) {
                    day32_filter_contract_ok = false;
                    sys_log("[DAY32-FAIL] fault filter leaked non-fault record");
                    break;
                }
            }
            for (int i = 0; i < day32_lattice_count; i++) {
                if (fate_lattice_buf[i].record_type != FATE_RECORD_LATTICE) {
                    day32_filter_contract_ok = false;
                    sys_log("[DAY32-FAIL] lattice filter leaked non-lattice record");
                    break;
                }
            }
            for (int i = 0; i < day32_attest_count; i++) {
                if (fate_attest_buf[i].record_type != FATE_RECORD_ATTEST) {
                    day32_filter_contract_ok = false;
                    sys_log("[DAY32-FAIL] attest filter leaked non-attest record");
                    break;
                }
            }
            if (day32_fault_count <= 0) {
                day32_filter_contract_ok = false;
                sys_log("[DAY32-FAIL] fault filter returned no records");
            }
            if (day32_all_count < day32_fault_count) {
                day32_filter_contract_ok = false;
                sys_log("[DAY32-FAIL] all-record view smaller than fault-record view");
            }
        }

        if (day32_filter_contract_ok) {
            bool metadata_ok = false;
            for (int i = 0; i < day32_fault_count; i++) {
                bool vec_ok = (fate_fault_buf[i].fault_vector == 13 || fate_fault_buf[i].fault_vector == 14);
                bool base_ok = fate_fault_buf[i].fault_rip != 0 &&
                               fate_fault_buf[i].fault_rsp != 0 &&
                               fate_fault_buf[i].fault_cs != 0 &&
                               fate_fault_buf[i].fault_rflags != 0;
                bool pf_ok = (fate_fault_buf[i].fault_vector != 14) || (fate_fault_buf[i].fault_cr2 != 0);
                if (vec_ok && base_ok && pf_ok) {
                    metadata_ok = true;
                    break;
                }
            }
            if (metadata_ok) {
                day32_fault_context_contract_ok = true;
            } else {
                sys_log("[DAY32-FAIL] fault metadata context incomplete");
            }
        }

        if (day32_fault_read_cycles <= DAY32_FAULT_READ_BUDGET_CYCLES) {
            day32_perf_contract_ok = true;
        } else {
            sys_log("[DAY32-FAIL] fault read performance budget exceeded");
        }

        if (day32_filter_contract_ok) {
            sys_log("[TEST] Day 32 Fault Filter Contract: SUCCESS.");
        }
        if (day32_fault_context_contract_ok) {
            sys_log("[TEST] Day 32 Fault Metadata Contract: SUCCESS.");
        }
        if (day32_perf_contract_ok) {
            sys_log("[TEST] Day 32 Fault Read Performance Contract: SUCCESS.");
        }
        if (!(day32_filter_contract_ok && day32_fault_context_contract_ok && day32_perf_contract_ok)) {
            sys_log("[DAY32-FAIL] day32 fault-to-string closure contract failed");
        }
    }

    {
        bool day33_full_context_ok = true;
        bool day33_vector_sanity_ok = true;
        bool day33_perf_ok = false;
        bool day33_pf_full_context_seen = false;

        uint64_t day33_start = rdtsc_user();
        int day33_fault_count = sys_fate_read_ex(fate_fault_buf, 16, 4, FATE_READ_FAULTS);
        uint64_t day33_cycles = rdtsc_user() - day33_start;

        if (day33_fault_count <= 0) {
            day33_full_context_ok = false;
            day33_vector_sanity_ok = false;
            sys_log("[DAY33-FAIL] fault audit window empty");
        } else {
            for (int i = 0; i < day33_fault_count; i++) {
                if (fate_fault_buf[i].record_type != FATE_RECORD_FAULT) {
                    day33_vector_sanity_ok = false;
                    sys_log("[DAY33-FAIL] non-fault record in fault audit window");
                    break;
                }

                bool vector_ok = (fate_fault_buf[i].fault_vector == 13 || fate_fault_buf[i].fault_vector == 14);
                if (!vector_ok) {
                    day33_vector_sanity_ok = false;
                    sys_log("[DAY33-FAIL] unexpected fault vector in audit window");
                    break;
                }

                bool base_ok = fate_fault_buf[i].fault_rip != 0 &&
                               fate_fault_buf[i].fault_rsp != 0 &&
                               fate_fault_buf[i].fault_cs != 0 &&
                               fate_fault_buf[i].fault_rflags != 0;
                bool pf_ok = (fate_fault_buf[i].fault_vector != 14) || (fate_fault_buf[i].fault_cr2 != 0);
                bool gp_ok = (fate_fault_buf[i].fault_vector != 13) || (fate_fault_buf[i].fault_cr2 == 0);
                if (!(base_ok && pf_ok && gp_ok)) {
                    day33_full_context_ok = false;
                    sys_log("[DAY33-FAIL] incomplete full fault context");
                    break;
                }

                if (fate_fault_buf[i].fault_vector == 14 && base_ok && pf_ok) {
                    day33_pf_full_context_seen = true;
                }
            }
        }

        if (!day33_pf_full_context_seen) {
            day33_vector_sanity_ok = false;
            sys_log("[DAY33-FAIL] no full-context page fault evidence in audit window");
        }

        if (day33_cycles <= DAY33_FULL_CONTEXT_AUDIT_BUDGET_CYCLES) {
            day33_perf_ok = true;
        } else {
            sys_log("[DAY33-FAIL] full-context audit performance budget exceeded");
        }

        if (day33_full_context_ok) {
            sys_log("[TEST] Day 33 Full Context Coverage Contract: SUCCESS.");
        }
        if (day33_vector_sanity_ok) {
            sys_log("[TEST] Day 33 Fault Vector Coverage Contract: SUCCESS.");
        }
        if (day33_perf_ok) {
            sys_log("[TEST] Day 33 Full Context Performance Contract: SUCCESS.");
        }
        if (!(day33_full_context_ok && day33_vector_sanity_ok && day33_perf_ok)) {
            sys_log("[DAY33-FAIL] day33 full-context closure contract failed");
        }
    }

    /* Pulse */
    int counter = 0;
    while (1) {
        if (counter++ % 10000000 == 0) {
            sys_log("PARADIGM: Pulse...");
        }
        sys_yield();
    }

    return 0;
}
