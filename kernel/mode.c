#include <mode.h>
#include <mode_internal.h>
#include <utils.h>  // For spinlock, memset, memcpy
#include <cpu.h>    // For rdtsc
#include <console.h> // For kprintf/logging
#include <klog.h>
#include <stddef.h>
#include <scheduler.h>
#include <process.h>
#include <include/ocular.h>
#include <include/pcid.h>
#include <include/vmm.h>
#include <include/port_io.h>
#include <include/power.h>
#include <include/audit.h>

/* 
 * Global Kernel Mode State 
 * Initialized in .data to ensure writable memory.
 */
struct mode_state kernel_mode_state __attribute__((section(".data"))) = {
    .transition_lock = 0,
    .current_mode = MODE_VOID,
    .previous_mode = MODE_VOID,
    .global_mode_mask = 0,
    .fate_head_index = 0,
    .fate_generation_id = 0,
    .stats = {0},
    .security_epoch = 1
};

#define MODE_BOOT_RECORD_MAGIC 0x4Du
#define MODE_BOOT_RECORD_VERSION 0x01u
#define MODE_PROMPT_AUTH_RETRY_LIMIT 3u
#define MODE_SECURE_COOLDOWN_TICKS 2400u
#define MODE_LOCKDOWN_DEESC_TICKS 300u

typedef struct {
    uint8_t  magic;
    uint8_t  version;
    uint8_t  sequence;
    uint8_t  mode;
    uint8_t  valid;
    uint8_t  checksum;
    uint8_t  reserved0;
    uint8_t  reserved1;
} __attribute__((packed)) mode_boot_record_t;

static mode_boot_record_t g_mode_boot_record __attribute__((section(".data"))) = {
    .magic = MODE_BOOT_RECORD_MAGIC,
    .version = MODE_BOOT_RECORD_VERSION,
    .sequence = 0,
    .mode = MODE_CASUAL,
    .valid = 0,
    .checksum = 0,
    .reserved0 = 0,
    .reserved1 = 0
};

/* --- Helper Functions (Internal) --- */

static void calculate_record_hash(struct mode_transition* record) {
    uint64_t hash = 0xcbf29ce484222325ULL;

    const uint8_t* p = (const uint8_t*)record;
    for (size_t i = 0; i < sizeof(struct mode_transition); i++) {
        /* Skip curr_hash bytes [16..23] to avoid self-referential hashing. */
        if (i >= 16 && i < 24) continue;
        hash ^= p[i];
        hash *= 0x100000001b3ULL;
    }

    record->curr_hash = hash;
}

static void fate_append_record(struct mode_transition* record) {
    uint64_t idx = kernel_mode_state.fate_head_index % MODE_HISTORY_SIZE;

    if (kernel_mode_state.fate_head_index > 0) {
        uint64_t prev_idx = (kernel_mode_state.fate_head_index - 1) % MODE_HISTORY_SIZE;
        record->prev_hash = kernel_mode_state.fate_history[prev_idx].curr_hash;
    } else {
        record->prev_hash = 0;
    }

    calculate_record_hash(record);
    memcpy(&kernel_mode_state.fate_history[idx], record, sizeof(struct mode_transition));

    kernel_mode_state.fate_head_index++;
    if (kernel_mode_state.fate_head_index % MODE_HISTORY_SIZE == 0) {
        kernel_mode_state.fate_generation_id++;
    }
}

static void fate_log_transition_event(mode_id_t from_mode,
                                      mode_id_t to_mode,
                                      transition_source_t source,
                                      fate_result_t result,
                                      mode_reject_reason_t reason_code) {
    struct mode_transition record;
    memset(&record, 0, sizeof(record));
    record.timestamp_tsc = rdtsc();
    record.from_mode = (uint8_t)from_mode;
    record.to_mode = (uint8_t)to_mode;
    record.trigger_source = (uint8_t)source;
    record.result_code = (uint8_t)result;
    record.record_type = FATE_RECORD_TRANSITION;
    record.fault_error_code = (uint32_t)reason_code;

    thread_t* curr_thread = scheduler_get_current();
    if (curr_thread && curr_thread->owner) {
        record.requestor_pid = curr_thread->owner->pid;
    } else {
        record.requestor_pid = 0;
    }

    fate_append_record(&record);
}

