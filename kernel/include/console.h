#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdarg.h>

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write_string(const char* str);
void kprintf(const char* fmt, ...);
void kvprintf(const char* fmt, va_list args);
void kpanic(const char* fmt, ...);
void kpanic_at(const char* file, int line, const char* msg);

#endif
