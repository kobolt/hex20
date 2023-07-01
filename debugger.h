#ifndef _DEBUGGER_H
#define _DEBUGGER_H

#include <stdint.h>
#include <stdbool.h>
#include "hd6301.h"
#include "mem.h"

typedef enum {
  SCI_TRACE_DIR_MASTER_TO_SLAVE,
  SCI_TRACE_DIR_SLAVE_TO_MASTER,
  SCI_TRACE_DIR_MASTER_TO_EXT,
  SCI_TRACE_DIR_EXT_TO_MASTER,
} sci_trace_dir_t;

void debugger_init(void);
bool debugger(hd6301_t *master_mcu, hd6301_t *slave_mcu,
  mem_t *master_mem, mem_t *slave_mem);

void debugger_sci_trace_add(sci_trace_dir_t dir,
  uint8_t byte, uint16_t cycle);

#endif /* _DEBUGGER_H */
