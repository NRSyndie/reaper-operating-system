#include <stdint.h>
#include "../../shared/include/syscall.h"
#include "../../shared/include/mode.h"

/* Generic Syscall Shim */
static uint64_t do_syscall(uint64_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    uint64_t ret;
    register uint64_t r10 __asm__("r10") = a3;
    register uint64_t r8  __asm__("r8")  = a4;

    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static int do_gate_call(uint64_t op, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    gate_call_msg_t msg;
    msg.args[0] = a0;
    msg.args[1] = a1;
    msg.args[2] = a2;
    msg.args[3] = a3;
    msg.args[4] = a4;
    msg.args[5] = 0;
    return (int)do_syscall(SYS_GATE_CALL, op, (uint64_t)&msg, sizeof(msg), 0, 0);
}

/* Public API */

int sys_log_checked(const char* msg) {
    return do_gate_call(GATE_OP_VOID_LOG, (uint64_t)msg, 0, 0, 0, 0);
}

void sys_log(const char* msg) {
    (void)sys_log_checked(msg);
}

void sys_exit(int code) {
    (void)do_gate_call(GATE_OP_EXIT, (uint64_t)code, 0, 0, 0, 0);
}

void sys_yield(void) {
    (void)do_gate_call(GATE_OP_YIELD, 0, 0, 0, 0, 0);
}

int sys_wait(uint64_t flags) {
    return do_gate_call(GATE_OP_WAIT, flags, 0, 0, 0, 0);
}

int sys_mode_query(void) {
    return do_gate_call(GATE_OP_MODE_QUERY, 0, 0, 0, 0, 0);
}

int sys_map(uint32_t parent_cap, uint32_t index, uint32_t child_cap, uint64_t flags) {
    return do_gate_call(GATE_OP_MAP, parent_cap, index, child_cap, flags, 1);
}

int sys_map_strict(uint32_t parent_cap, uint32_t index, uint32_t child_cap, uint64_t flags) {
    return sys_map(parent_cap, index, child_cap, flags);
}

int sys_unmap(uint32_t parent_cap, uint32_t index) {
    return do_gate_call(GATE_OP_UNMAP, parent_cap, index, 1, 0, 0);
}

int sys_unmap_strict(uint32_t parent_cap, uint32_t index) {
    return sys_unmap(parent_cap, index);
}

int sys_cap_retype(uint32_t src, uint32_t dst, uint32_t new_type, uint32_t badge) {
    return do_gate_call(GATE_OP_CAP_RETYPE, src, dst, new_type, badge, 0);
}

int sys_frame_alloc(uint32_t slot) {
    return do_gate_call(GATE_OP_FRAME_ALLOC, slot, 0, 0, 0, 0);
}

int sys_cap_delete(uint32_t slot) {
    return do_gate_call(GATE_OP_CAP_DELETE, slot, 0, 0, 0, 0);
}

int sys_lattice_create(uint32_t page_count, uint32_t slot) {
    return do_gate_call(GATE_OP_LATTICE_CREATE, page_count, slot, 0, 0, 0);
}

int sys_lattice_create_broadcast(uint32_t page_count, uint32_t source_slot, uint32_t listener_count, uint32_t listener_slot0, uint32_t listener_slot1) {
    return do_gate_call(GATE_OP_LATTICE_CREATE, page_count, source_slot, listener_count, listener_slot0, listener_slot1);
}

int sys_lattice_attach(uint32_t lattice_cap, uint64_t vaddr) {
    return do_gate_call(GATE_OP_LATTICE_ATTACH, lattice_cap, vaddr, 0, 0, 0);
}

int sys_lattice_detach(uint32_t lattice_cap, uint64_t vaddr) {
    return do_gate_call(GATE_OP_LATTICE_DETACH, lattice_cap, vaddr, 0, 0, 0);
}

int sys_attune(uint32_t lattice_cap, uint32_t index) {
    return do_gate_call(GATE_OP_ATTUNE, lattice_cap, index, 0, 0, 0);
}

int sys_cap_mint(uint32_t src, uint32_t dst, uint16_t rights, uint32_t badge, uint8_t modes) {
    return do_gate_call(GATE_OP_CAP_MINT, src, dst, rights, badge, modes);
}

int sys_fate_read(void* buffer, int count, uint32_t audit_cap) {
    return do_gate_call(GATE_OP_FATE_READ, (uint64_t)buffer, (uint64_t)count, (uint64_t)audit_cap, FATE_READ_ALL, 0);
}

int sys_fate_read_ex(void* buffer, int count, uint32_t audit_cap, uint32_t read_mode) {
    return do_gate_call(GATE_OP_FATE_READ, (uint64_t)buffer, (uint64_t)count, (uint64_t)audit_cap, (uint64_t)read_mode, 0);
}

int sys_audit(uint64_t target_pid, uint64_t flags, void* out_buf, uint64_t count) {
    return do_gate_call(GATE_OP_AUDIT, target_pid, flags, (uint64_t)out_buf, count, 0);
}

int sys_sched_metrics(gate_sched_metrics_t* out_metrics) {
    if (!out_metrics) return -1;
    return do_gate_call(GATE_OP_SCHED_METRICS, (uint64_t)out_metrics, sizeof(*out_metrics), 0, 0, 0);
}
