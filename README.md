# Reaper-OS

A security-focused microkernel operating system built from scratch in C and x86_64 assembly for the limine boot protocol.

Reaper-OS is organized around a minimal trusted computing base. The ReaperCore microkernel provides only mechanisms (memory, threads, capabilities, IPC, interrupts, hardware mapping); all policy lives in user space daemons. Access is enforced through unforgeable capabilities rather than ambient authority, with atomic mode transitions that reshape the system's security posture.

## Project layout

```
docs/      Architecture, conformance matrix, component contracts, development reports
kernel/    ReaperCore microkernel sources, headers, linker script, ISO build assets
user/      User space runtime (init) and the Paradigm daemon, syscall wrappers
shared/    ABI contracts shared by kernel and user space (syscall, mode, capability)
limine/    Vendored bootloader source (upstream-managed)
tools/     Runtime closure and validation suites
```

## Current kernel features

- Microkernel with GDT, IDT, interrupts, and CPU-state setup for x86_64
- Physical and virtual memory management (`pmm`/`vmm`), SLAB allocator
- Process, thread, and scheduler layer with SMP-aware design
- Capability-based access control (mint, copy, delete, revoke, delegate, retype)
- Single-gate syscall ABI (`SYS_GATE_CALL`) with 27 gate operation ids
- Mode system: Void, Casual, Secure, Lockdown, and Ghost realities
- Fate String auditing: an SMP-safe Lattice ring buffer with rolling hash chaining
- Hardware security: IOMMU/DMAR, CET, and PKU support
- On-chip hashing via BLAKE3
- Bootstrapping model: the kernel exposes a single Genesis capability from which the first user space address space and daemons are created, then that capability is destroyed

## Building

From the repository root:

```sh
# Build user space
make -C user

# Build the kernel (produces kernel/kernel.elf)
make -C kernel

# Build a bootable ISO
make -C kernel iso

# Boot in QEMU with serial logging
make -C kernel run
```

## Runtime validation

The kernel ships with a set of runtime closure and matrix suites used to gate changes:

```sh
# Mandatory 3-run runtime matrix gate
make -C kernel verify_matrix
```

See `tools/` for day-specific closure suites and `make help` targets in the kernel Makefile.

## Status

Actively developed. The kernel builds and links to `kernel.elf`; user space bootstrapping and daemon work are in progress.
