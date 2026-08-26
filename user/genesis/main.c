#include "../../user/include/reaper.h"
#include <stdbool.h>
#include <string.h>

#define BOOTINFO_ADDR ((boot_info_t*)0x1000)

/* Slot layout Genesis chooses for the spawned Paradigm process. These mirror the
 * historical kernel-bootstrap convention so Paradigm's existing suite is
 * unaffected; moving this authority into user-space Genesis is the policy fix. */
#define GENESIS_PAGETABLE_SLOT   2u
#define GENESIS_RAM_SLOT         3u
#define GENESIS_AUDIT_SLOT       4u
#define GENESIS_SCHED_ROOT_SLOT  5u
#define GENESIS_SCHED_THREAD_SLOT 6u
#define GENESIS_CAP_SLOT         1u
#define GENESIS_MODULE_INDEX     1u /* module 1 = Paradigm */

int main(void) {
    sys_log("[GENESIS] Awake in the Void.");

    boot_info_t* bi = BOOTINFO_ADDR;
    if (bi->magic != BOOTINFO_MAGIC || bi->version != BOOTINFO_VERSION) {
        sys_log("[GENESIS] Fatal: bootinfo magic/version mismatch.");
        return 1;
    }
    if (bi->module_count < 2) {
        sys_log("[GENESIS] Fatal: < 2 modules present.");
        return 1;
    }
    if (bi->modules[1].type != BOOT_MODULE_TYPE_PARADIGM) {
        sys_log("[GENESIS] Fatal: module[1] is not Paradigm.");
        return 1;
    }
    sys_log("[GENESIS] Bootinfo verified; Paradigm module present.");

    /* Spawn Paradigm (module 1) via GENESIS_OP_SPAWN on GENESIS_CAP slot. */
    genesis_spawn_req_t spawn = {
        .module_index          = GENESIS_MODULE_INDEX,
        .flags                 = GENESIS_SPAWN_FLAG_QUEUE_RUN,
        .out_pid_slot          = 0,
        .out_root_cnode_slot   = 0,
        .out_pagetable_slot    = GENESIS_PAGETABLE_SLOT,
        .out_sched_root_slot   = GENESIS_SCHED_ROOT_SLOT,
        .out_sched_thread_slot = GENESIS_SCHED_THREAD_SLOT,
        .out_ram_slot          = GENESIS_RAM_SLOT,
        .out_audit_slot        = GENESIS_AUDIT_SLOT,
        .reserved0             = 0,
    };
    int paradigm_pid = sys_genesis_invoke(GENESIS_OP_SPAWN, GENESIS_CAP_SLOT,
                                          &spawn, sizeof(spawn), NULL);
    if (paradigm_pid <= 0) {
        sys_log("[GENESIS] Fatal: could not spawn Paradigm.");
        return 1;
    }
    sys_log("[GENESIS] Paradigm spawned.");

    /* Delegate a single bounded CAP_TYPE_SPAWN_AUTH to Paradigm so it can spawn
     * future daemons (Sage/Archive/...) after Genesis destroys GENESIS_CAP. */
    genesis_delegate_req_t del = {
        .target_pid    = (uint32_t)paradigm_pid,
        .target_slot   = GENESIS_SPAWN_AUTH_SLOT,
        .cap_type      = CAP_TYPE_SPAWN_AUTH,
        .cap_rights    = CAP_RIGHT_INVOKE,
        .badge         = 0,
        .object_ptr    = 0,
        .allowed_modes = CAP_MODE_ALL,
    };
    if (sys_genesis_invoke(GENESIS_OP_DELEGATE, GENESIS_CAP_SLOT,
                           &del, sizeof(del), NULL) != 0) {
        sys_log("[GENESIS] Fatal: could not delegate SPAWN_AUTH to Paradigm.");
        return 1;
    }
    sys_log("[GENESIS] SPAWN_AUTH delegated to Paradigm.");

    /* Destroy GENESIS_CAP: the seed of unbounded authority does not survive. */
    if (sys_genesis_invoke(GENESIS_OP_DESTROY, GENESIS_CAP_SLOT,
                           NULL, 0, NULL) == 0) {
        sys_log("[GENESIS] GENESIS_CAP destroyed. The Bridge is closed.");
    } else {
        sys_log("[GENESIS] Fatal: GENESIS_CAP destroy failed.");
        return 1;
    }

    /* Bootstrap job complete. Idle indefinitely. */
    sys_log("[GENESIS] Bootstrap complete. Idling.");
    while (1) {
        sys_yield();
    }
    return 0;
}
