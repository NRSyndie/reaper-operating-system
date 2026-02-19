#include "../../user/include/reaper.h"
#include <stdbool.h>

#define BOOTINFO_ADDR ((boot_info_t*)0x1000)
#define MAP_FLAG_P         (1ULL << 0)
#define MAP_FLAG_W         (1ULL << 1)
#define MAP_FLAG_U         (1ULL << 2)
#define MAP_FLAGS_USER_RW  (MAP_FLAG_P | MAP_FLAG_W | MAP_FLAG_U)
static struct mode_transition fate_history_buf[16] __attribute__((aligned(8)));
static struct mode_transition fate_fault_buf[16] __attribute__((aligned(8)));

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    sys_log("PARADIGM: Awake in the Void.");

    boot_info_t* bi = BOOTINFO_ADDR;
    
    if (bi->magic != BOOTINFO_MAGIC) {
        sys_log("PARADIGM: BootInfo Magic Mismatch!");
        return 1;
    }
    
    sys_log("PARADIGM: BootInfo Verified.");
    
    sys_log("PARADIGM: Checking Reality...");
    int mode = sys_mode_query();
    
    if (mode == MODE_CASUAL) {
        sys_log("PARADIGM: Reality is CASUAL (Correct).");
    } else {
        sys_log("PARADIGM: Reality is WRONG.");
    }

    sys_log("PARADIGM: Assuming Control.");

    /* Adversarial syscall boundary probes (must fail safely) */
    int guard_failures = 0;
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

    if (sys_unmap_strict(9999, 0) != -1) {
        guard_failures++;
        sys_log("PARADIGM: Probe FAIL (unmap invalid parent slot).");
    } else {
        sys_log("PARADIGM: Probe PASS (unmap invalid parent slot rejected).");
    }

    if (sys_map_strict(2, 600, 0, MAP_FLAGS_USER_RW) != -1) {
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

    if (guard_failures == 0) {
        sys_log("PARADIGM: Boundary probes passed (safe failures confirmed).");
    } else {
        sys_log("PARADIGM: Boundary probes FAILED.");
    }

    /* TEST: SHADOW MAPPING (LAW 2) - THE ARCHITECT'S PATH */
    sys_log("PARADIGM: Testing Recursive Shadow Mapping...");

    /* 
     * Target Virtual Address: 0x80_0000_0000 (512GB mark)
     * Path: PML4[1] -> PDPT[0] -> PD[0] -> PT[0] -> Page
     */
    
    // Slots for our construction materials
    uint32_t slot_pdpt = 100;
    uint32_t slot_pd   = 101;
    uint32_t slot_pt   = 102;
    uint32_t slot_data = 103;

    // 1. Alloc & Retype PDPT (Level 3)
    if (sys_frame_alloc(10) != 0) sys_log("PARADIGM: Alloc PDPT Failed");
    if (sys_cap_retype(10, slot_pdpt, CAP_TYPE_PAGETABLE, 3) != 0) sys_log("PARADIGM: Retype PDPT Failed");
    sys_cap_delete(10);

    // 2. Alloc & Retype PD (Level 2)
    if (sys_frame_alloc(10) != 0) sys_log("PARADIGM: Alloc PD Failed");
    if (sys_cap_retype(10, slot_pd, CAP_TYPE_PAGETABLE, 2) != 0) sys_log("PARADIGM: Retype PD Failed");
    sys_cap_delete(10);

    // 3. Alloc & Retype PT (Level 1)
    if (sys_frame_alloc(10) != 0) sys_log("PARADIGM: Alloc PT Failed");
    if (sys_cap_retype(10, slot_pt, CAP_TYPE_PAGETABLE, 1) != 0) sys_log("PARADIGM: Retype PT Failed");
    sys_cap_delete(10);

    // 4. Alloc Data Page (Leaf)
    if (sys_frame_alloc(slot_data) != 0) sys_log("PARADIGM: Alloc Data Failed");

    /* 5. Strict negative-path probes for Law 2 staged rollout. */
    int law2_failures = 0;

    /* Invalid flag bit outside allowed user mask must fail. */
    if (sys_map_strict(2, 10, slot_data, MAP_FLAGS_USER_RW | (1ULL << 9)) != -1) law2_failures++;

    /* Child must be RAM or PAGETABLE; auditor cap should be rejected. */
    if (sys_map_strict(2, 11, 4, MAP_FLAGS_USER_RW) != -1) law2_failures++;

    /* Non-leaf strict link requires USER|WRITABLE. Missing WRITABLE must fail. */
    if (sys_map_strict(2, 12, slot_pdpt, MAP_FLAG_P | MAP_FLAG_U) != -1) law2_failures++;

    if (law2_failures == 0) {
        sys_log("PARADIGM: Law 2 strict negative probes passed.");
    } else {
        sys_log("PARADIGM: Law 2 strict negative probes FAILED.");
    }

    // 6. Link the Chain (strict path)
    // Map PDPT into PML4 (Slot 2) at index 1
    if (sys_map_strict(2, 1, slot_pdpt, MAP_FLAGS_USER_RW) != 0) sys_log("PARADIGM: Link PDPT Failed");

    // Map PD into PDPT at index 0
    if (sys_map_strict(slot_pdpt, 0, slot_pd, MAP_FLAGS_USER_RW) != 0) sys_log("PARADIGM: Link PD Failed");

    // Map PT into PD at index 0
    if (sys_map_strict(slot_pd, 0, slot_pt, MAP_FLAGS_USER_RW) != 0) sys_log("PARADIGM: Link PT Failed");

    // Map Data into PT at index 0
    if (sys_map_strict(slot_pt, 0, slot_data, MAP_FLAGS_USER_RW) != 0) sys_log("PARADIGM: Link Data Failed");

    sys_log("PARADIGM: Construction Complete.");

    // 6. Access Verification
    volatile uint64_t* target_ptr = (volatile uint64_t*)0x8000000000;
    sys_log("PARADIGM: Attempting Write...");
    *target_ptr = 0xCAFEBABE12345678;
    
    sys_log("PARADIGM: Attempting Read...");
    if (*target_ptr == 0xCAFEBABE12345678) {
        sys_log("PARADIGM: Shadow Mapping SUCCESS. The Architect is pleased.");
    } else {
        sys_log("PARADIGM: Shadow Mapping FAILED. Data Mismatch.");
    }

    /* TEST: FATAL FORENSICS (Updated) */
    sys_log("PARADIGM: Auditing Fate Strings...");
    
    int count = sys_fate_read(fate_history_buf, 16, 4); // Slot 4 is CAP_TYPE_AUDITOR
    
    if (count > 0) {
        sys_log("PARADIGM: Retrieved Fate Records.");
        
        bool chain_ok = true;
        bool rejected_seen = false;
        for (int i = 0; i < count - 1; i++) {
             /* Verify Hash Chain: history[i].prev_hash must match history[i+1].curr_hash */
             if (fate_history_buf[i].prev_hash != fate_history_buf[i+1].curr_hash) {
                 chain_ok = false;
             }
            if (fate_history_buf[i].result_code == FATE_RESULT_REJECTED) {
                rejected_seen = true;
            }
        }
        if (fate_history_buf[count - 1].result_code == FATE_RESULT_REJECTED) {
            rejected_seen = true;
        }
        
        if (chain_ok) {
            sys_log("PARADIGM: Hash Chain Integrity VERIFIED.");
        } else {
            sys_log("PARADIGM: Hash Chain BROKEN!");
        }

        if (rejected_seen) {
            sys_log("PARADIGM: Fate Strings include rejected transition evidence.");
        } else {
            sys_log("PARADIGM: Fate Strings missing rejected transition evidence.");
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
                sys_log("PARADIGM: Fault Fate records missing expected metadata.");
            }
        } else {
            sys_log("PARADIGM: No fault Fate records in current audit window.");
        }
    } else {
        sys_log("PARADIGM: Failed to read Fate Strings.");
    }

    /* TEST: PRISMATIC LATTICES (LAW 6) */
    sys_log("PARADIGM: Testing Prismatic Lattices...");
    uint32_t slot_lattice = 50;
    if (sys_lattice_create(2, slot_lattice) != 0) {
        sys_log("PARADIGM: Lattice Creation Failed.");
    } else {
        sys_log("PARADIGM: Lattice Created (2 pages).");
        
        uint64_t lattice_vaddr = 0x20000000;
        if (sys_lattice_attach(slot_lattice, lattice_vaddr) != 0) {
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
                sys_log("PARADIGM: Lattice Attunement FAILED.");
            }

            /* Real-fault probe verification: first-touch lattice access should be logged as #PF. */
            int fault_count_after_lattice = sys_fate_read_ex(fate_fault_buf, 16, 4, FATE_READ_FAULTS);
            if (fault_count_after_lattice > 0) {
                bool real_pf_seen = false;
                for (int i = 0; i < fault_count_after_lattice; i++) {
                    if (fate_fault_buf[i].fault_vector == 14 &&
                        fate_fault_buf[i].fault_cr2 >= lattice_vaddr &&
                        fate_fault_buf[i].fault_cr2 < (lattice_vaddr + (2ULL * 4096ULL))) {
                        real_pf_seen = true;
                        break;
                    }
                }
                if (real_pf_seen) {
                    sys_log("PARADIGM: Real fault probe captured in Fate Strings.");
                } else {
                    sys_log("PARADIGM: Real fault probe missing from Fate Strings.");
                }
            } else {
                sys_log("PARADIGM: Fault ledger empty after real fault probe.");
            }
        }
    }

    sys_log("PARADIGM: Testing ReadOnly Lattice Listeners...");
    uint32_t source_slot = 53;
    uint32_t listener_slot_a = 54;
    uint32_t listener_slot_b = 55;
    uint64_t source_vaddr = 0x21000000;
    uint64_t listener_vaddr = 0x22000000;
    if (sys_lattice_create_broadcast(2, source_slot, 2, listener_slot_a, listener_slot_b) != 0) {
        sys_log("PARADIGM: Broadcast Lattice Creation Failed.");
    } else {
        if (sys_lattice_attach(source_slot, source_vaddr) != 0 || sys_lattice_attach(listener_slot_a, listener_vaddr) != 0) {
            sys_log("PARADIGM: Broadcast Lattice Attach Failed.");
        } else {
            volatile uint64_t* source_ptr = (volatile uint64_t*)source_vaddr;
            volatile uint64_t* listener_ptr = (volatile uint64_t*)listener_vaddr;
            source_ptr[0] = 0xA11CEBADC0FFEEULL;
            if (listener_ptr[0] == 0xA11CEBADC0FFEEULL) {
                sys_log("PARADIGM: Broadcast Lattice ReadOnly Listener PASS.");
            } else {
                sys_log("PARADIGM: Broadcast Lattice ReadOnly Listener FAIL.");
            }

            if (sys_attune(listener_slot_a, 1) != 0) {
                sys_log("PARADIGM: ReadOnly Listener Attune Rejected (expected).");
            } else {
                sys_log("PARADIGM: ReadOnly Listener Attune Unexpectedly Allowed.");
            }
        }
    }

    if (sys_lattice_detach(listener_slot_a, listener_vaddr) == 0) {
        sys_log("PARADIGM: ReadOnly Listener Detach SUCCESS.");
    } else {
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
            sys_log("PARADIGM: Lattice Forensics attach records missing.");
        }
        if (saw_detach) {
            sys_log("PARADIGM: Lattice Forensics detach records visible.");
        } else {
            sys_log("PARADIGM: Lattice Forensics detach records missing.");
        }
    } else {
        sys_log("PARADIGM: No lattice forensic records in audit window.");
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
