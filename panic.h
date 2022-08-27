#ifndef _PANIC_H
#define _PANIC_H

#include <stdarg.h>
#include <stdbool.h>

extern bool debugger_break;
extern bool warp_mode;

void panic(const char *format, ...);

#endif /* _PANIC_H */
