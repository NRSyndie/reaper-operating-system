#ifndef USER_REAPER_H
#define USER_REAPER_H

#include <stdint.h>
#include "../../shared/include/bootinfo.h"
#include "../../shared/include/capability.h"
#include "../../shared/include/mode.h"
#include "../../shared/include/syscall.h"

void sys_log(const char* msg);
int sys_log_checked(const char* msg);
void sys_exit(int code);
void sys_yield(void);
int sys_wait(uint64_t flags);
int sys_mode_query(void);
int sys_mode_transition(uint32_t target_mode);
int sys_mode_transition_ex(uint32_t target_mode, uint32_t auth_flags);

/* Recursive VMM API (strict contract enforced by kernel) */
int sys_map(uint32_t parent_cap, uint32_t index, uint32_t child_cap, uint64_t flags);
int sys_map_strict(uint32_t parent_cap, uint32_t index, uint32_t child_cap, uint64_t flags);
int sys_unmap(uint32_t parent_cap, uint32_t index);
int sys_unmap_strict(uint32_t parent_cap, uint32_t index);
int sys_cap_retype(uint32_t src, uint32_t dst, uint32_t new_type, uint32_t badge);
int sys_frame_alloc(uint32_t slot);
int sys_cap_delete(uint32_t slot);

int sys_lattice_create(uint32_t page_count, uint32_t slot);
int sys_lattice_create_broadcast(uint32_t page_count, uint32_t source_slot, uint32_t listener_count, uint32_t listener_slot0, uint32_t listener_slot1);
int sys_lattice_attach(uint32_t lattice_cap, uint64_t vaddr);
int sys_lattice_detach(uint32_t lattice_cap, uint64_t vaddr);
int sys_attune(uint32_t lattice_cap, uint32_t index);
int sys_cap_mint(uint32_t src, uint32_t dst, uint16_t rights, uint32_t badge, uint8_t modes);
int sys_fate_read(void* buffer, int count, uint32_t audit_cap);
int sys_fate_read_ex(void* buffer, int count, uint32_t audit_cap, uint32_t read_mode);
int sys_audit(uint64_t target_pid, uint64_t flags, void* out_buf, uint64_t count);
int sys_sched_metrics(gate_sched_metrics_t* out_metrics);
int sys_sched_auth_root_mint(uint32_t mode_binding,
                             uint64_t max_total_budget,
                             uint64_t refill_period_ticks,
                             uint64_t max_accumulated,
                             uint32_t dst_slot);
int sys_sched_auth_thread_derive(uint32_t root_slot,
                                 uint32_t dst_slot,
                                 uint32_t max_slice,
                                 uint32_t weight,
                                 uint64_t local_max_accumulated);

/* String utils (implemented in string.c or similar) */
void* memset(void* dest, int c, unsigned long n);
void* memcpy(void* dest, const void* src, unsigned long n);
unsigned long strlen(const char* str);
char* itoa(long value, char* str, int base);

#endif /* USER_REAPER_H */
