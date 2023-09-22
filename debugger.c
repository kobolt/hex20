#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "debugger.h"
#include "hd6301.h"
#include "mem.h"
#include "rs232.h"
#include "cassette.h"
#include "panic.h"



#define DEBUGGER_ARGS 2
#define SCI_TRACE_BUFFER_SIZE 1024

typedef struct sci_trace_t {
  bool used;
  sci_trace_dir_t dir;
  uint8_t byte;
  uint16_t cycle;
} sci_trace_t;

static int sci_trace_buffer_index;
static sci_trace_t sci_trace_buffer[SCI_TRACE_BUFFER_SIZE];



static void sci_trace_init(void)
{
  for (int i = 0; i < SCI_TRACE_BUFFER_SIZE; i++) {
    sci_trace_buffer[i].used = false;
  }
  sci_trace_buffer_index = 0;
}



static char *sci_trace_dir_text(sci_trace_dir_t dir)
{
  switch (dir) {
  case SCI_TRACE_DIR_MASTER_TO_SLAVE:
    return "Master  --> Slave";
  case SCI_TRACE_DIR_SLAVE_TO_MASTER:
    return "Master <--  Slave";
  case SCI_TRACE_DIR_MASTER_TO_EXT:
    return "Master  --> External";
  case SCI_TRACE_DIR_EXT_TO_MASTER:
    return "Master <--  External";
  default:
    return "???";
  }
}



static void sci_trace_dump(FILE *fh)
{
  for (int i = sci_trace_buffer_index;
           i < SCI_TRACE_BUFFER_SIZE; i++) {
    if (sci_trace_buffer[i].used) {
      fprintf(fh, "%04x : %s : 0x%02x\n",
      sci_trace_buffer[i].cycle,
      sci_trace_dir_text(sci_trace_buffer[i].dir),
      sci_trace_buffer[i].byte);
    }
  }
  for (int i = 0; i < sci_trace_buffer_index; i++) {
    if (sci_trace_buffer[i].used) {
      fprintf(fh, "%04x : %s : 0x%02x\n",
      sci_trace_buffer[i].cycle,
      sci_trace_dir_text(sci_trace_buffer[i].dir),
      sci_trace_buffer[i].byte);
    }
  }
}



void debugger_sci_trace_add(sci_trace_dir_t dir,
  uint8_t byte, uint16_t cycle)
{
  sci_trace_buffer[sci_trace_buffer_index].used = true;
  sci_trace_buffer[sci_trace_buffer_index].dir = dir;
  sci_trace_buffer[sci_trace_buffer_index].byte = byte;
  sci_trace_buffer[sci_trace_buffer_index].cycle = cycle;
  sci_trace_buffer_index++;
  if (sci_trace_buffer_index >= SCI_TRACE_BUFFER_SIZE) {
    sci_trace_buffer_index = 0;
  }
}



static void port_dump_set(FILE *fh, int no, uint8_t direction, uint8_t value)
{
  for (int i = 0; i < 8; i++) {
    fprintf(fh, "  P%d%d %c--%c %d\n", no, i,
      ((direction >> i) & 1) ? ' ' : '<', /* In */
      ((direction >> i) & 1) ? '>' : ' ', /* Out */
      (value >> i) & 1);
  }
}



