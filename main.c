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
#ifdef WIN32
#include <windows.h>
#endif

#include "hd6301.h"
#include "mem.h"
#include "console.h"
#include "rs232.h"
#include "piezo.h"
#include "cassette.h"
#include "serial.h"
#include "printer.h"
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

#define SREC_LINE_MAX 128



typedef enum {
  AUTOLOAD_NONE,
  AUTOLOAD_BASIC_FILE,
  AUTOLOAD_BASIC_RUN,
  AUTOLOAD_SREC_NEXT,
  AUTOLOAD_SREC_LINE,
  AUTOLOAD_END,
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
    fprintf(stdout, "Loading of system ROM '%s' at 0x%04x failed!\n",
      path, address);
    exit(EXIT_FAILURE);
  }

  crc = crc32(&mem->ram[address], size);
  if (! (crc == crc1 || crc == crc2)) {
    fprintf(stdout, "System ROM '%s' has invalid CRC32: %08x\n",
      filename, crc);
    exit(EXIT_FAILURE);
  }
}



static bool autoload_srec_line_from_file(FILE *fh, char *out_line)
{
  int i;
  int n;
  char in_line[SREC_LINE_MAX];
  uint8_t byte_count;

  while (fgets(in_line, SREC_LINE_MAX, fh) != NULL) {
    if (in_line[0] != 'S') {
      continue; /* Line must start with 'S'. */
    }

    if (in_line[1] != '1') {
      continue; /* Only 16-bit load address supported. */
    }

    if (sscanf(&in_line[2], "%02hhX", &byte_count) != 1) {
      continue; /* Unable to read byte count. */
    }

    /* Prepare MONITOR set command: */
    out_line[0] = 'S';
    out_line[1] = in_line[4];
    out_line[2] = in_line[5];
    out_line[3] = in_line[6];
    out_line[4] = in_line[7];
    out_line[5] = '\r';

    /* Copy data bytes: */
    for (i = 8, n = 6; i < (byte_count * 2) + 2; i += 2, n += 3) {
      if (n+2 > SREC_LINE_MAX) {
        continue; /* Overflow! */
      }
      out_line[n]   = in_line[i];
      out_line[n+1] = in_line[i+1];
      out_line[n+2] = '\r';
    }

    /* Termination: */
    if (n+2 > SREC_LINE_MAX) {
      continue; /* Overflow! */
    }
    out_line[n]   = '.';
    out_line[n+1] = '\r';
    out_line[n+2] = '\0';

    return true;
  }

  return false; /* EOF */
}



