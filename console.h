#ifndef _CONSOLE_H
#define _CONSOLE_H

#include <stdint.h>
#include "hd6301.h"
#include "mem.h"

typedef enum {
  CONSOLE_MODE_NONE         = 1,
  CONSOLE_MODE_CURSES_ASCII = 2,
  CONSOLE_MODE_CURSES_PIXEL = 3,
} console_mode_t;

typedef enum {
  CONSOLE_CHARSET_US,
  CONSOLE_CHARSET_FR,
  CONSOLE_CHARSET_DE,
  CONSOLE_CHARSET_GB,
  CONSOLE_CHARSET_DK,
  CONSOLE_CHARSET_SE,
  CONSOLE_CHARSET_IT,
  CONSOLE_CHARSET_ES,
} console_charset_t;

void console_pause(void);
void console_resume(void);
void console_exit(void);
int console_init(console_mode_t mode, console_charset_t);
void console_execute(hd6301_t *cpu, mem_t *mem);

void console_lcd_select(uint8_t value);
void console_lcd_data(uint8_t value);

#endif /* _CONSOLE_H */
