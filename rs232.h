#ifndef _RS232_H
#define _RS232_H

#include "hd6301.h"
#include "mem.h"

int rs232_load_file(const char *filename);
int rs232_save_file(const char *filename);
void rs232_execute(hd6301_t *master_mcu, mem_t *master_mem,
  hd6301_t *slave_mcu, mem_t *slave_mem);

#endif /* _RS232_H */
