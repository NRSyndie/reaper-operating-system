# Reaper-OS: Gemini CLI Context

This file provides the necessary architectural, technical, and operational context for the Gemini CLI to assist in the development of Reaper-OS.

## 1. Project Overview
Reaper-OS is a security-focused, next-generation operating system built on the **ReaperCore** "Voidborn" microkernel. It employs a **Pentabrid Architecture** designed for extreme isolation and capability-based access control.

- **Core Philosophy:** "Primordial Absence" (The kernel starts with nothing and exposes a single Genesis Capability).
- **Microkernel (ReaperCore):** Handles only basic mechanisms: Memory Primitives, Threading, Capabilities, IPC, and Interrupt routing.
- **Userspace (Daemon Space):** All policies (Filesystems, Networking, Security) reside in userspace daemons.
- **Execution Modes:** The system operates in four primary modes (Casual, Secure, Lockdown, Ghost) enforced via capability templates.

## 2. Technical Architecture

### Pentabrid Layers
1. **Layer 0 (Voidborn Foundation):** Substrate of memory, time, and capabilities.
2. **Layer 1 (Quantum Split):** Division between Power (capabilities) and Permission (modes).
3. **Layer 2 (Universe Layer/Realities):** Isolated virtual universes (Realities) managed by Paradigm.
4. **Layer 3 (Fate Strings):** Cryptographically bound audit trails (append-only history).
5. **Layer 4 (Occult Contracts):** Binding, cryptographically enforced action contracts.

### System Call Interface (ABI)
The kernel exposes a minimal set of syscalls (defined in `shared/include/syscall.h`):
- `SYS_VOID_LOG`: Kernel logging.
- `SYS_CAP_INVOKE`: Invoke a capability (IPC or object method).
- `SYS_CAP_MINT`: Create a new capability from an existing one with restricted rights.
- `SYS_CAP_COPY`/`DELETE`/`REVOKE`: Manage capability handles.
- `SYS_MODE_QUERY`: Get the current system execution mode.
- `SYS_YIELD`/`EXIT`/`WAIT`: Thread management.
- `SYS_MAP`/`UNMAP`: Address space management.
- `SYS_FRAME_ALLOC`: Physical memory allocation.
- `SYS_LATTICE_CREATE`: Create a new "Reality" or address space.
- `SYS_CAP_RETYPE`: Change capability type (e.g. RAM -> PAGETABLE).
- `SYS_ATTUNE`: Block until lattice resonance (doorbell strike).
- `SYS_OCULAR_SET`: Define an Ocular Projection window for the display.

### Core Daemons
- **Paradigm:** The "Reality Orchestrator" and security authority. The first process spawned via the "Genesis Bridge".
- **Prism (Proposed):** The display daemon utilizing the Ocular Projection Engine.
- **Aegis:** Capability router and IPC firewall.
- **Archivist:** Filesystem and storage server.
- **Sage:** High-level memory policy manager.
- **Cipher:** Process, namespace, and sandbox manager.
- **Sentinel:** Monitoring, forensics, and threat engine.

### Unconventional Mechanisms
- **Resonance Pact (Doorbell IPC):** Asynchronous signaling via Page Fault "Strikes" on non-present Lattice pages. Zero-copy and low-overhead.
- **Ocular Projection Engine:** An asynchronous, kernel-managed compositor that projects user-space Lattices to the hardware framebuffer during idle cycles. Supports pixel-level reality isolation.
- **The Great Bleaching:** Proactive memory and hardware scrubbing (using SSE-accelerated `hyper_scrub`) integrated into every Reality Shift (Mode Transition).

## 3. Build and Development Environment

### Prerequisites
- **Cross-Compiler:** `x86_64-elf-gcc` (located at `/usr/local/cross/bin/x86_64-elf-` by default).
- **Bootloader:** Limine (bundled in the `limine/` directory).
- **ISO Tools:** `xorriso`.
- **Emulator:** `qemu-system-x86_64`.

### Key Commands
- **Build Kernel:** `make` (within the `kernel/` directory).
- **Build Userland:** `make` (within the `user/` directory).
- **Generate ISO:** `make iso` (orchestrates both kernel and userland builds).
- **Run Emulator:** `make run` (boots the ISO in QEMU with serial output to `serial.log`).
- **Clean:** `make clean`.

## 4. Source Structure
- `kernel/`: The ReaperCore microkernel source code.
    - `main.c`: Kernel entry point and boot-time test suite.
    - `genesis.c`: The "Genesis Bridge" that loads the Paradigm ELF.
    - `capability.c`: Core capability management logic.
    - `include/`: Kernel headers defining the internal API.
- `user/`: Initial user-space applications and libraries.
    - `paradigm/`: Source for the primary orchestrator daemon.
    - `lib/`: User-space runtime (`reaper.c`, etc.).
- `docs/`: Extensive project documentation and reports.
    - `project_vision_and_architecture.md`: The definitive architectural guide.
    - `reports/`: Historical progress reports (Day 1 to Day 12+).
- `limine/`: Bootloader source and binaries.

## 5. Development Conventions
- **Capability-First:** No resource access is permitted without an explicit capability.
- **Test-Driven Boot:** The `kernel_main` function includes a "Stable Test Suite" that verifies PMM, VMM, SLAB, and Capabilities before spawning userspace.
- **Klog:** A structured kernel logging system used for auditing and debugging.
- **Minimalism:** Avoid adding complex logic to the kernel; if it can be a daemon, it should be.

## 6. Current Roadmap Status
- **Epoch I (Completed):** Core kernel primitives (Memory, IPC, Threads, Capabilities).
- **Epoch II (In Progress):** Significant progress on Core Daemons, including Paradigm's evolution to a C-based daemon and initial user-space stability.
- **Next Steps:** Continued completion of Core Daemons (Aegis, Sentinel), refining Mode Transitions, and further development of Layer 3 (Fate Strings), which has seen initial implementation with Fatal Forensics (Auditing).