static void port_dump(FILE *fh, mem_t *mem)
{
  /* Ports */
  port_dump_set(fh, 1, mem->ram[HD6301_REG_DDR_1], mem->ram[HD6301_REG_PORT_1]);
  port_dump_set(fh, 2, mem->ram[HD6301_REG_DDR_2], mem->ram[HD6301_REG_PORT_2]);
  port_dump_set(fh, 3, mem->ram[HD6301_REG_DDR_3], mem->ram[HD6301_REG_PORT_3]);
  port_dump_set(fh, 4, mem->ram[HD6301_REG_DDR_4], mem->ram[HD6301_REG_PORT_4]);

  /* TCSR */
  fprintf(fh, "  TCSR.OLVL : %d\n",  mem->ram[HD6301_REG_TCSR]       & 1);
  fprintf(fh, "  TCSR.IEDG : %d\n", (mem->ram[HD6301_REG_TCSR] >> 1) & 1);
  fprintf(fh, "  TCSR.ETOI : %d\n", (mem->ram[HD6301_REG_TCSR] >> 2) & 1);
  fprintf(fh, "  TCSR.EOCI : %d\n", (mem->ram[HD6301_REG_TCSR] >> 3) & 1);
  fprintf(fh, "  TCSR.EICI : %d\n", (mem->ram[HD6301_REG_TCSR] >> 4) & 1);
  fprintf(fh, "  TCSR.TOF  : %d\n", (mem->ram[HD6301_REG_TCSR] >> 5) & 1);
  fprintf(fh, "  TCSR.OCF  : %d\n", (mem->ram[HD6301_REG_TCSR] >> 6) & 1);
  fprintf(fh, "  TCSR.ICF  : %d\n", (mem->ram[HD6301_REG_TCSR] >> 7) & 1);

  /* P3CSR */
  fprintf(fh, "  P3CSR.LATCH : %d\n", (mem->ram[HD6301_REG_TCSR] >> 3) & 1);
  fprintf(fh, "  P3CSR.OSS   : %d\n", (mem->ram[HD6301_REG_TCSR] >> 4) & 1);
  fprintf(fh, "  P3CSR.IS3I  : %d\n", (mem->ram[HD6301_REG_TCSR] >> 6) & 1);
  fprintf(fh, "  P3CSR.IS3   : %d\n", (mem->ram[HD6301_REG_TCSR] >> 7) & 1);

  /* RMCR */
  fprintf(fh, "  RMCR.SS0 : %d\n",  mem->ram[HD6301_REG_RMCR]       & 1);
  fprintf(fh, "  RMCR.SS1 : %d\n", (mem->ram[HD6301_REG_RMCR] >> 1) & 1);
  fprintf(fh, "  RMCR.CC0 : %d\n", (mem->ram[HD6301_REG_RMCR] >> 2) & 1);
  fprintf(fh, "  RMCR.CC1 : %d\n", (mem->ram[HD6301_REG_RMCR] >> 3) & 1);

  /* TRCSR */
  fprintf(fh, "  TRCSR.WU   : %d\n",  mem->ram[HD6301_REG_TRCSR]       & 1);
  fprintf(fh, "  TRCSR.TE   : %d\n", (mem->ram[HD6301_REG_TRCSR] >> 1) & 1);
  fprintf(fh, "  TRCSR.TIE  : %d\n", (mem->ram[HD6301_REG_TRCSR] >> 2) & 1);
  fprintf(fh, "  TRCSR.RE   : %d\n", (mem->ram[HD6301_REG_TRCSR] >> 3) & 1);
  fprintf(fh, "  TRCSR.RIE  : %d\n", (mem->ram[HD6301_REG_TRCSR] >> 4) & 1);
  fprintf(fh, "  TRCSR.TDRE : %d\n", (mem->ram[HD6301_REG_TRCSR] >> 5) & 1);
  fprintf(fh, "  TRCSR.ORFE : %d\n", (mem->ram[HD6301_REG_TRCSR] >> 6) & 1);
  fprintf(fh, "  TRCSR.RDRF : %d\n", (mem->ram[HD6301_REG_TRCSR] >> 7) & 1);

  /* RAM Control */
  fprintf(fh, "  RAM.RAME : %d\n", (mem->ram[HD6301_REG_RAM_CTRL] >> 6) & 1);
  fprintf(fh, "  RAM.STBY : %d\n", (mem->ram[HD6301_REG_RAM_CTRL] >> 7) & 1);

  /* Various */
  fprintf(fh, "  FRC : 0x%02x%02x\n",
    mem->ram[HD6301_REG_FRC_HIGH], mem->ram[HD6301_REG_FRC_LOW]);
  fprintf(fh, "  OCR : 0x%02x%02x\n",
    mem->ram[HD6301_REG_OCR_HIGH], mem->ram[HD6301_REG_OCR_LOW]);
  fprintf(fh, "  ICR : 0x%02x%02x\n",
    mem->ram[HD6301_REG_ICR_HIGH], mem->ram[HD6301_REG_ICR_LOW]);
  fprintf(fh, "  RDR : 0x%02x\n", mem->ram[HD6301_REG_RDR]);
  fprintf(fh, "  TDR : 0x%02x\n", mem->ram[HD6301_REG_TDR]);
}



