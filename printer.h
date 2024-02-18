#ifndef _PRINTER_H
#define _PRINTER_H

#include "hd6301.h"
#include "mem.h"

int printer_init(const char *filename);
void printer_execute(hd6301_t *slave_mcu, mem_t *slave_mem);

#endif /* _PRINTER_H */