void mode_log_fault_event(uint8_t vector, uint64_t error_code, uint64_t rip,
                          uint64_t cr2, uint64_t rsp, uint64_t cs, uint64_t rflags,
                          bool from_user) {
    uint64_t flags = spinlock_irqsave(&kernel_mode_state.transition_lock);

    struct mode_transition record;
    memset(&record, 0, sizeof(record));
    record.timestamp_tsc = rdtsc();
    record.record_type = FATE_RECORD_FAULT;
    record.result_code = FATE_RESULT_ACCEPTED;
    record.fault_vector = vector;
    record.fault_error_code = (uint32_t)error_code;
    record.fault_rip = rip;
    record.fault_cr2 = cr2;
    record.fault_rsp = rsp;
    record.fault_cs = cs;
    record.fault_rflags = rflags;
    record.from_mode = (uint8_t)kernel_mode_state.current_mode;
    record.to_mode = (uint8_t)kernel_mode_state.current_mode;
    record.trigger_source = from_user ? TRANSITION_SOURCE_USER : TRANSITION_SOURCE_KERNEL;

    thread_t* curr_thread = scheduler_get_current();
    if (from_user && curr_thread && curr_thread->owner) {
        record.requestor_pid = curr_thread->owner->pid;
    } else {
        record.requestor_pid = 0;
    }

    fate_append_record(&record);
    spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
}

void mode_log_lattice_event(uint32_t pid, uint64_t vaddr, uint32_t page_count,
                            bool is_source, bool is_attach) {
    uint64_t flags = spinlock_irqsave(&kernel_mode_state.transition_lock);

    struct mode_transition record;
    memset(&record, 0, sizeof(record));
    record.timestamp_tsc = rdtsc();
    record.record_type = FATE_RECORD_LATTICE;
    record.result_code = FATE_RESULT_ACCEPTED;
    record.fault_vector = (uint8_t)(is_attach ? FATE_LATTICE_ATTACH : FATE_LATTICE_DETACH);
    record.fault_error_code = page_count;
    record.fault_rip = is_source ? 1 : 0;
    record.fault_cr2 = vaddr;
    record.from_mode = (uint8_t)kernel_mode_state.current_mode;
    record.to_mode = (uint8_t)kernel_mode_state.current_mode;
    record.trigger_source = TRANSITION_SOURCE_KERNEL;
    record.requestor_pid = pid;

    fate_append_record(&record);
    spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
}

void mode_log_sched_event(uint32_t pid, uint8_t event_code, uint32_t detail, fate_result_t result) {
    uint64_t flags = spinlock_irqsave(&kernel_mode_state.transition_lock);
    struct mode_transition record;

    memset(&record, 0, sizeof(record));
    record.timestamp_tsc = rdtsc();
    record.record_type = FATE_RECORD_TRANSITION;
    record.result_code = (uint8_t)result;
    record.trigger_source = TRANSITION_SOURCE_KERNEL;
    record.requestor_pid = pid;
    record.from_mode = (uint8_t)kernel_mode_state.current_mode;
    record.to_mode = (uint8_t)kernel_mode_state.current_mode;
    record.fault_vector = event_code;
    record.fault_error_code = detail;

    fate_append_record(&record);
    spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
}

void mode_log_law2_attestation(uint8_t day_id, fate_result_t result, uint16_t reason_code, uint32_t detail) {
    uint64_t flags = spinlock_irqsave(&kernel_mode_state.transition_lock);
    struct mode_transition record;

    memset(&record, 0, sizeof(record));
    record.timestamp_tsc = rdtsc();
    record.record_type = FATE_RECORD_ATTEST;
    record.result_code = (uint8_t)result;
    record.fault_vector = day_id;
    record.fault_error_code = (uint32_t)reason_code;
    record.fault_rip = (uint64_t)detail;
    record.trigger_source = TRANSITION_SOURCE_KERNEL;
    record.from_mode = (uint8_t)kernel_mode_state.current_mode;
    record.to_mode = (uint8_t)kernel_mode_state.current_mode;
    record.requestor_pid = 0;

    fate_append_record(&record);
    spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
}

static bool fate_record_matches_filter(const struct mode_transition* rec, fate_read_mode_t mode) {
    if (mode == FATE_READ_ALL) return true;
    if (mode == FATE_READ_TRANSITIONS) return rec->record_type == FATE_RECORD_TRANSITION;
    if (mode == FATE_READ_FAULTS) return rec->record_type == FATE_RECORD_FAULT;
    if (mode == FATE_READ_LATTICE) return rec->record_type == FATE_RECORD_LATTICE;
    if (mode == FATE_READ_ATTEST) return rec->record_type == FATE_RECORD_ATTEST;
    return false;
}

