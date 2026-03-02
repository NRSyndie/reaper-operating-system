#ifndef REAPER_ENTRY_INTERNAL_H
#define REAPER_ENTRY_INTERNAL_H

#include <entry.h>
#include <thread.h>

typedef struct {
    lease_id_t lease_id;
    uint64_t   rip;
    uint64_t   rsp;
    thread_t*  thread;
} entry_compiled_t;

/* Entry Pipeline Stages */
bool entry_compile(thread_t* thread, uint64_t rip, uint64_t rsp, entry_compiled_t* out);
bool entry_verify(entry_compiled_t* compiled, const char** reject_reason);
void entry_apply(entry_compiled_t* compiled);
void entry_attest(entry_compiled_t* compiled);

/**
 * entry_pipeline_run: The Day 11 "Void Gate" entry pipeline.
 * Replaces the direct user_mode_jump.
 */
void entry_pipeline_run(thread_t* thread, uint64_t rip, uint64_t rsp);

#endif /* REAPER_ENTRY_INTERNAL_H */
