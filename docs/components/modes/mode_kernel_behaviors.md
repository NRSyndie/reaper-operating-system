# Reaper-OS Kernel Behaviors by Mode (The Laws of Physics)

This document defines how the "Physical Laws" of the machine change depending on the current Reality (Mode). The Kernel (Voidborn) enforces these laws at the lowest level.

## 1. Physical Memory Manager (PMM)

*   **Casual (Reality 1):**
    *   **Allocation:** Standard `kalloc`.
    *   **Limits:** None (up to physical RAM).
    *   **Scrubbing:** Standard zeroing on allocation.

*   **Secure (Reality 2):**
    *   **Allocation:** Standard `kalloc` with Quotas.
    *   **Limits:** Per-process capping to prevent Denial of Service (DoS).
    *   **Scrubbing:** Aggressive zeroing on free (immediate) rather than lazy.

*   **Lockdown (Reality 3):**
    *   **Allocation:** **FROZEN.** New non-critical allocations are denied (`kalloc` returns `NULL`).
    *   **Emergency Pool:** Critical subsystems (logging, auth) draw from a pre-reserved `EMERGENCY_POOL`.
    *   **Rationale:** Prevent heap spraying and resource exhaustion attacks during active breach.

*   **Ghost (Reality 4):**
    *   **Allocation:** **Segregated.** Allocates *only* from `GHOST_POOL` pages (separate free list).
    *   **Tagging:** All pages tagged with `GHOST_BIT` in PMM metadata.
    *   **Exit:** Entire `GHOST_POOL` is cryptographically scrubbed (multi-pass overwrite) upon mode exit.

## 2. Virtual Memory Manager (VMM)

*   **Casual:**
    *   **Mapping:** Standard RWX permissions.
    *   **ASLR:** Standard KASLR/ASLR.

*   **Secure:**
    *   **Mapping:** `W^X` (Write XOR Execute) strictly enforced.
    *   **Guard Pages:** Doubled size for stack guard pages.

*   **Lockdown:**
    *   **Mapping:** **Immutable.** `mmap` and `mprotect` syscalls are disabled for existing processes.
    *   **Exceptions:** None. The memory map is sealed to prevent code injection.

*   **Ghost:**
    *   **Mapping:** **Shadow Page Tables.** Ghost processes run in a completely separate PML4 hierarchy.
    *   **Isolation:** Ghost page tables *cannot* map any physical address marked as "Host/Casual" (except shared kernel text).
    *   **Side-Channels:** PCID flushing forced on every context switch to/from Ghost to flush TLB completely.

## 3. Inter-Process Communication (IPC) (Future)

*   **Casual:**
    *   **Policy:** Unrestricted local IPC (Pipes, Shared Memory, Signals).
    *   **Performance:** Zero-copy optimization enabled.

*   **Secure:**
    *   **Policy:** Authenticated IPC only. Receiver must explicitly accept connections.
    *   **Logging:** Metadata (Sender, Receiver, Size, Timestamp) logged to Fate Strings.

*   **Lockdown:**
    *   **Policy:** **SILENCED.** All user-space IPC blocked.
    *   **Exceptions:** Only `Paradigm` daemon can speak to the Kernel (via special syscall).

*   **Ghost:**
    *   **Policy:** **Dimensional Wall.**
        *   Ghost ↔ Ghost: Allowed.
        *   Ghost ↔ Casual: **Hard Block.** No signals, no shared memory.
        *   Ghost ↔ Kernel: Minimal syscall subset only.

## 4. Networking (Future)

*   **Casual:**
    *   **Stack:** Standard TCP/IP.
    *   **Interfaces:** All interfaces up.

*   **Secure:**
    *   **Stack:** VPN Enforced (Drop traffic if VPN tunnel is down - "Killswitch").
    *   **Firewall:** Strict Egress filtering active.

*   **Lockdown:**
    *   **Stack:** **SEVERED.** NIC drivers put into D3 (Power Off) state if possible.
    *   **Syscalls:** `socket`, `connect`, `send` return `-ENETDOWN` immediately.

*   **Ghost:**
    *   **Stack:** Virtual Network Namespace (isolated).
    *   **Routing:** Forced via internal Tor Proxy (SOCKS5).
    *   **Leak Protection:** Direct NIC access forbidden. No raw sockets.

## 5. Filesystem (Future)

*   **Casual:**
    *   **Access:** Standard Read/Write permissions.

*   **Secure:**
    *   **Access:** System binaries mounted Read-Only. User home Read-Write.
    *   **Auditing:** `open`, `write`, `unlink` calls logged to Fate Strings.

*   **Lockdown:**
    *   **Access:** **GLOBAL READ-ONLY.**
    *   **Exception:** `/var/log` (Append-Only) for forensic logs.
    *   **Mounts:** `mount`/`umount` operations disabled.

*   **Ghost:**
    *   **Access:** **Ephemeral Overlay.**
    *   **Mechanism:** `overlayfs` mounted at `/ghost` (or similar mechanism).
    *   **Writes:** All writes go to the upper (RAM-based) layer.
    *   **Reads:** Can read lower layer (host) only if explicitly permitted (default: blocked).
    *   **Persistence:** None. Upper layer discarded on exit.