static uint8_t boot_record_checksum(uint8_t version, uint8_t sequence, uint8_t mode, uint8_t valid) {
    uint8_t c = (uint8_t)(MODE_BOOT_RECORD_MAGIC ^ version ^ sequence ^ mode ^ valid ^ 0xA5u);
    c = (uint8_t)((c << 3) | (c >> 5));
    c ^= 0x5Au;
    return c;
}

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, (uint8_t)(reg | 0x80u));
    return inb(0x71);
}

static void cmos_write(uint8_t reg, uint8_t value) {
    outb(0x70, (uint8_t)(reg | 0x80u));
    outb(0x71, value);
}

static void boot_record_load_persistent(void) {
    uint8_t* p = (uint8_t*)&g_mode_boot_record;
    uint8_t reg = 0x40;
    for (size_t i = 0; i < sizeof(g_mode_boot_record); i++) {
        p[i] = cmos_read((uint8_t)(reg + (uint8_t)i));
    }
}

static void boot_record_store_persistent(void) {
    const uint8_t* p = (const uint8_t*)&g_mode_boot_record;
    uint8_t reg = 0x40;
    for (size_t i = 0; i < sizeof(g_mode_boot_record); i++) {
        cmos_write((uint8_t)(reg + (uint8_t)i), p[i]);
    }
}

static bool boot_record_is_valid(void) {
    if (g_mode_boot_record.magic != MODE_BOOT_RECORD_MAGIC) return false;
    if (g_mode_boot_record.version != MODE_BOOT_RECORD_VERSION) return false;
    if (g_mode_boot_record.valid == 0) return false;
    return g_mode_boot_record.checksum ==
           boot_record_checksum(g_mode_boot_record.version,
                                g_mode_boot_record.sequence,
                                g_mode_boot_record.mode,
                                g_mode_boot_record.valid);
}

static void boot_record_store_mode(mode_id_t mode) {
    if (mode < MODE_CASUAL || mode > MODE_LOCKDOWN) return;
    g_mode_boot_record.sequence++;
    g_mode_boot_record.mode = (uint8_t)mode;
    g_mode_boot_record.valid = 1;
    g_mode_boot_record.checksum = boot_record_checksum(g_mode_boot_record.version,
                                                       g_mode_boot_record.sequence,
                                                       g_mode_boot_record.mode,
                                                       g_mode_boot_record.valid);
    boot_record_store_persistent();
}

static bool boot_record_load_mode(mode_id_t* out_mode) {
    if (!out_mode) return false;
    if (!boot_record_is_valid()) return false;
    if (g_mode_boot_record.mode < MODE_CASUAL || g_mode_boot_record.mode > MODE_LOCKDOWN) return false;
    *out_mode = (mode_id_t)g_mode_boot_record.mode;
    return true;
}

static uint32_t mode_default_auth_flags(transition_source_t source) {
    switch (source) {
        case TRANSITION_SOURCE_USER:
            return MODE_AUTH_PASSWORD | MODE_AUTH_MANUAL |
                   MODE_AUTH_SPECIAL_KEY | MODE_AUTH_COOLDOWN_ELAPSED;
        case TRANSITION_SOURCE_DAEMON:
            return MODE_AUTH_PASSWORD | MODE_AUTH_SYSTEM_PROMPT |
                   MODE_AUTH_COOLDOWN_ELAPSED | MODE_AUTH_DEESC_ELAPSED;
        case TRANSITION_SOURCE_KERNEL:
            return MODE_AUTH_AUTOMATIC | MODE_AUTH_BOOT_INTENT |
                   MODE_AUTH_COOLDOWN_ELAPSED | MODE_AUTH_DEESC_ELAPSED;
        default:
            return 0;
    }
}

typedef struct {
    uint64_t txid;
    mode_id_t from_mode;
    mode_id_t to_mode;
    transition_source_t source;
    uint32_t auth_flags;
    mode_reject_reason_t reject_reason;
    envelope_class_t from_class;
    envelope_class_t to_class;
    uint32_t requestor_pid;
    bool ghost_flush_required;
} env_compiled_transition_t;

typedef struct {
    mode_id_t prev_mode;
    mode_id_t current_mode;
    uint8_t mode_mask;
} env_apply_snapshot_t;

