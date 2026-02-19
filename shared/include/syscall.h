#ifndef SHARED_SYSCALL_H
#define SHARED_SYSCALL_H

/*
 * Syscall ABI v2:
 * One kernel entry (`SYS_GATE_CALL`) with operation-select in userspace payload.
 */
#define SYS_GATE_CALL 0

/*
 * Gate operation ids.
 * These replace direct syscall-number invocation from user space.
 */
#define GATE_OP_VOID_LOG        1
#define GATE_OP_CAP_INVOKE      2
#define GATE_OP_CAP_MINT        3
#define GATE_OP_CAP_COPY        4
#define GATE_OP_CAP_DELETE      5
#define GATE_OP_MODE_QUERY      6
#define GATE_OP_YIELD           7
#define GATE_OP_EXIT            8
#define GATE_OP_WAIT            9
#define GATE_OP_MAP             10
#define GATE_OP_UNMAP           11
#define GATE_OP_CAP_REVOKE      12
#define GATE_OP_LATTICE_CREATE  13
#define GATE_OP_LATTICE_ATTACH  14
#define GATE_OP_FATE_READ       15
#define GATE_OP_FRAME_ALLOC     16
#define GATE_OP_CAP_RETYPE      17
#define GATE_OP_ATTUNE          18
#define GATE_OP_OCULAR_SET      19
#define GATE_OP_LATTICE_DETACH  20
#define GATE_OP_AUDIT           21

typedef struct {
    uint64_t args[6];
} gate_call_msg_t;

#endif
