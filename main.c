#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <strings.h> /* strncasecmp() */
#include <limits.h> /* PATH_MAX */

#include "hd6301.h"
#include "mem.h"
#include "console.h"
#include "rs232.h"
#include "piezo.h"
#include "crc32.h"
#include "debugger.h"
#include "panic.h"



#define BASIC1_ROM  "basic1.rom"
#define BASIC2_ROM  "basic2.rom"
#define UTILITY_ROM "utility.rom"
#define MONITOR_ROM "monitor.rom"
#define SLAVE_ROM   "slave.rom"

#define BASIC1_10_CRC32  0x33fbb1ab
#define BASIC1_11_CRC32  0x4de0b4b6
#define BASIC2_10_CRC32  0x27d743ed
#define BASIC2_11_CRC32  0x10d6ae76
#define UTILITY_10_CRC32 0xf5cc8868
#define UTILITY_11_CRC32 0x26c203a1
#define MONITOR_10_CRC32 0xed7482c6
#define MONITOR_11_CRC32 0x101cb3e8
#define SLAVE_CRC32      0xb36f5b99



typedef enum {
  AUTOLOAD_NONE = 0,
  AUTOLOAD_FILE = 1,
  AUTOLOAD_CMD  = 2,
  AUTOLOAD_END  = 3,
} autoload_t;



static hd6301_t master_mcu;
static hd6301_t slave_mcu;
static mem_t master_mem;
static mem_t slave_mem;

bool debugger_break = false;
bool warp_mode = false;
static char panic_msg[80];



static void sig_handler(int sig)
{
  switch (sig) {
  case SIGALRM:
    return; /* Ignore */

  case SIGINT:
    debugger_break = true;
    return;
  }
}



void panic(const char *format, ...)
{
  va_list args;

  va_start(args, format);
  vsnprintf(panic_msg, sizeof(panic_msg), format, args);
  va_end(args);

  debugger_break = true;
}



static void rom_load(const char *directory, mem_t *mem,
  const char *filename, uint16_t address, size_t size,
  uint32_t crc1, uint32_t crc2)
{
  uint32_t crc;
  char path[PATH_MAX];

  snprintf(path, PATH_MAX, "%s/%s", (directory) ? directory : ".", filename);
  if (mem_load_from_file(mem, path, address) != 0) {
    fprintf(stdout, "Loading of ROM '%s' at 0x%04x failed!\n", path, address);
    exit(EXIT_FAILURE);
  }

  crc = crc32(&mem->ram[address], size);
  if (! (crc == crc1 || crc == crc2)) {
    fprintf(stdout, "ROM '%s' has invalid CRC32: %08x\n", filename, crc);
    exit(EXIT_FAILURE);
  }
}



static void display_help(const char *progname)
{
  fprintf(stdout, "Usage: %s <options> [BASIC text program]\n", progname);
  fprintf(stdout, "Options:\n"
    "  -h         Display this help.\n"
    "  -b         Break into debugger on start.\n"
    "  -w         Warp (full speed) mode.\n"
    "  -m MODE    Set MODE for console.\n"
    "  -c LANG    Use LANG character set.\n"
    "  -r DIR     Load ROMs from DIR instead of current directory.\n"
    "  -e         Activate extra 16K RAM expansion.\n"
    "  -a         Disable piezo speaker audio.\n"
    "\n");
  fprintf(stdout,
    "Specify a BASIC program text file to load it automatically.\n"
    "This happens by injecting the characters through auto key loading.\n\n");
  fprintf(stdout, "Console modes:\n"
    "  %d   None/Disable.\n"
    "  %d   Curses with ASCII. (20x4)\n"
    "  %d   Curses with '#' pixels. (120x32)\n"
    "\n",
    CONSOLE_MODE_NONE,
    CONSOLE_MODE_CURSES_ASCII,
    CONSOLE_MODE_CURSES_PIXEL);
  fprintf(stdout, "Languages: US, FR, DE, GB, DK, SE, IT, ES\n\n");
  fprintf(stdout,
    "Using Ctrl+C will break into debugger, use 'q' from there to quit.\n\n");
}



