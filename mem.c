#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h> /* clock_gettime() / localtime_r() */

#include "mem.h"
#include "hd6301.h"
#include "console.h"



static uint8_t bcd(uint8_t value)
{
  return ((value / 10) * 0x10) + value % 10;
}



static uint8_t rtc_value(mem_t *mem, uint16_t address)
{
  struct timespec ts;
  struct tm tm;
  bool bcd_mode;
  bool hour_mode;

  bcd_mode = (mem->ram[MASTER_RTC_REGISTER_B] >> 2) & 1;
  hour_mode = (mem->ram[MASTER_RTC_REGISTER_B] >> 1) & 1;

  clock_gettime(CLOCK_REALTIME, &ts);
  localtime_r(&ts.tv_sec, &tm);

  switch (address) {
  case MASTER_RTC_SECONDS:
    return bcd_mode ? tm.tm_sec : bcd(tm.tm_sec);

  case MASTER_RTC_MINUTES:
    return bcd_mode ? tm.tm_min : bcd(tm.tm_min);

  case MASTER_RTC_HOUR:
    if (hour_mode) { /* 24 Hour Mode */
      return bcd_mode ? tm.tm_hour : bcd(tm.tm_hour);
    } else { /* 12 Hour Mode */
      if (tm.tm_hour > 12) {
        return bcd_mode ? (tm.tm_hour + 0x80) : (bcd(tm.tm_hour) + 0x80);
      } else {
        return bcd_mode ? tm.tm_hour : bcd(tm.tm_hour);
      }
    }

  case MASTER_RTC_DAY:
    return tm.tm_wday + 1;

  case MASTER_RTC_DATE:
    return bcd_mode ? tm.tm_mday : bcd(tm.tm_mday);

  case MASTER_RTC_MONTH:
    return bcd_mode ? (tm.tm_mon + 1) : bcd(tm.tm_mon + 1);

  case MASTER_RTC_YEAR:
    return bcd_mode ? (tm.tm_year % 100) : bcd(tm.tm_year % 100);

  case MASTER_RTC_SECONDS_ALARM:
  case MASTER_RTC_MINUTES_ALARM:
  case MASTER_RTC_HOUR_ALARM:
  default:
    return 0;
  }
}



void mem_init(mem_t *mem, void *cpu, uint16_t ram_max)
{
  int i;

  if (ram_max > 0) {
    mem->ram_max = ram_max;
  } else {
    mem->ram_max = MEM_RAM_MAX_DEFAULT;
  }

  for (i = 0x0000; i <= mem->ram_max; i++) {
    mem->ram[i] = 0x00; /* RAM area. */
  }
  for (i = (mem->ram_max + 1); i <= 0xFFFF; i++) {
    mem->ram[i] = 0xFF; /* ROM area. */
  }

  mem->cpu = cpu;
}



uint8_t mem_read(mem_t *mem, uint16_t address)
{
  if (address < 0x20) {
    hd6301_register_read_notify(mem->cpu, mem, address);
  }
  if (address >= MASTER_RTC_SECONDS && address <= MASTER_RTC_YEAR) {
    return rtc_value(mem, address);
  } else {
    return mem->ram[address];
  }
}



void mem_read_area(mem_t *mem, uint16_t address, uint8_t data[], size_t size)
{
  for (uint16_t i = 0; i < size; i++) {
    data[i] = mem->ram[address + i];
  }
}



void mem_write(mem_t *mem, uint16_t address, uint8_t value)
{
  if (address < 0x20) {
    hd6301_register_write(mem->cpu, mem, address, value);

  } else if (address == MASTER_IO_PORT_26) { /* Special port at 0x26... */
    mem->ram[MASTER_IO_PORT_26_FB] = value; /* ...is read back at 0x4F. */
    console_lcd_select(value);

  } else if (address == MASTER_IO_LCD_DATA) {
    console_lcd_data(value);

  } else if (address <= mem->ram_max) {
    mem->ram[address] = value;

  }
}



void mem_write_area(mem_t *mem, uint16_t address, uint8_t data[], size_t size)
{
  for (uint16_t i = 0; i < size; i++) {
    mem->ram[address + i] = data[i];
  }
}



int mem_load_from_file(mem_t *mem, const char *filename, uint16_t address)
{
  FILE *fh;
  int c;

  fh = fopen(filename, "rb");
  if (fh == NULL) {
    return -1;
  }

  while ((c = fgetc(fh)) != EOF) {
    mem->ram[address] = c;
    address++; /* Just overflow... */
  }

  fclose(fh);
  return 0;
}



static void mem_ram_dump_16(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  uint16_t address;

  fprintf(fh, "%04x   ", start & 0xFFF0);

  /* Hex */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    if ((address >= start) && (address <= end)) {
      fprintf(fh, "%02x ", mem->ram[address]);
    } else {
      fprintf(fh, "   ");
    }
    if (i % 4 == 3) {
      fprintf(fh, " ");
    }
  }

  /* Character */
  for (i = 0; i < 16; i++) {
    address = (start & 0xFFF0) + i;
    if ((address >= start) && (address <= end)) {
      if (isprint(mem->ram[address])) {
        fprintf(fh, "%c", mem->ram[address]);
      } else {
        fprintf(fh, ".");
      }
    } else {
      fprintf(fh, " ");
    }
  }

  fprintf(fh, "\n");
}



void mem_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end)
{
  int i;
  mem_ram_dump_16(fh, mem, start, end);
  for (i = (start & 0xFFF0) + 16; i <= end; i += 16) {
    mem_ram_dump_16(fh, mem, i, end);
  }
}