static envelope_class_t mode_to_envelope_class(mode_id_t mode) {
    switch (mode) {
        case MODE_CASUAL: return ENV_CLASS_BASELINE;
        case MODE_SECURE: return ENV_CLASS_DEFENSIVE;
        case MODE_LOCKDOWN: return ENV_CLASS_FAIL_CLOSED;
        case MODE_GHOST: return ENV_CLASS_EPHEMERAL;
        default: return ENV_CLASS_NONE;
    }
}

static void env_marker_compile(const env_compiled_transition_t* compiled) {
    kprintf("[ENV_COMPILE] txid=%lu from=%u to=%u src=%u auth=0x%x class_from=%u class_to=%u pid=%u\n",
            compiled->txid,
            (unsigned)compiled->from_mode,
            (unsigned)compiled->to_mode,
            (unsigned)compiled->source,
            (unsigned)compiled->auth_flags,
            (unsigned)compiled->from_class,
            (unsigned)compiled->to_class,
            compiled->requestor_pid);
}

static void env_marker_verify(const env_compiled_transition_t* compiled, envelope_deny_t deny) {
    kprintf("[ENV_VERIFY] txid=%lu from=%u to=%u result=%s deny=%u reason=%u\n",
            compiled->txid,
            (unsigned)compiled->from_mode,
            (unsigned)compiled->to_mode,
            deny == ENV_DENY_NONE ? "ALLOW" : "DENY",
            (unsigned)deny,
            (unsigned)compiled->reject_reason);
}

static void env_marker_apply(const env_compiled_transition_t* compiled) {
    kprintf("[ENV_APPLY] txid=%lu from=%u to=%u ghost_flush=%u\n",
            compiled->txid,
            (unsigned)compiled->from_mode,
            (unsigned)compiled->to_mode,
            compiled->ghost_flush_required ? 1U : 0U);
}

static void env_marker_attest(const env_compiled_transition_t* compiled, fate_result_t result, envelope_deny_t deny) {
    kprintf("[ENV_ATTEST] txid=%lu from=%u to=%u result=%u deny=%u reason=%u epoch=%lu\n",
            compiled->txid,
            (unsigned)compiled->from_mode,
            (unsigned)compiled->to_mode,
            (unsigned)result,
            (unsigned)deny,
            (unsigned)compiled->reject_reason,
            mode_get_security_epoch());
}

static void env_marker_rollback(const env_compiled_transition_t* compiled) {
    kprintf("[ENV_ROLLBACK] txid=%lu from=%u to=%u\n",
            compiled->txid,
            (unsigned)compiled->from_mode,
            (unsigned)compiled->to_mode);
}

static bool env_compile_transition(mode_id_t current, mode_id_t target_mode, transition_source_t source,
                                   uint32_t auth_flags,
                                   env_compiled_transition_t* out_compiled) {
    thread_t* curr_thread;
    if (!out_compiled) return false;

    memset(out_compiled, 0, sizeof(*out_compiled));
    out_compiled->txid = ++kernel_mode_state.envelope_txid;
    out_compiled->from_mode = current;
    out_compiled->to_mode = target_mode;
    out_compiled->source = source;
    out_compiled->auth_flags = auth_flags;
    out_compiled->reject_reason = MODE_REJECT_NONE;
    out_compiled->from_class = mode_to_envelope_class(current);
    out_compiled->to_class = mode_to_envelope_class(target_mode);
    out_compiled->ghost_flush_required = (target_mode == MODE_GHOST || current == MODE_GHOST);

    curr_thread = scheduler_get_current();
    if (curr_thread && curr_thread->owner) {
        out_compiled->requestor_pid = curr_thread->owner->pid;
    }

    env_marker_compile(out_compiled);
    return true;
}

