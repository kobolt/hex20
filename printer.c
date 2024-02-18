#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "hd6301.h"
#include "mem.h"

#define PULSE_TIMING 368 /* 600us / (1000000us / 612900Hz) */
#define DOTS 144

static uint32_t head_pos = 0;
static char dot_line[DOTS];
static FILE *printer_output_fh = NULL;



static void printer_exit_handler(void)
{
  if (printer_output_fh != NULL) {
    fclose(printer_output_fh);
    printer_output_fh = NULL;
  }
}



int printer_init(const char *filename)
{
  printer_output_fh = fopen(filename, "a"); /* Append */
  if (printer_output_fh == NULL) {
    return -1;
  }

  atexit(printer_exit_handler);
  return 0;
}



void printer_execute(hd6301_t *slave_mcu, mem_t *slave_mem)
{
  static uint16_t sync_catchup = 0;
  static uint16_t sync_counter = 0;

  if (printer_output_fh == NULL) {
    return;
  }

  while (slave_mcu->sync_counter != sync_catchup) {
    /* Check motor power output (P14) port. */
    if ((slave_mem->ram[HD6301_REG_PORT_1] & 0x10) == 0) {
      sync_counter++;
      if (sync_counter > PULSE_TIMING) {
        /* Toggle timing signal (TS) input (P17) port. */
        if (slave_mem->ram[HD6301_REG_PORT_1] & 0x80) {
          slave_mem->ram[HD6301_REG_PORT_1] &= ~0x80;
        } else {
          slave_mem->ram[HD6301_REG_PORT_1] |= 0x80;
        }

        /* Set reset signal (RS) input (P16) port. */
        if (head_pos < 72) {
          slave_mem->ram[HD6301_REG_PORT_1] |= 0x40;
        } else {
          slave_mem->ram[HD6301_REG_PORT_1] &= ~0x40;
        }

        if (head_pos >= 2 && head_pos < (DOTS + 2)) {
          if (slave_mem->ram[HD6301_REG_PORT_1] & 0x01) {
            dot_line[((head_pos - 2) / 4) + 108] = '#';
          } else if (slave_mem->ram[HD6301_REG_PORT_1] & 0x02) {
            dot_line[((head_pos - 2) / 4) + 72]  = '#';
          } else if (slave_mem->ram[HD6301_REG_PORT_1] & 0x04) {
            dot_line[((head_pos - 2) / 4) + 36]  = '#';
          } else if (slave_mem->ram[HD6301_REG_PORT_1] & 0x08) {
            dot_line[((head_pos - 2) / 4)]       = '#';
          }

        } else if (head_pos == (DOTS + 2)) {
          /* All dots printed, output to file. */
          for (int i = 0; i < DOTS; i++) {
            if (dot_line[i] != 0) {
              fputc(dot_line[i], printer_output_fh);
            } else {
              fputc(' ', printer_output_fh);
            }
            dot_line[i] = 0;
          }
          fputc('\n', printer_output_fh);
          fflush(printer_output_fh);
        }

        head_pos++;
        if (head_pos >= 252) {
          head_pos = 0;
        }
        sync_counter = 0;
      }
    }

    sync_catchup++;
  }
}