static void variable_ktb_dump(FILE *fh, mem_t *mem, uint16_t address)
{
  for (int i = 0; i < 10; i++) {
    fprintf(fh, "    %c%c%c%c%c%c%c%c\n",
       mem->ram[address+i]       & 1 ? '1' : '0',
      (mem->ram[address+i] >> 1) & 1 ? '1' : '0',
      (mem->ram[address+i] >> 2) & 1 ? '1' : '0',
      (mem->ram[address+i] >> 3) & 1 ? '1' : '0',
      (mem->ram[address+i] >> 4) & 1 ? '1' : '0',
      (mem->ram[address+i] >> 5) & 1 ? '1' : '0',
      (mem->ram[address+i] >> 6) & 1 ? '1' : '0',
      (mem->ram[address+i] >> 7) & 1 ? '1' : '0');
  }
}



static void variable_dump(FILE *fh, mem_t *mem)
{
  fprintf(fh, "Keyboard:\n");
  fprintf(fh, "  KSTKSZ: %d\n", mem->ram[0x140]);
  fprintf(fh, "  KICNT1: %d\n", mem->ram[0x141]);
  fprintf(fh, "  KICNT2: %d\n", mem->ram[0x142]);
  fprintf(fh, "  KICNTM: %d\n", (mem->ram[0x143] * 0x100) + mem->ram[0x144]);
  fprintf(fh, "  NEWKTB:\n");
  variable_ktb_dump(fh, mem, 0x145);
  fprintf(fh, "  OLDKTB:\n");
  variable_ktb_dump(fh, mem, 0x14F);
  fprintf(fh, "  CHKKTB:\n");
  variable_ktb_dump(fh, mem, 0x159);
  fprintf(fh, "  KYISAD: 0x%02x%02x\n", mem->ram[0x163], mem->ram[0x164]);
  fprintf(fh, "  KYISFL: 0x%02x\n", mem->ram[0x165]);
  fprintf(fh, "  KYISCN: %d\n", mem->ram[0x166]);
  fprintf(fh, "  KYISPN: %d\n", mem->ram[0x167]);
  fprintf(fh, "  STKCNT: %d\n", mem->ram[0x168]);
  fprintf(fh, "  KEYMOD: 0x%02x\n", mem->ram[0x169]);
  fprintf(fh, "  ONKFLG: 0x%02x\n", mem->ram[0x16A]);
  fprintf(fh, "  KPRFLG: %d\n", mem->ram[0x16B]);
  fprintf(fh, "  KEYRPT: %d\n", mem->ram[0x16C]);
  fprintf(fh, "  CKEYRD: 0x%02x%02x\n", mem->ram[0x16D], mem->ram[0x16E]);
  fprintf(fh, "  KYISTK:\n");
  for (int i = 0; i < 18; i++) {
    if (i % 9 == 0) {
      fprintf(fh, "    ");
    }
    fprintf(fh, "0x%02x,", mem->ram[0x16F + i]);
    if (i % 9 == 8) {
      fprintf(fh, "\n");
    }
  }
}



void debugger_init(void)
{
  sci_trace_init();
}



static void debugger_help(void)
{
  fprintf(stdout, "Debugger Commands:\n");
  fprintf(stdout, "  q        - Quit\n");
  fprintf(stdout, "  ? | h    - Help\n");
  fprintf(stdout, "  c        - Continue\n");
  fprintf(stdout, "  s        - Step\n");
  fprintf(stdout, "  w        - Warp Mode Toggle\n");
  fprintf(stdout, "  t        - Master MCU Trace\n");
  fprintf(stdout, "  r        - Slave MCU Trace\n");
  fprintf(stdout, "  m        - Master MCU RAM\n");
  fprintf(stdout, "  n        - Slave MCU RAM\n");
  fprintf(stdout, "  p        - Master MCU Ports\n");
  fprintf(stdout, "  o        - Slave MCU Ports\n");
  fprintf(stdout, "  x        - MCU Internals\n");
  fprintf(stdout, "  v        - Variables\n");
  fprintf(stdout, "  u        - SCI Trace\n");
  fprintf(stdout, "  l <file> - Load file into RS-232               ");
  fprintf(stdout, " - Prior: LOAD\"COM0:(48N1F)\"\n");
  fprintf(stdout, "  k <file> - Save file from RS-232               ");
  fprintf(stdout, " - After: SAVE\"COM0:(68N1F)\",A\n");
  fprintf(stdout, "  g <file> - Load file into External Cassette In ");
  fprintf(stdout, " - Prior: LOAD\"CAS1:\"\n");
  fprintf(stdout, "  f <file> - Save file from External Cassette Out");
  fprintf(stdout, " - After: SAVE\"CAS1:FILENAME\"\n");
}