static envelope_deny_t env_verify_transition(env_compiled_transition_t* compiled) {
    uint64_t tick_now = scheduler_get_global_tick();
    uint64_t in_mode_ticks;
    bool auth_ok;
    bool source_ok = false;
    bool kernel_override = (compiled->source == TRANSITION_SOURCE_KERNEL) &&
                           ((compiled->auth_flags & MODE_AUTH_AUTOMATIC) != 0);

    if (compiled->to_mode < MODE_CASUAL || compiled->to_mode > MODE_GHOST) {
        compiled->reject_reason = MODE_REJECT_TARGET_INVALID;
        return ENV_DENY_TARGET_INVALID;
    }
    if (compiled->from_mode < MODE_VOID || compiled->from_mode > MODE_GHOST) {
        compiled->reject_reason = MODE_REJECT_SOURCE_INVALID;
        return ENV_DENY_SOURCE_INVALID;
    }

    if (compiled->from_mode == MODE_VOID) {
        if (compiled->to_mode == MODE_CASUAL) return ENV_DENY_NONE;
        if ((compiled->auth_flags & MODE_AUTH_BOOT_INTENT) && compiled->to_mode == MODE_SECURE) return ENV_DENY_NONE;
        if ((compiled->auth_flags & MODE_AUTH_BOOT_INTENT) && compiled->to_mode == MODE_LOCKDOWN) return ENV_DENY_NONE;
        compiled->reject_reason = MODE_REJECT_BOOT_POLICY;
        return ENV_DENY_EDGE_ILLEGAL;
    }

    /* Edge/source policy */
    if (compiled->from_mode == MODE_CASUAL && compiled->to_mode == MODE_SECURE) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER ||
                     compiled->source == TRANSITION_SOURCE_DAEMON ||
                     compiled->source == TRANSITION_SOURCE_KERNEL);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_CASUAL && compiled->to_mode == MODE_LOCKDOWN) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER ||
                     compiled->source == TRANSITION_SOURCE_DAEMON ||
                     kernel_override);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        auth_ok = (compiled->auth_flags & MODE_AUTH_PASSWORD) != 0 || kernel_override;
        if (!auth_ok) {
            if (compiled->source == TRANSITION_SOURCE_DAEMON) {
                kernel_mode_state.prompt_auth_failures++;
                if (kernel_mode_state.prompt_auth_failures >= MODE_PROMPT_AUTH_RETRY_LIMIT) {
                    boot_record_store_mode(MODE_LOCKDOWN);
                    compiled->reject_reason = MODE_REJECT_AUTH_RETRY_LIMIT;
                    kprintf("[MODE_SHUTDOWN_INTENT] action=boot_lockdown reason=auth_retry_limit\n");
                    kprintf("[MODE_REBOOT] action=immediate reason=auth_retry_limit\n");
                    system_reboot_now();
                }
            }
            compiled->reject_reason = MODE_REJECT_AUTH_REQUIRED;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        if (compiled->source == TRANSITION_SOURCE_DAEMON &&
            (compiled->auth_flags & MODE_AUTH_SYSTEM_PROMPT) == 0) {
            compiled->reject_reason = MODE_REJECT_SYSTEM_PROMPT_REQUIRED;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_CASUAL && compiled->to_mode == MODE_GHOST) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        if ((compiled->auth_flags & MODE_AUTH_PASSWORD) == 0 && !kernel_override) {
            compiled->reject_reason = MODE_REJECT_AUTH_REQUIRED;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_SECURE && compiled->to_mode == MODE_CASUAL) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER ||
                     compiled->source == TRANSITION_SOURCE_DAEMON ||
                     kernel_override);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        in_mode_ticks = tick_now - kernel_mode_state.mode_enter_tick[MODE_SECURE];
        if (in_mode_ticks < MODE_SECURE_COOLDOWN_TICKS &&
            (compiled->auth_flags & MODE_AUTH_COOLDOWN_ELAPSED) == 0) {
            compiled->reject_reason = MODE_REJECT_COOLDOWN_ACTIVE;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        if ((compiled->auth_flags & MODE_AUTH_PASSWORD) == 0 && !kernel_override) {
            compiled->reject_reason = MODE_REJECT_AUTH_REQUIRED;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_SECURE && compiled->to_mode == MODE_LOCKDOWN) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER ||
                     compiled->source == TRANSITION_SOURCE_DAEMON ||
                     kernel_override);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        if ((compiled->auth_flags & MODE_AUTH_PASSWORD) == 0 && !kernel_override) {
            compiled->reject_reason = MODE_REJECT_AUTH_REQUIRED;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_SECURE && compiled->to_mode == MODE_GHOST) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER ||
                     compiled->source == TRANSITION_SOURCE_DAEMON ||
                     compiled->source == TRANSITION_SOURCE_KERNEL);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_LOCKDOWN && compiled->to_mode == MODE_SECURE) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER ||
                     compiled->source == TRANSITION_SOURCE_DAEMON ||
                     kernel_override);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        if (compiled->source == TRANSITION_SOURCE_USER && !kernel_override) {
            if ((compiled->auth_flags & MODE_AUTH_SPECIAL_KEY) == 0) {
                compiled->reject_reason = MODE_REJECT_SPECIAL_KEY_REQUIRED;
                return ENV_DENY_EDGE_ILLEGAL;
            }
        } else {
            in_mode_ticks = tick_now - kernel_mode_state.mode_enter_tick[MODE_LOCKDOWN];
            if (in_mode_ticks < MODE_LOCKDOWN_DEESC_TICKS &&
                (compiled->auth_flags & MODE_AUTH_DEESC_ELAPSED) == 0) {
                compiled->reject_reason = MODE_REJECT_DEESC_WINDOW_ACTIVE;
                return ENV_DENY_EDGE_ILLEGAL;
            }
            if (((compiled->auth_flags & MODE_AUTH_PASSWORD) == 0 ||
                 (compiled->auth_flags & MODE_AUTH_SYSTEM_PROMPT) == 0)) {
                if (kernel_override) return ENV_DENY_NONE;
                compiled->reject_reason = MODE_REJECT_SYSTEM_PROMPT_REQUIRED;
                return ENV_DENY_EDGE_ILLEGAL;
            }
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_GHOST && compiled->to_mode == MODE_SECURE) {
        source_ok = (compiled->source == TRANSITION_SOURCE_USER || kernel_override);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        if ((compiled->auth_flags & MODE_AUTH_PASSWORD) == 0 && !kernel_override) {
            compiled->reject_reason = MODE_REJECT_AUTH_REQUIRED;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    } else if (compiled->from_mode == MODE_GHOST && compiled->to_mode == MODE_LOCKDOWN) {
        source_ok = (compiled->source == TRANSITION_SOURCE_KERNEL ||
                     compiled->source == TRANSITION_SOURCE_DAEMON);
        if (!source_ok) {
            compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
            return ENV_DENY_EDGE_ILLEGAL;
        }
        return ENV_DENY_NONE;
    }

    compiled->reject_reason = MODE_REJECT_EDGE_ILLEGAL;
    return ENV_DENY_EDGE_ILLEGAL;
}

