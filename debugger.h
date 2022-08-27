#ifndef _DEBUGGER_H
#define _DEBUGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "hd6301.h"
#include "mem.h"

void debugger_init(void);
bool debugger(hd6301_t *master_mcu, hd6301_t *slave_mcu,
  mem_t *master_mem, mem_t *slave_mem);

void debugger_sci_trace_add(bool master_to_slave,
  uint8_t byte, uint16_t cycle);

#endif /* _DEBUGGER_H */
