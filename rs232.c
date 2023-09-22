#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "hd6301.h"
#include "mem.h"
#include "panic.h"



static FILE *rs232_save_fh = NULL;
static int rs232_save_bit_state = 0;
static int rs232_save_byte = 0;

static FILE *rs232_load_fh = NULL;
static int rs232_load_bit_state = -1; /* Start at init state after power up. */
static int rs232_load_byte = 0;
static bool rs232_load_eof = false;



int rs232_load_file(const char *filename)
{
  if (rs232_load_fh != NULL) {
    return -2; /* Loading already in progress. */
  }

  rs232_load_fh = fopen(filename, "rb");
  if (rs232_load_fh == NULL) {
    return -1; /* File not found. */
  }

  rs232_load_eof = false;
  return 0;
}



int rs232_save_file(const char *filename)
{
  if (rs232_save_fh != NULL) {
    return -2; /* Saving already in progress. */
  }

  rs232_save_fh = fopen(filename, "wb");
  if (rs232_save_fh == NULL) {
    return -1; /* File not found. */
  }

  return 0;
}



void rs232_execute(hd6301_t *master_mcu, mem_t *master_mem,
  hd6301_t *slave_mcu, mem_t *slave_mem)
{
  static uint16_t sync_catchup = 0;
  static uint16_t sync_counter = 0;
  bool bit;

  while (slave_mcu->sync_counter != sync_catchup) {

    /* Saving */

    if (rs232_save_fh != NULL) {
      if (master_mcu->p21_set) {
        if (master_mem->ram[HD6301_REG_PORT_2] & 0x02) {
          bit = 1;
        } else {
          bit = 0;
        }

        switch (rs232_save_bit_state) {
        case 0: /* Wait for Start Bit */
          if (bit == 0) {
            rs232_save_byte = 0;
            rs232_save_bit_state++;
          }
          break;

        case 1: /* Bits of Byte */
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
          rs232_save_byte += bit << (rs232_save_bit_state - 1);
          rs232_save_bit_state++;
          break;

        case 9: /* Stop Bit */
          if (rs232_save_byte == 0x1A) { /* EOF */
            fclose(rs232_save_fh);
            rs232_save_fh = NULL;
          } else {
            fputc(rs232_save_byte, rs232_save_fh);
          }
          rs232_save_bit_state = 0;
          break;
        }

        master_mcu->p21_set = false;
      }
    }

    /* Loading */

    if (rs232_load_fh == NULL) {
      sync_catchup++;
      sync_counter = 512; /* Always primed! */
      continue;
    }

    sync_counter++;
    if (sync_counter > 512) { /* Synchronized to 1200 baud. */
      sync_counter = 0;

      switch (rs232_load_bit_state) {
      case -1: /* Init */
        slave_mem->ram[HD6301_REG_PORT_2] |= 1;
        break;

      case 0: /* Start Bit */
        rs232_load_byte = fgetc(rs232_load_fh);
        if (rs232_load_byte == EOF) {
          rs232_load_eof = true;
          rs232_load_byte = 0x1A; /* EOF */
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
        if ((rs232_load_byte >> (rs232_load_bit_state - 1)) & 1) {
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

      rs232_load_bit_state++;
      if (rs232_load_bit_state >= 11) {
        rs232_load_bit_state = 0;
        if (rs232_load_eof) {
          fclose(rs232_load_fh);
          rs232_load_fh = NULL;
        }
      }
    }

    sync_catchup++;
  }
}



