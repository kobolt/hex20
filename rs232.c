#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "hd6301.h"
#include "mem.h"
#include "panic.h"



static FILE *rs232_fh = NULL;
static int rs232_bit_state = -1; /* Start at init state after power up. */
static int rs232_byte = 0;
static bool rs232_eof = false;



int rs232_transmit_file(const char *filename)
{
  if (rs232_fh != NULL) {
    return -2; /* Transfer already in progress. */
  }

  rs232_fh = fopen(filename, "r");
  if (rs232_fh == NULL) {
    return -1; /* File not found. */
  }

  rs232_eof = false;
  return 0;
}



void rs232_execute(hd6301_t *slave_mcu, mem_t *slave_mem)
{
  if (rs232_fh == NULL) {
    return;
  }

  if (slave_mcu->sync_counter > 512) { /* Synchronized to 1200 baud. */
    slave_mcu->sync_counter = 0;

    switch (rs232_bit_state) {
    case -1: /* Init */
      slave_mem->ram[HD6301_REG_PORT_2] |= 1;
      break;

    case 0: /* Start Bit */
      rs232_byte = fgetc(rs232_fh);
      if (rs232_byte == EOF) {
        rs232_eof = true;
        rs232_byte = 0x1A; /* EOF */
      }
      slave_mem->ram[HD6301_REG_PORT_2] &= ~1;
      break;

    case 1: /* Bits of Byte */
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
      if ((rs232_byte >> (rs232_bit_state - 1)) & 1) {
        slave_mem->ram[HD6301_REG_PORT_2] |= 1;
      } else {
        slave_mem->ram[HD6301_REG_PORT_2] &= ~1;
      }
      break;

    case 9: /* Stop Bit */
    case 10: /* Idle Bit (Needed!) */
      slave_mem->ram[HD6301_REG_PORT_2] |= 1;
      break;
    }

    rs232_bit_state++;
    if (rs232_bit_state >= 11) {
      rs232_bit_state = 0;
      if (rs232_eof) {
        fclose(rs232_fh);
        rs232_fh = NULL;
      }
    }
  }
}



