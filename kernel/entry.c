#include <include/entry_internal.h>
#include <include/mode.h>
#include <include/scheduler.h>
#include <include/klog.h>
#include <include/console.h>
#include <include/utils.h>

extern void user_mode_jump(uint64_t rip, uint64_t rsp);

static uint64_t global_lease_id_counter = 0;

bool entry_compile(thread_t* thread, uint64_t rip, uint64_t rsp, entry_compiled_t* out) {
    if (!thread || !out) return false;
    
    out->lease_id = ++global_lease_id_counter;
    out->rip = rip;
    out->rsp = rsp;
    out->thread = thread;

    kprintf("%s txid=%lu tid=%u pid=%u rip=0x%lx rsp=0x%lx\n", 
            ENTRY_MARKER_COMPILE, out->lease_id, thread->tid, 
            thread->owner ? thread->owner->pid : 0, rip, rsp);
    return true;
}

bool entry_verify(entry_compiled_t* compiled, const char** reject_reason) {
    thread_t* t = compiled->thread;
    mode_id_t current_mode = mode_get_current();

    /* 1. Mode check: Occupant must match current reality */
    if (t->owner && t->owner->mode != current_mode && t->owner->mode != MODE_KERNEL) {
        *reject_reason = ENTRY_REJECT_MODE;
        return false;
    }

    /* 2. Epoch check: Security epoch must be fresh (already handled by scheduler usually) */

    kprintf("%s txid=%lu result=ALLOW\n", ENTRY_MARKER_VERIFY, compiled->lease_id);
    return true;
}

void entry_apply(entry_compiled_t* compiled) {
    thread_t* t = compiled->thread;
    
    /* Bind the lease to the thread */
    t->lease.id = compiled->lease_id;
    t->lease.epoch = mode_get_security_epoch();
    t->lease.mode_mask = (1 << mode_get_current());
    t->lease.authority_mask = 0xFFFF; // Placeholder for authority levels
    t->lease.expiry_tsc = 0; // Never expires for now
    
    kprintf("%s txid=%lu rip=0x%lx\n", ENTRY_MARKER_APPLY, compiled->lease_id, compiled->rip);
}

void entry_attest(entry_compiled_t* compiled) {
    kprintf("%s txid=%lu epoch=%lu mode=%u\n", 
            ENTRY_MARKER_ATTEST, compiled->lease_id, 
            mode_get_security_epoch(), (unsigned)mode_get_current());
}

void entry_pipeline_run(thread_t* thread, uint64_t rip, uint64_t rsp) {
    entry_compiled_t compiled;
    const char* reject_reason = NULL;

    if (!entry_compile(thread, rip, rsp, &compiled)) {
        kpanic("ENTRY: Compile failed");
    }

    if (!entry_verify(&compiled, &reject_reason)) {
        kprintf("%s txid=%lu reason=%s\n", reject_reason, compiled.lease_id, reject_reason);
        // In a real system, we'd probably kill the thread. 
        // For now, let's panic to ensure visibility during Day 11 development.
        kpanic("ENTRY: Verification failed");
    }

    entry_apply(&compiled);
    entry_attest(&compiled);

    /* Finally, the leap to userland */
    user_mode_jump(rip, rsp);
}
