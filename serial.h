#ifndef _SERIAL_H
#define _SERIAL_H

#include "hd6301.h"
#include "mem.h"

int serial_init(const char *tty_device);
void serial_execute(hd6301_t *master_mcu, mem_t *master_mem);

#endif /* _SERIAL_H */
