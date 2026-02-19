# Repository Guidelines

## Project Structure & Module Organization
Reaper-OS is split into kernel, userland, shared ABI headers, docs, and vendored/toolchain code:
- `kernel/`: microkernel source (`*.c`, `*.s`), linker script, and `kernel/include/` headers.
- `user/`: initial userspace runtime and Paradigm daemon (`user/paradigm/main.c`).
- `shared/include/`: kernel/userspace ABI headers (syscalls, mode/capability contracts).
- `docs/`: architecture notes, reports, and logs.
- `limine/`: vendored bootloader source (treat as upstream-managed).
- `toolchain/`, `build-gcc/`: cross-compiler sources/build outputs (do not hand-edit generated files).

## Build, Test, and Development Commands
Run commands from the repo root:
- `make -C kernel`: build `kernel/kernel.elf`.
- `make -C user`: build `user/init.elf`.
- `make -C kernel iso`: build kernel + userland and pack `kernel/reaper-os.iso`.
- `make -C kernel run`: boot ISO in QEMU; serial output is written to `kernel/serial.log`.
- `make -C kernel clean && make -C user clean`: remove local build artifacts.
- `make -C kernel limine_build`: rebuild Limine binaries if boot assets are missing.

## Coding Style & Naming Conventions
- Language: freestanding C (`gnu11` in kernel, `gnu99` in user), plus x86_64 assembly.
- Indentation: 4 spaces; keep braces in existing K&R-style patterns.
- Naming: `snake_case` for functions/variables, `UPPER_SNAKE_CASE` for macros/constants, suffix structs with `_t` where already established (`process_t`).
- Keep shared ABI changes synchronized between `shared/include/` and both consumers (`kernel/`, `user/`).
- Prefer small, auditable functions in kernel paths; avoid policy-heavy logic in kernel space.

## Testing Guidelines
- Primary tests are boot-time kernel self-tests and userspace probes exercised during `make -C kernel run`.
- Review `kernel/serial.log` for pass/fail markers and syscall boundary checks from Paradigm.
- Add regression coverage by extending kernel self-test routines and deterministic userspace probes.

## Commit & Pull Request Guidelines
- This workspace does not include a top-level `.git` history, so project-specific commit conventions cannot be derived directly here.
- Use concise, imperative commit messages with scope prefixes (for example: `kernel: harden syscall pointer checks`).
- PRs should include: purpose, touched subsystems, exact test commands run, and relevant serial/log excerpts.
- If boot flow/UI behavior changes, include updated config snippets (for example `kernel/isofiles/limine.conf`) and screenshots/log evidence where applicable.