static bool env_apply_transition(const env_compiled_transition_t* compiled, env_apply_snapshot_t* snapshot) {
    mode_id_t current = kernel_mode_state.current_mode;
    bool secure_context = false;

    if (!snapshot) return false;

    snapshot->prev_mode = kernel_mode_state.previous_mode;
    snapshot->current_mode = current;
    snapshot->mode_mask = kernel_mode_state.global_mode_mask;

    switch (current) {
        case MODE_GHOST:
        case MODE_LOCKDOWN:
        case MODE_SECURE:
        case MODE_CASUAL:
        default:
            break;
    }

    kernel_mode_state.previous_mode = current;
    kernel_mode_state.current_mode = compiled->to_mode;
    kernel_mode_state.global_mode_mask = (uint8_t)(1U << compiled->to_mode);
    kernel_mode_state.mode_enter_tick[(uint8_t)compiled->to_mode] = scheduler_get_global_tick();
    if (compiled->to_mode == MODE_LOCKDOWN) {
        kernel_mode_state.prompt_auth_failures = 0;
    }

    kernel_mode_state.stats.transitions_total++;
    if (compiled->source == TRANSITION_SOURCE_KERNEL) kernel_mode_state.stats.transitions_auto++;
    else kernel_mode_state.stats.transitions_manual++;
    __atomic_fetch_add(&kernel_mode_state.security_epoch, 1, __ATOMIC_RELAXED);
    
    // Day 12 Audit: Rotate seed and strike phase shift
    audit_rotate_seed(compiled->to_mode, mode_get_security_epoch());
    audit_meta_t meta = {0};
    meta.sched.tid = (uint32_t)compiled->source;
    meta.sched.auth_status = compiled->auth_flags;
    audit_strike(AUDIT_EVENT_PHASE_SHIFT, AUDIT_RESULT_OK, (uint64_t)compiled->to_mode << 32 | (uint64_t)current, meta);

    scheduler_on_mode_transition(current, compiled->to_mode, mode_get_security_epoch());

    mode_enter_secure_context();
    secure_context = true;

    if (compiled->ghost_flush_required) {
        invpcid_flush_all();
        klog_info("SCHED_GHOST_FLUSH enter=%u exit=%u",
                  compiled->to_mode == MODE_GHOST ? 1U : 0U,
                  current == MODE_GHOST ? 1U : 0U);
    }

    ocular_bleach();

    switch (compiled->to_mode) {
        case MODE_GHOST:
        case MODE_LOCKDOWN:
        case MODE_SECURE:
        case MODE_CASUAL:
        default:
            break;
    }

    if (secure_context) {
        mode_exit_secure_context();
    }

    env_marker_apply(compiled);
    return true;
}

