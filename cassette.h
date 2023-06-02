#ifndef _CASSETTE_H
#define _CASSETTE_H

#include "hd6301.h"
#include "mem.h"

int cassette_load_file(const char *filename);
int cassette_save_file(const char *filename);
void cassette_execute(hd6301_t *slave_mcu, mem_t *slave_mem);

#endif /* _CASSETTE_H */
