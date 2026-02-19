#ifndef INIT_H
#define INIT_H

// This macro places a function pointer into the .initcall section.
// The kernel's initcall mechanism will iterate over this section to call
// all registered initialization functions.
#define initcall(fn) \
    static void (*__initcall_##fn)(void) __attribute__((section(".initcall"), used)) = fn

// External declarations for the start and end of the initcall section,
// defined in the linker script.
extern char initcall_start[];
extern char initcall_end[];

#endif // INIT_H
