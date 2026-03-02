# Repository Guidelines

## Project Structure & Module Organization
Reaper-OS is organized by kernel, userspace, shared ABI, and documentation:
- `kernel/`: microkernel sources (`*.c`, `interrupts.s`), headers in `kernel/include/`, linker script, ISO build assets.
- `user/`: init userspace runtime and Paradigm daemon (`user/paradigm/main.c`), userspace syscall wrappers in `user/lib/`.
- `shared/include/`: ABI contracts shared by kernel and userspace (`syscall.h`, `mode.h`, `capability.h`).
- `tools/`: runtime closure and matrix validation scripts (e.g., day-specific closure suites).
- `docs/`: architecture, conformance matrix, component contracts, and day reports.
- `limine/`: vendored bootloader source; treat as upstream-managed.

## Build, Test, and Development Commands
Run from repository root:
- `make -C user`: build userspace (`user/init.elf`).
- `make -C kernel`: build kernel (`kernel/kernel.elf`).
- `make -C kernel iso`: build bootable image (`kernel/reaper-os.iso`).
- `make -C kernel run`: boot in QEMU with serial logging.
- `make -C kernel verify_matrix`: mandatory 3-run runtime matrix gate.
- `make -C kernel verify_day28`: Day 28 strict-adoption closure validation.
- `./tools/run_day28_closure_suite.sh --runs 5`: extended repeat-run closure check.

## Coding Style & Naming Conventions
- Languages: freestanding C (`gnu11` kernel, `gnu99` user) and x86_64 assembly.
- Indentation: 4 spaces; follow existing K&R-style brace patterns.
- Naming: `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for macros/constants.
- Keep ABI updates synchronized across `shared/include/`, `kernel/`, and `user/`.
- Prefer small, auditable kernel functions and fail-closed validation paths.

## Testing Guidelines
- Primary evidence is runtime serial markers plus forbidden-marker absence.
- Add deterministic positive and negative probes for high-risk syscall paths.
- For closure changes, update matrix markers in `tools/run_law2_fate_matrix.sh` and conformance docs.
- Keep probes bounded; avoid unbounded retry loops in validation paths.

## Commit & Pull Request Guidelines
- Use concise, imperative commits with subsystem scope, e.g.:
  - `kernel: harden map/unmap validation`
  - `docs: add day28 closure contract`
- PRs should include:
  - purpose and impacted subsystems
  - exact commands executed
  - serial/log evidence for required markers
  - doc synchronization for contracts, reports, and conformance when behavior changes.

## Security & Configuration Notes
- Preserve fail-closed semantics for syscall validation and capability checks.
- Do not weaken required/forbidden runtime marker gates.
- Avoid hand-editing generated toolchain/build outputs (`build-gcc/`, `toolchain/`).