int main(int argc, char *argv[])
{
  int c;
  char *charset_select = NULL;
  FILE *autoload_fh = NULL;
  char *rom_directory = NULL;
  bool ram_expansion = false;
  bool disable_audio = false;

  const char *autoload_cmd = "RUN";
  unsigned int autoload_cmd_index = 0;
  autoload_t autoload = AUTOLOAD_NONE;

  console_mode_t console_mode = CONSOLE_MODE_CURSES_PIXEL;
  console_charset_t console_charset = CONSOLE_CHARSET_US;

  while ((c = getopt(argc, argv, "hbwaem:c:r:")) != -1) {
    switch (c) {
    case 'h':
      display_help(argv[0]);
      return EXIT_SUCCESS;

    case 'b':
      debugger_break = true;
      break;

    case 'w':
      warp_mode = true;
      break;

    case 'm':
      console_mode = atoi(optarg);
      break;

    case 'e':
      ram_expansion = true;
      break;

    case 'a':
      disable_audio = true;
      break;

    case 'c':
      charset_select = optarg;
      break;

    case 'r':
      rom_directory = optarg;
      break;

    case '?':
    default:
      display_help(argv[0]);
      return EXIT_FAILURE;
    }
  }

  /* Autoload BASIC text program if specified: */
  if (argc > optind) {
    autoload_fh = fopen(argv[optind], "r");
    if (autoload_fh == NULL) {
      fprintf(stdout, "Failed to open '%s' for reading!\n", argv[optind]);
      return EXIT_FAILURE;
    }
    autoload = AUTOLOAD_FILE;
  }

  if (charset_select != NULL) {
    if (strncasecmp(charset_select, "FR", 2) == 0) {
      console_charset = CONSOLE_CHARSET_FR;
    } else if (strncasecmp(charset_select, "DE", 2) == 0) {
      console_charset = CONSOLE_CHARSET_DE;
    } else if (strncasecmp(charset_select, "GB", 2) == 0) {
      console_charset = CONSOLE_CHARSET_GB;
    } else if (strncasecmp(charset_select, "DK", 2) == 0) {
      console_charset = CONSOLE_CHARSET_DK;
    } else if (strncasecmp(charset_select, "SE", 2) == 0) {
      console_charset = CONSOLE_CHARSET_SE;
    } else if (strncasecmp(charset_select, "IT", 2) == 0) {
      console_charset = CONSOLE_CHARSET_IT;
    } else if (strncasecmp(charset_select, "ES", 2) == 0) {
      console_charset = CONSOLE_CHARSET_ES;
    } else { /* Default "US" */
      console_charset = CONSOLE_CHARSET_US;
    }
  }

  hd6301_trace_init();
  debugger_init();
  signal(SIGINT, sig_handler);

  if (! disable_audio) {
    if (piezo_init() != 0) {
      fprintf(stdout, "Piezo speaker initialization failed!\n");
      return EXIT_FAILURE;
    }
  }

  if (ram_expansion) {
    mem_init(&master_mem, &master_mcu, MEM_RAM_MAX_EXPANSION);
  } else {
    mem_init(&master_mem, &master_mcu, MEM_RAM_MAX_DEFAULT);
  }
  mem_init(&slave_mem, &slave_mcu, 0);

  /* Initialize specific memory: */
  master_mem.ram[MASTER_IO_KRTN_GATE_A] = 0xFF; /* No lines high. */
  master_mem.ram[MASTER_IO_KRTN_GATE_B] = 0xFF; /* No lines high. */
  master_mem.ram[HD6301_REG_PORT_1] = 0xF8; /* Clear interrupt lines. */

  /* Set max RAM here, so Ctrl+@ initialization is not needed: */
  master_mem.ram[0x012C] = (master_mem.ram_max + 1) / 0x100; /* RMBADR */
  master_mem.ram[0x012D] = (master_mem.ram_max + 1) % 0x100;
  master_mem.ram[0x0134] = (master_mem.ram_max + 1) / 0x100; /* BSWTAD */
  master_mem.ram[0x0135] = (master_mem.ram_max + 1) % 0x100;

  rom_load(rom_directory, &master_mem, BASIC1_ROM,  0x8000,
    8192, BASIC1_10_CRC32, BASIC1_11_CRC32);
  rom_load(rom_directory, &master_mem, BASIC2_ROM,  0xA000,
    8192, BASIC2_10_CRC32, BASIC2_11_CRC32);
  rom_load(rom_directory, &master_mem, UTILITY_ROM, 0xC000,
    8192, UTILITY_10_CRC32, UTILITY_11_CRC32);
  rom_load(rom_directory, &master_mem, MONITOR_ROM, 0xE000,
    8192, MONITOR_10_CRC32, MONITOR_11_CRC32);
  rom_load(rom_directory, &slave_mem,  SLAVE_ROM,   0xF000,
    4096, SLAVE_CRC32, 0);

  if (console_init(console_mode, console_charset) != 0) {
    fprintf(stdout, "Console initialization failed!\n");
    return EXIT_FAILURE;
  }

  hd6301_reset(&master_mcu, &master_mem, 0);
  hd6301_reset(&slave_mcu, &slave_mem, 1);

  /* Setup timer to relax CPU: */
  struct itimerval new;
  new.it_value.tv_sec = 0;
  new.it_value.tv_usec = 13353;
  new.it_interval.tv_sec = 0;
  new.it_interval.tv_usec = 13353;
  signal(SIGALRM, sig_handler);
  setitimer(ITIMER_REAL, &new, NULL);

  if (autoload == AUTOLOAD_FILE) {
    /* Always enable warp mode for faster loading: */
    warp_mode = true;
    /* Set up automatic key input: */
    master_mem.ram[0x165] = 0xA; /* KYISFL */
    master_mem.ram[0x166] = 2;   /* KYISCN */
    master_mem.ram[0x16F] = '2'; /* KYISTK[0] */
    /* Key '2' will enter BASIC on startup. */
  }

  while (1) {
    hd6301_execute(&master_mcu, &master_mem);
    hd6301_execute(&slave_mcu, &slave_mem);

    /* SCI transfer from master MCU to slave MCU: */
    if (master_mcu.transmit_shift_register >= 0) {
      debugger_sci_trace_add(true,
        master_mcu.transmit_shift_register, master_mcu.counter);
      hd6301_sci_receive(&slave_mcu, &slave_mem,
        master_mcu.transmit_shift_register);
      master_mcu.transmit_shift_register = -1;
    }

    /* SCI transfer from slave MCU to master MCU: */
    if (slave_mcu.transmit_shift_register >= 0) {
      debugger_sci_trace_add(false,
        slave_mcu.transmit_shift_register, master_mcu.counter);
      hd6301_sci_receive(&master_mcu, &master_mem,
        slave_mcu.transmit_shift_register);
      slave_mcu.transmit_shift_register = -1;
    }

    rs232_execute(&slave_mcu, &slave_mem);
    piezo_execute(&slave_mcu, &slave_mem);
    console_execute(&master_mcu, &master_mem);

    /* Handle automatic loading and key input: */
    if (master_mem.ram[0x167] == 2) {
      switch (autoload) {
      case AUTOLOAD_FILE:
        c = fgetc(autoload_fh);
        if (c == EOF) {
          fclose(autoload_fh);
          autoload_fh = NULL;
          c = autoload_cmd[0];
          autoload_cmd_index++;
          if (autoload_cmd_index >= strlen(autoload_cmd)) {
            autoload = AUTOLOAD_END;
          } else {
            autoload = AUTOLOAD_CMD;
          }
        }
        master_mem.ram[0x170] = c; /* KYISTK[1] */
        master_mem.ram[0x167] = 1; /* KYISPN */
        break;

      case AUTOLOAD_CMD:
        c = autoload_cmd[autoload_cmd_index];
        autoload_cmd_index++;
        if (autoload_cmd_index >= strlen(autoload_cmd)) {
          autoload = AUTOLOAD_END;
        }
        master_mem.ram[0x170] = c; /* KYISTK[1] */
        master_mem.ram[0x167] = 1; /* KYISPN */
        break;

      case AUTOLOAD_END:
        master_mem.ram[0x170] = '\r'; /* KYISTK[1] */
        warp_mode = false;
        autoload = AUTOLOAD_NONE;
        break;

      case AUTOLOAD_NONE:
      default:
        break;
      }
    }

    /* Debugger break?: */
    if (debugger_break) {
      console_pause();
      if (panic_msg[0] != '\0') {
        fprintf(stdout, "%s", panic_msg);
        panic_msg[0] = '\0';
      }
      debugger_break = debugger(&master_mcu, &slave_mcu,
        &master_mem, &slave_mem);
      if (! debugger_break) {
        console_resume();
      }
    }

    /* Sleep: */
    if (! warp_mode) {
      if (master_mcu.sync_counter > 8192) {
        master_mcu.sync_counter = 0;
        pause(); /* Wait for SIGALRM. */
      }
    }
  }

  return EXIT_SUCCESS;
}



