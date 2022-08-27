#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MEM_RAM_MAX_DEFAULT   0x3FFF /* Default 16K. */
#define MEM_RAM_MAX_EXPANSION 0x7FFF /* 16K + 16K Expansion = 32K. */

#define MASTER_IO_KSC_GATE    0x0020 /* Keyboard Scan 0~7 */
#define MASTER_IO_KRTN_GATE_A 0x0022 /* Keyboard Input 0~7 */
#define MASTER_IO_PORT_26     0x0026 /* Special Port 26 */
#define MASTER_IO_KRTN_GATE_B 0x0028 /* Keyboard Input 8, 9, PWSW & BUSY */
#define MASTER_IO_LCD_DATA    0x002A /* Output Data to LCD Controller */
#define MASTER_IO_PORT_26_FB  0x004F /* Special Port 26 Feedback */

#define MASTER_RTC_SECONDS       0x0040
#define MASTER_RTC_SECONDS_ALARM 0x0041
#define MASTER_RTC_MINUTES       0x0042
#define MASTER_RTC_MINUTES_ALARM 0x0043
#define MASTER_RTC_HOUR          0x0044
#define MASTER_RTC_HOUR_ALARM    0x0045
#define MASTER_RTC_DAY           0x0046
#define MASTER_RTC_DATE          0x0047
#define MASTER_RTC_MONTH         0x0048
#define MASTER_RTC_YEAR          0x0049
#define MASTER_RTC_REGISTER_A    0x004A
#define MASTER_RTC_REGISTER_B    0x004B
#define MASTER_RTC_REGISTER_C    0x004C
#define MASTER_RTC_REGISTER_D    0x004D

typedef struct mem_s {
  uint8_t ram[UINT16_MAX + 1];
  void *cpu;
  uint16_t ram_max;
} mem_t;

void mem_init(mem_t *mem, void *cpu, uint16_t ram_max);
uint8_t mem_read(mem_t *mem, uint16_t address);
void mem_read_area(mem_t *mem, uint16_t address, uint8_t data[], size_t size);
void mem_write(mem_t *mem, uint16_t address, uint8_t value);
void mem_write_area(mem_t *mem, uint16_t address, uint8_t data[], size_t size);
int mem_load_from_file(mem_t *mem, const char *filename, uint16_t address);
void mem_dump(FILE *fh, mem_t *mem, uint16_t start, uint16_t end);

#endif /* _MEM_H */