static void env_rollback_transition(const env_compiled_transition_t* compiled, const env_apply_snapshot_t* snapshot) {
    if (!snapshot) return;

    kernel_mode_state.previous_mode = snapshot->prev_mode;
    kernel_mode_state.current_mode = snapshot->current_mode;
    kernel_mode_state.global_mode_mask = snapshot->mode_mask;
    env_marker_rollback(compiled);
}

/* --- Public API Implementation --- */

const char* mode_get_name(mode_id_t mode) {
    switch (mode) {
        case MODE_VOID:     return "VOID";
        case MODE_CASUAL:   return "CASUAL";
        case MODE_SECURE:   return "SECURE";
        case MODE_LOCKDOWN: return "LOCKDOWN";
        case MODE_GHOST:    return "GHOST";
        default:            return "UNKNOWN";
    }
}

mode_id_t mode_get_current(void) {
    return kernel_mode_state.current_mode;
}

uint8_t mode_get_current_mask(void) {
    return kernel_mode_state.global_mode_mask;
}

bool mode_is_secure(void) {
    mode_id_t m = kernel_mode_state.current_mode;
    return (m == MODE_SECURE || m == MODE_LOCKDOWN);
}

uint64_t mode_get_security_epoch(void) {
    return __atomic_load_n(&kernel_mode_state.security_epoch, __ATOMIC_RELAXED);
}

void mode_init(void) {
    mode_id_t restore_mode = MODE_CASUAL;
    if (kernel_mode_state.current_mode != MODE_VOID) {
        return; // Idempotent: Already initialized
    }

        
    // 1. Zero state
    memset(&kernel_mode_state, 0, sizeof(struct mode_state));
    boot_record_load_persistent();
    
    // 2. Set VOID initially (explicitly, though memset did it)
    kernel_mode_state.current_mode = MODE_VOID;
    kernel_mode_state.previous_mode = MODE_VOID;
    kernel_mode_state.transition_lock = 0;
    kernel_mode_state.mode_enter_tick[MODE_VOID] = scheduler_get_global_tick();
    
    // 3. Transition to CASUAL (The Genesis Event)
    mode_request_transition_ex(MODE_CASUAL, TRANSITION_SOURCE_KERNEL, MODE_AUTH_AUTOMATIC | MODE_AUTH_BOOT_INTENT);

    if (boot_record_load_mode(&restore_mode) && restore_mode != MODE_CASUAL) {
        if (restore_mode == MODE_SECURE) {
            mode_request_transition_ex(MODE_SECURE,
                                       TRANSITION_SOURCE_KERNEL,
                                       MODE_AUTH_AUTOMATIC | MODE_AUTH_BOOT_INTENT);
            kprintf("[MODE_BOOT_RESTORE] restored=SECURE source=boot_record\n");
        } else if (restore_mode == MODE_LOCKDOWN) {
            mode_request_transition_ex(MODE_LOCKDOWN,
                                       TRANSITION_SOURCE_KERNEL,
                                       MODE_AUTH_AUTOMATIC | MODE_AUTH_BOOT_INTENT);
            kprintf("[MODE_BOOT_RESTORE] restored=LOCKDOWN source=boot_record\n");
        } else {
            kprintf("[MODE_BOOT_RESTORE] restored=CASUAL source=fallback reason=unsupported\n");
        }
    }
}

int mode_request_transition(mode_id_t target_mode, transition_source_t source) {
    return mode_request_transition_ex(target_mode, source, mode_default_auth_flags(source));
}