static void display_help(const char *progname)
{
  fprintf(stdout, "Usage: %s <options> [file]\n", progname);
  fprintf(stdout, "Options:\n"
    "  -h         Display this help.\n"
    "  -b         Break into debugger on start.\n"
    "  -w         Warp (full speed) mode.\n"
    "  -m MODE    Set MODE for console.\n"
    "  -c LANG    Use LANG character set.\n"
    "  -r DIR     Load system ROMs from DIR instead of current directory.\n"
    "  -e         Activate extra 16K RAM expansion.\n"
    "  -o ROM     Load option ROM into address 0x6000.\n"
    "  -s         Load file as S-record into MONITOR.\n"
    "  -p FILE    Enable micro-printer output to FILE.\n"
#ifndef SERIAL_DISABLE
    "  -t TTY     Use TTY for external 38400 baud high speed serial.\n"
#endif /* SERIAL_DISABLE */
#ifdef PIEZO_AUDIO_ENABLE
    "  -a         Disable piezo speaker audio.\n"
#endif /* PIEZO_AUDIO_ENABLE */
    "\n");
  fprintf(stdout,
    "Specify a BASIC program text file to load it automatically.\n"
    "This happens by injecting the characters through auto key loading.\n"
    "Use -s to load the file as an S-record into the MONITOR instead.\n\n");
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
  char *option_rom = NULL;
  char *printer_filename = NULL;
#ifndef SERIAL_DISABLE
  char *tty_device = NULL;
#endif /* SERIAL_DISABLE */
  bool ram_expansion = false;
  bool autoload_srec = false;
#ifdef PIEZO_AUDIO_ENABLE
  bool disable_audio = false;
#endif /* PIEZO_AUDIO_ENABLE */

  const char *autoload_basic_run = "RUN";
  unsigned int autoload_basic_run_index = 0;
  char autoload_srec_line[SREC_LINE_MAX];
  unsigned int autoload_srec_line_index = 0;
  autoload_t autoload = AUTOLOAD_NONE;

  console_mode_t console_mode = CONSOLE_MODE_CURSES_PIXEL;
  console_charset_t console_charset = CONSOLE_CHARSET_US;

  while ((c = getopt(argc, argv, "hbwaesm:c:r:o:p:t:")) != -1) {
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

    case 's':
      autoload_srec = true;
      break;

    case 'a':
#ifdef PIEZO_AUDIO_ENABLE
      disable_audio = true;
#endif /* PIEZO_AUDIO_ENABLE */
      break;

    case 'c':
      charset_select = optarg;
      break;

    case 'r':
      rom_directory = optarg;
      break;

    case 'o':
      option_rom = optarg;
      break;

    case 'p':
      printer_filename = optarg;
      break;

    case 't':
#ifndef SERIAL_DISABLE
      tty_device = optarg;
#endif /* SERIAL_DISABLE */
      break;

    case '?':
    default:
      display_help(argv[0]);
      return EXIT_FAILURE;
    }
  }

  /* Autoload program if specified: */
  if (argc > optind) {
    autoload_fh = fopen(argv[optind], "rb");
    if (autoload_fh == NULL) {
      fprintf(stdout, "Failed to open '%s' for reading!\n", argv[optind]);
      return EXIT_FAILURE;
    }
    if (autoload_srec) {
      autoload = AUTOLOAD_SREC_NEXT;
    } else {
      autoload = AUTOLOAD_BASIC_FILE;
    }
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

#ifdef PIEZO_AUDIO_ENABLE
  if (! disable_audio) {
    if (piezo_init() != 0) {
      fprintf(stdout, "Piezo speaker initialization failed!\n");
      return EXIT_FAILURE;
    }
  }
#endif

  if (ram_expansion) {
    if (option_rom) {
      fprintf(stdout, "Option ROM and RAM expansion overlaps!\n");
      return EXIT_FAILURE;
    }
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

  if (option_rom) {
    if (mem_load_from_file(&master_mem, option_rom, 0x6000) != 0) {
      fprintf(stdout, "Loading of option ROM '%s' at 0x6000 failed!\n",
        option_rom);
      return EXIT_FAILURE;
    }
  }

  if (printer_filename) {
    if (printer_init(printer_filename) != 0) {
      fprintf(stdout, "Printer initialization with output to '%s' failed!\n",
        printer_filename);
      return EXIT_FAILURE;
    }
  }

#ifndef SERIAL_DISABLE
  if (tty_device) {
    if (serial_init(tty_device) != 0) {
      fprintf(stdout, "Serial initialization failed!\n");
      return EXIT_FAILURE;
    }
  }
#endif /* SERIAL_DISABLE */

  if (console_init(console_mode, console_charset,
    printer_filename ? true : false) != 0) {
    fprintf(stdout, "Console initialization failed!\n");
    return EXIT_FAILURE;
  }

  hd6301_reset(&master_mcu, &master_mem, 0);
  hd6301_reset(&slave_mcu, &slave_mem, 1);

  /* Setup timer to relax CPU: */
#ifdef WIN32
  HANDLE timer = NULL;
  LARGE_INTEGER due_time;

  timer = CreateWaitableTimer(NULL, FALSE, NULL);
  if (timer == NULL) {
    fprintf(stdout, "CreateWaitableTimer() failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }

  due_time.QuadPart = -100000;
  if (SetWaitableTimer(timer, &due_time, 10, NULL, NULL, FALSE) == FALSE) {
    fprintf(stdout, "SetWaitableTimer() failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }
#else
  struct itimerval new;
  new.it_value.tv_sec = 0;
  new.it_value.tv_usec = 13353;
  new.it_interval.tv_sec = 0;
  new.it_interval.tv_usec = 13353;
  signal(SIGALRM, sig_handler);
  setitimer(ITIMER_REAL, &new, NULL);
#endif /* WIN32 */

  if (autoload == AUTOLOAD_BASIC_FILE || autoload == AUTOLOAD_SREC_NEXT) {
    /* Always enable warp mode for faster loading: */
    warp_mode = true;

    /* Set up automatic key input: */
    master_mem.ram[0x165] = 0xA; /* KYISFL */
    master_mem.ram[0x166] = 2;   /* KYISCN */

    if (autoload == AUTOLOAD_BASIC_FILE) {
      /* Key '2' will enter BASIC on startup. */
      master_mem.ram[0x16F] = '2'; /* KYISTK[0] */
    } else if (autoload == AUTOLOAD_SREC_NEXT) {
      /* Key '1' will enter MONITOR on startup. */
      master_mem.ram[0x16F] = '1'; /* KYISTK[0] */
    }
  }

  while (1) {
    hd6301_execute(&master_mcu, &master_mem);
    hd6301_execute(&slave_mcu, &slave_mem);

    if (master_mem.ram[HD6301_REG_PORT_2] & 0x4) {
      /* SCI transfer from master MCU to slave MCU: */
      if (master_mcu.transmit_shift_register >= 0) {
        debugger_sci_trace_add(SCI_TRACE_DIR_MASTER_TO_SLAVE,
          master_mcu.transmit_shift_register, master_mcu.counter);
        hd6301_sci_receive(&slave_mcu, &slave_mem,
          master_mcu.transmit_shift_register);
        master_mcu.transmit_shift_register = -1;
      }

      /* SCI transfer from slave MCU to master MCU: */
      if (slave_mcu.transmit_shift_register >= 0) {
        debugger_sci_trace_add(SCI_TRACE_DIR_SLAVE_TO_MASTER,
          slave_mcu.transmit_shift_register, master_mcu.counter);
        hd6301_sci_receive(&master_mcu, &master_mem,
          slave_mcu.transmit_shift_register);
        slave_mcu.transmit_shift_register = -1;
      }

    } else {
      /* SCI from master MCU has been redirected to external: */
#ifndef SERIAL_DISABLE
      serial_execute(&master_mcu, &master_mem);
#endif /* SERIAL_DISABLE */
    }

    rs232_execute(&master_mcu, &master_mem, &slave_mcu, &slave_mem);
#ifdef PIEZO_AUDIO_ENABLE
    piezo_execute(&slave_mcu, &slave_mem);
#endif /* PIEZO_AUDIO_ENABLE */
    console_execute(&master_mcu, &master_mem);
    cassette_execute(&slave_mcu, &slave_mem);
    printer_execute(&slave_mcu, &slave_mem);

    /* Connect slave MCU P34 to master MCU P12: */
    if (slave_mem.ram[HD6301_REG_PORT_3] & 0x10) {
      master_mem.ram[HD6301_REG_PORT_1] |= 0x04;
    } else {
      master_mem.ram[HD6301_REG_PORT_1] &= ~0x04;
    }

    /* Handle automatic loading and key input: */
    if (master_mem.ram[0x167] == 2) {
      switch (autoload) {
      case AUTOLOAD_BASIC_FILE:
        c = fgetc(autoload_fh);
        if (c == EOF) {
          fclose(autoload_fh);
          autoload_fh = NULL;
          c = autoload_basic_run[0];
          autoload_basic_run_index++;
          if (autoload_basic_run_index >= strlen(autoload_basic_run)) {
            autoload = AUTOLOAD_END;
          } else {
            autoload = AUTOLOAD_BASIC_RUN;
          }
        }
        master_mem.ram[0x170] = c; /* KYISTK[1] */
        master_mem.ram[0x167] = 1; /* KYISPN */
        break;

      case AUTOLOAD_BASIC_RUN:
        c = autoload_basic_run[autoload_basic_run_index];
        autoload_basic_run_index++;
        if (autoload_basic_run_index >= strlen(autoload_basic_run)) {
          autoload = AUTOLOAD_END;
        }
        master_mem.ram[0x170] = c; /* KYISTK[1] */
        master_mem.ram[0x167] = 1; /* KYISPN */
        break;

      case AUTOLOAD_SREC_NEXT:
        if (autoload_srec_line_from_file(autoload_fh, autoload_srec_line)) {
          autoload_srec_line_index = 0;
          autoload = AUTOLOAD_SREC_LINE;
        } else {
          autoload = AUTOLOAD_END;
        }
        break;

      case AUTOLOAD_SREC_LINE:
        c = autoload_srec_line[autoload_srec_line_index];
        autoload_srec_line_index++;
        if (autoload_srec_line_index >= strlen(autoload_srec_line)) {
          autoload = AUTOLOAD_SREC_NEXT;
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
#ifdef WIN32
        if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
          fprintf(stdout, "WaitForSingleObject() failed: %lu\n",
            GetLastError());
          return EXIT_FAILURE;
        }
#else
        pause(); /* Wait for SIGALRM. */
#endif /* WIN32 */
      }
    }
  }
  return EXIT_SUCCESS;
}



