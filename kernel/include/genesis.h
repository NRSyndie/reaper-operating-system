#ifndef REAPER_GENESIS_H
#define REAPER_GENESIS_H

#include <stdbool.h>
#include <stdint.h>
#include "bootinfo.h"
#include "limine.h"
#include "mode.h"
#include "process.h"
#include "thread.h"

typedef struct {
    uint32_t genesis_cap_slot;
    uint32_t pagetable_slot;
    uint32_t ram_slot;
    uint32_t audit_slot;
    uint32_t sched_root_slot;
    uint32_t sched_thread_slot;
} genesis_initial_caps_t;

typedef struct {
    process_t* process;
    thread_t* thread;
    uint64_t entry_point;
    uint64_t bootinfo_phys;
    genesis_initial_caps_t caps;
} genesis_spawn_result_t;

/**
 * genesis_bridge_spawn: Injects the first capability into user-space 
 * and launches the Paradigm daemon.
 */
void genesis_bridge_spawn(void);

bool genesis_bootinfo_init(boot_info_t* bootinfo, uint32_t genesis_cap_slot);
int genesis_inject_initial_caps(process_t* proc, const genesis_initial_caps_t* caps);
int genesis_map_bootinfo(process_t* proc, uint64_t bootinfo_phys);
int genesis_map_initial_stack(process_t* proc, uint64_t stack_top, uint64_t stack_pages);
int genesis_load_module_image(struct limine_file* module, process_t* proc, uint64_t* entry_point);
int genesis_spawn_process_from_module(struct limine_file* module,
                                      mode_id_t mode,
                                      uint64_t stack_top,
                                      uint64_t stack_pages,
                                      const genesis_initial_caps_t* caps,
                                      bool mint_sched_auth,
                                      bool queue_thread,
                                      genesis_spawn_result_t* out_result);

#endif /* REAPER_GENESIS_H */