bool debugger(hd6301_t *master_mcu, hd6301_t *slave_mcu,
  mem_t *master_mem, mem_t *slave_mem)
{
  char input[128];
  char *argv[DEBUGGER_ARGS];
  int argc;
  int result;

  fprintf(stdout, "\n");
  while (1) {
    fprintf(stdout, "%04x:%04x> ", master_mcu->counter, master_mcu->pc);

    if (fgets(input, sizeof(input), stdin) == NULL) {
      if (feof(stdin)) {
        exit(EXIT_SUCCESS);
      }
      continue;
    }

        if ((strlen(input) > 0) && (input[strlen(input) - 1] == '\n')) {
      input[strlen(input) - 1] = '\0'; /* Strip newline. */
    }

    argv[0] = strtok(input, " ");
    if (argv[0] == NULL) {
      continue;
    }

    for (argc = 1; argc < DEBUGGER_ARGS; argc++) {
      argv[argc] = strtok(NULL, " ");
      if (argv[argc] == NULL) {
        break;
      }
    }

    if (strncmp(argv[0], "q", 1) == 0) {
      exit(EXIT_SUCCESS);

    } else if (strncmp(argv[0], "?", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "h", 1) == 0) {
      debugger_help();

    } else if (strncmp(argv[0], "c", 1) == 0) {
      return false;

    } else if (strncmp(argv[0], "s", 1) == 0) {
      return true;

    } else if (strncmp(argv[0], "w", 1) == 0) {
      if (warp_mode) {
        fprintf(stdout, "Warp Mode: Off\n");
        warp_mode = false;
      } else {
        fprintf(stdout, "Warp Mode: On\n");
        warp_mode = true;
      }

    } else if (strncmp(argv[0], "t", 1) == 0) {
      fprintf(stdout, "Master Trace:\n");
      hd6301_trace_dump(stdout, 0);

    } else if (strncmp(argv[0], "r", 1) == 0) {
      fprintf(stdout, "Slave Trace:\n");
      hd6301_trace_dump(stdout, 1);

    } else if (strncmp(argv[0], "m", 1) == 0) {
      fprintf(stdout, "Master RAM:\n");
      mem_dump(stdout, master_mem, 0x0000, 0x7FFF);

    } else if (strncmp(argv[0], "n", 1) == 0) {
      fprintf(stdout, "Slave RAM:\n");
      mem_dump(stdout, slave_mem, 0x0000, 0x01FF);

    } else if (strncmp(argv[0], "p", 1) == 0) {
      fprintf(stdout, "Master Ports:\n");
      port_dump(stdout, master_mem);

    } else if (strncmp(argv[0], "o", 1) == 0) {
      fprintf(stdout, "Slave Ports:\n");
      port_dump(stdout, slave_mem);

    } else if (strncmp(argv[0], "x", 1) == 0) {
      hd6301_dump(stdout, master_mcu);
      hd6301_dump(stdout, slave_mcu);

    } else if (strncmp(argv[0], "v", 1) == 0) {
      variable_dump(stdout, master_mem);

    } else if (strncmp(argv[0], "u", 1) == 0) {
      sci_trace_dump(stdout);

    } else if (strncmp(argv[0], "l", 1) == 0) {
      if (argc >= 2) {
        result = rs232_load_file(argv[1]);
        if (result != 0) {
          fprintf(stdout, "Failed to load file into RS-232! Error Code: %d\n",
            result);
        }
      } else {
        fprintf(stdout, "Specify filename!\n");
      }

    } else if (strncmp(argv[0], "k", 1) == 0) {
      if (argc >= 2) {
        result = rs232_save_file(argv[1]);
        if (result != 0) {
          fprintf(stdout, "Failed to save file from RS-232! Error Code: %d\n",
            result);
        }
      } else {
        fprintf(stdout, "Specify filename!\n");
      }

    } else if (strncmp(argv[0], "f", 1) == 0) {
      if (argc >= 2) {
        result = cassette_save_file(argv[1]);
        if (result != 0) {
          fprintf(stdout, "Failed to save cassette file! Error Code: %d\n",
            result);
        }
      } else {
        fprintf(stdout, "Specify filename!\n");
      }

    } else if (strncmp(argv[0], "g", 1) == 0) {
      if (argc >= 2) {
        result = cassette_load_file(argv[1]);
        if (result != 0) {
          fprintf(stdout, "Failed to load cassette file! Error Code: %d\n",
            result);
        }
      } else {
        fprintf(stdout, "Specify filename!\n");
      }

    }
  }
}



