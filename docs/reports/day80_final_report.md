# Epoch III, Day 80: Slot 1 Step 4 Buffered IPC Queues

**Date:** Sunday, April 19, 2026  
**Status:** DONE (Slot 1 Step 4 landed)  
**Modules:** `kernel/include/ipc.h`, `kernel/syscall.c`, `kernel/capability.c`, `shared/include/syscall.h`, `kernel/main.c`, `user/lib/reaper.c`, `user/include/reaper.h`

## 1. Executive Summary
Implemented standard microkernel Slot 1 Step 4 buffered IPC queues. Upgraded the synchronous rendezvous endpoint model to support a 16-message circular buffer, split send/receive wait queues, and rights-aware capability invocation (`CAP_RIGHT_READ` for receive, `CAP_RIGHT_WRITE` for send). Enabled user-space endpoint creation via RAM-to-ENDPOINT retyping and added deterministic boot self-tests.

## 2. What Was Implemented
- **Kernel IPC Upgrades (`kernel/include/ipc.h`):**
    - Added `ipc_message_t` structure (4x `uint64_t` payload).
    - Expanded `ipc_endpoint_t` with a 16-slot circular buffer.
    - Split single `wait_head` into `send_head` and `recv_head` for clear role separation.
    - Added `spinlock_t lock` to `ipc_endpoint_t` for fine-grained synchronization.
- **Syscall Logic (`kernel/syscall.c`):**
    - Refactored `ipc_invoke_endpoint` to support asynchronous sends (buffering) and non-blocking receives.
    - Implemented rights enforcement: `CAP_RIGHT_WRITE` required for Send, `CAP_RIGHT_READ` for Receive.
    - Added support for explicit operation selection via the 5th syscall argument (`CAP_INVOKE_OPT_SEND/RECV`).
    - Handled complex queue transitions (waking blocked senders when a receiver drains the buffer).
- **Capability System (`kernel/capability.c`):**
    - Extended `cap_retype` to allow `CAP_TYPE_RAM` -> `CAP_TYPE_ENDPOINT` transitions.
    - Integrated endpoint initialization (zeroing and lock reset) into the retype path.
- **User Runtime (`user/lib/reaper.c`, `user/include/reaper.h`):**
    - Added `sys_cap_invoke` and `sys_cap_invoke_ex` to the user library.
- **Verification (`kernel/main.c`):**
    - Added `test_slot1_buffered_ipc()` boot self-test.
    - Validates: endpoint creation, async send (buffering), buffer count integrity, non-blocking receive, and payload/badge consistency.
    - Success marker: `[TEST] Slot 1 buffered IPC: SUCCESS.`

## 3. Why It Was Added
- Synchronous-only IPC creates high coupling and potential deadlocks in complex multi-daemon workflows.
- Buffered queues allow for fire-and-forget signaling and asynchronous event loops in user-space.
- Rights-aware invocation fixes a legacy security hole where any endpoint capability allowed both send and receive regardless of intent.

## 4. Verification Evidence
- [PASS] `make -C kernel -j2`
- [PASS] `kernel_main` success marker: `[TEST] Slot 1 buffered IPC: SUCCESS.`
- [PASS] `make -C kernel verify_matrix` (3/3) - existing invariants preserved.

## 5. Known Limitations / Follow-Up
- Call-chain propagation and IPC priority inheritance are deferred to later steps.
- Buffer size is currently fixed at 16; future iterations may support variable-sized buffers via scaled RAM retyping.
- Next planned item is Slot 1 Step 5: Memory path upgrades (demand paging, COW, huge pages).
