#ifndef _PIEZO_H
#define _PIEZO_H

#include <stdbool.h>
#include "hd6301.h"
#include "mem.h"

int piezo_init(bool silence);
void piezo_execute(hd6301_t *slave_mcu, mem_t *slave_mem);

#endif /* _PIEZO_H */
