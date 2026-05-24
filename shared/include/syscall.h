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
#define CAP_INVOKE_OPT_AUTO     0
#define CAP_INVOKE_OPT_SEND     1
#define CAP_INVOKE_OPT_RECV     2
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
#define GATE_OP_SCHED_METRICS   22
#define GATE_OP_SCHED_AUTH_ROOT_MINT   23
#define GATE_OP_SCHED_AUTH_THREAD_DERIVE 24
#define GATE_OP_MODE_TRANSITION 25
#define GATE_OP_LAW2_ATTEST     26
#define GATE_OP_GENESIS_INVOKE  27

#define LAW2_DAY29_REASON_AUTH_OK      (1ULL << 0)
#define LAW2_DAY29_REASON_CTRL_DENY    (1ULL << 1)
#define LAW2_DAY29_REASON_PARENT_DENY  (1ULL << 2)
#define LAW2_DAY29_REASON_RIGHTS_DENY  (1ULL << 3)
#define LAW2_DAY29_REASON_INDEX_DENY   (1ULL << 4)
#define LAW2_DAY29_REASON_MASK_REQUIRED (LAW2_DAY29_REASON_AUTH_OK | \
                                         LAW2_DAY29_REASON_CTRL_DENY | \
                                         LAW2_DAY29_REASON_PARENT_DENY | \
                                         LAW2_DAY29_REASON_RIGHTS_DENY | \
                                         LAW2_DAY29_REASON_INDEX_DENY)

#define LAW2_DAY30_REASON_EDGE_ILLEGAL        (1ULL << 0)
#define LAW2_DAY30_REASON_AUTH_REQUIRED       (1ULL << 1)
#define LAW2_DAY30_REASON_SPECIAL_KEY_REQ     (1ULL << 2)
#define LAW2_DAY30_REASON_COOLDOWN_ACTIVE     (1ULL << 3)
#define LAW2_DAY30_REASON_MASK_REQUIRED (LAW2_DAY30_REASON_EDGE_ILLEGAL | \
                                         LAW2_DAY30_REASON_AUTH_REQUIRED | \
                                         LAW2_DAY30_REASON_SPECIAL_KEY_REQ)

typedef struct {
    uint32_t evidence_version;
    uint32_t day28_status;
    uint32_t day29_status;
    uint32_t day30_status;
    uint64_t map_calls;
    uint64_t map_strict_calls;
    uint64_t unmap_calls;
    uint64_t unmap_strict_calls;
    uint64_t map_fail_index;
    uint64_t map_fail_child;
    uint64_t map_fail_flags;
    uint64_t day29_reason_mask;
    uint64_t day29_unmap_cycles_max;
    uint64_t day29_unmap_cycles_avg;
    uint64_t day29_perf_budget_cycles;
    uint64_t transition_reject_with_reason;
    uint64_t day30_reason_mask;
    uint64_t day30_reject_scan_cycles;
    uint64_t day30_perf_budget_cycles;
} gate_law2_attest_t;

typedef struct {
    uint64_t schedule_count;
    uint64_t switch_count;
    uint64_t remote_enqueue;
    uint64_t migrations;
    uint64_t denied_enqueue;
    uint64_t denied_wake;
    uint64_t denied_dispatch;
    uint64_t denied_no_auth;
    uint64_t denied_mode_mismatch;
    uint64_t budget_exhaustions;
    uint64_t envelope_switches;
    uint64_t active_security_epoch;
    uint32_t cpu_id;
    uint32_t active_mode;
    uint32_t ready_depth;
    uint32_t zombie_depth;
} gate_sched_metrics_t;

typedef struct {
    uint64_t args[6];
} gate_call_msg_t;

typedef enum {
    GENESIS_OP_SPAWN = 1,
    GENESIS_OP_DELEGATE = 2,
    GENESIS_OP_DESTROY = 3
} genesis_op_t;

typedef struct {
    uint32_t module_index;
    uint32_t flags;
    uint32_t out_pid_slot;
    uint32_t out_root_cnode_slot;
    uint32_t out_pagetable_slot;
    uint32_t out_sched_root_slot;
    uint32_t out_sched_thread_slot;
    uint32_t reserved0;
} genesis_spawn_req_t;

typedef struct {
    uint32_t pid;
    uint32_t root_cnode_slot;
    uint32_t pagetable_slot;
    uint32_t sched_root_slot;
    uint32_t sched_thread_slot;
    uint32_t reserved0;
    uint64_t reserved1;
} genesis_spawn_resp_t;

typedef struct {
    uint32_t target_pid;
    uint32_t target_slot;
    uint16_t cap_type;
    uint16_t cap_rights;
    uint32_t badge;
    uint64_t object_ptr;
    uint8_t allowed_modes;
    uint8_t reserved0[7];
} genesis_delegate_req_t;

typedef struct {
    uint32_t delegated;
    uint32_t reserved0;
} genesis_delegate_resp_t;

typedef struct {
    uint32_t flags;
    uint32_t reserved0;
} genesis_destroy_req_t;

typedef struct {
    uint32_t destroyed;
    uint32_t reserved0;
} genesis_destroy_resp_t;

#endif