int mode_request_transition_ex(mode_id_t target_mode, transition_source_t source, uint32_t auth_flags) {
    uint64_t flags = spinlock_irqsave(&kernel_mode_state.transition_lock);
    mode_id_t current = kernel_mode_state.current_mode;
    env_compiled_transition_t compiled;
    env_apply_snapshot_t snapshot;
    envelope_deny_t deny;
    bool apply_ok;

    if (current == target_mode) {
        spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
        return 0;
    }

    kprintf("[MODE_LEGACY_SHIM] from=%u to=%u src=%u auth=0x%x\n",
            (unsigned)current, (unsigned)target_mode, (unsigned)source, (unsigned)auth_flags);

    if (!env_compile_transition(current, target_mode, source, auth_flags, &compiled)) {
        spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
        return -1;
    }

    deny = env_verify_transition(&compiled);
    env_marker_verify(&compiled, deny);
    if (deny != ENV_DENY_NONE) {
        fate_log_transition_event(current, target_mode, source, FATE_RESULT_REJECTED, compiled.reject_reason);
        env_marker_attest(&compiled, FATE_RESULT_REJECTED, deny);
        spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
        return -1;
    }

    apply_ok = env_apply_transition(&compiled, &snapshot);
    if (!apply_ok) {
        env_rollback_transition(&compiled, &snapshot);
        compiled.reject_reason = MODE_REJECT_EDGE_ILLEGAL;
        fate_log_transition_event(current, target_mode, source, FATE_RESULT_REJECTED, compiled.reject_reason);
        env_marker_attest(&compiled, FATE_RESULT_REJECTED, ENV_DENY_NONE);
        spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
        return -1;
    }

    compiled.reject_reason = MODE_REJECT_NONE;
    fate_log_transition_event(current, target_mode, source, FATE_RESULT_ACCEPTED, MODE_REJECT_NONE);
    if (compiled.to_mode == MODE_CASUAL || compiled.to_mode == MODE_SECURE || compiled.to_mode == MODE_LOCKDOWN) {
        boot_record_store_mode(compiled.to_mode);
    }
    env_marker_attest(&compiled, FATE_RESULT_ACCEPTED, ENV_DENY_NONE);
    spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
    return 0;
}

mode_id_t mode_get_previous(void) {
    return kernel_mode_state.previous_mode;
}

int mode_get_history(struct mode_transition *buffer, size_t count) {
    return mode_get_history_filtered(buffer, count, FATE_READ_ALL);
}

int mode_get_history_filtered(struct mode_transition *buffer, size_t count, fate_read_mode_t mode) {
    uint64_t flags = spinlock_irqsave(&kernel_mode_state.transition_lock);
    
    size_t total_available = (kernel_mode_state.fate_head_index < MODE_HISTORY_SIZE) ?
                             (size_t)kernel_mode_state.fate_head_index : (size_t)MODE_HISTORY_SIZE;

    size_t copied = 0;
    for (size_t i = 0; i < total_available && copied < count; i++) {
        uint64_t idx = (kernel_mode_state.fate_head_index - 1 - i + MODE_HISTORY_SIZE) % MODE_HISTORY_SIZE;
        struct mode_transition* rec = &kernel_mode_state.fate_history[idx];
        if (!fate_record_matches_filter(rec, mode)) continue;
        memcpy(&buffer[copied], rec, sizeof(struct mode_transition));
        copied++;
    }
    
    spinlock_irqrestore(&kernel_mode_state.transition_lock, flags);
    return (int)copied;
}

void mode_enter_secure_context(void) {
    // Switch to the dedicated Secure Kernel PCID (4095)
    // This provides a clean TLB slate for sensitive operations.
    // Use the current PML4 but a specialized PCID.
    vmm_switch(read_cr3() & ~0xFFFULL, PCID_KERNEL_SECURE, MODE_KERNEL);
    klog_debug("[MODE] Entered PCID_KERNEL_SECURE context.");
}

void mode_exit_secure_context(void) {
    // Return to the standard Kernel PCID (0)
    vmm_switch(read_cr3() & ~0xFFFULL, PCID_KERNEL, MODE_KERNEL);
    klog_debug("[MODE] Returned to PCID_KERNEL context.");
}

security_color_t mode_to_color(mode_id_t mode) {
    switch (mode) {
        case MODE_VOID:     return COLOR_VOID;
        case MODE_CASUAL:   return COLOR_CASUAL;
        case MODE_SECURE:   return COLOR_SECURE;
        case MODE_LOCKDOWN: return COLOR_LOCKDOWN;
        case MODE_GHOST:    return COLOR_GHOST;
        case MODE_KERNEL:   return COLOR_VOID; // Kernel allocations can use COLOR_VOID, or a dedicated COLOR_KERNEL if defined in pmm.h
        default:            return COLOR_VOID; // Default to a safe, neutral color
    }
}
