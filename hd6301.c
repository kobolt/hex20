#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "hd6301.h"
#include "mem.h"
#include "panic.h"



#define HD6301_TRACE_BUFFER_CPUS 2
#define HD6301_TRACE_BUFFER_SIZE 1024
#define HD6301_TRACE_BUFFER_ENTRY 80

static int hd6301_trace_buffer_index[HD6301_TRACE_BUFFER_CPUS];
static char hd6301_trace_buffer[HD6301_TRACE_BUFFER_CPUS]
                               [HD6301_TRACE_BUFFER_SIZE]
                               [HD6301_TRACE_BUFFER_ENTRY];



static void hd6301_trace(hd6301_t *cpu, const char *op_name, 
  const char *format, ...)
{
  va_list args;
  char buffer[HD6301_TRACE_BUFFER_ENTRY + 2];
  int n = 0;

  if (op_name != NULL) { /* Regular CPU instruction trace. */
    n += snprintf(&buffer[n], HD6301_TRACE_BUFFER_ENTRY - n,
      "PC=%04x A:B=%04x X=%04x SP=%04x CCR=%02x(11%c%c%c%c%c%c) [%d] ",
      cpu->pc - 1, cpu->d, cpu->x, cpu->sp, cpu->ccr,
      cpu->h ? 'H' : 'h',
      cpu->i ? 'I' : 'i',
      cpu->n ? 'N' : 'n',
      cpu->z ? 'Z' : 'z',
      cpu->v ? 'V' : 'v',
      cpu->c ? 'C' : 'c',
      cpu->counter);

    n += snprintf(&buffer[n], HD6301_TRACE_BUFFER_ENTRY - n, "%s ", op_name);

  } else { /* Special message trace. */
    n += snprintf(&buffer[n], HD6301_TRACE_BUFFER_ENTRY - n,
      "PC=%04x ", cpu->pc - 1);
  }

  if (format != NULL) {
    va_start(args, format);
    n += vsnprintf(&buffer[n], HD6301_TRACE_BUFFER_ENTRY - n, format, args);
    va_end(args);
  }

  n += snprintf(&buffer[n], HD6301_TRACE_BUFFER_ENTRY - n, "\n");

  strncpy(hd6301_trace_buffer[cpu->id][hd6301_trace_buffer_index[cpu->id]],
    buffer, HD6301_TRACE_BUFFER_ENTRY);
  hd6301_trace_buffer_index[cpu->id]++;
  if (hd6301_trace_buffer_index[cpu->id] >= HD6301_TRACE_BUFFER_SIZE) {
    hd6301_trace_buffer_index[cpu->id] = 0;
  }
}



void hd6301_trace_init(void)
{
  for (int i = 0; i < HD6301_TRACE_BUFFER_CPUS; i++) {
    for (int j = 0; j < HD6301_TRACE_BUFFER_SIZE; j++) {
      hd6301_trace_buffer[i][j][0] = '\0';
    }
    hd6301_trace_buffer_index[i] = 0;
  }
}



void hd6301_trace_dump(FILE *fh, int cpu_id)
{
  for (int i = hd6301_trace_buffer_index[cpu_id];
           i < HD6301_TRACE_BUFFER_SIZE; i++) {
    if (hd6301_trace_buffer[cpu_id][i][0] != '\0') {
      fprintf(fh, hd6301_trace_buffer[cpu_id][i]);
    }
  }
  for (int i = 0; i < hd6301_trace_buffer_index[cpu_id]; i++) {
    if (hd6301_trace_buffer[cpu_id][i][0] != '\0') {
      fprintf(fh, hd6301_trace_buffer[cpu_id][i]);
    }
  }
}



void hd6301_dump(FILE *fh, hd6301_t *cpu)
{
  fprintf(fh, "CPU #%d\n", cpu->id);
  fprintf(fh, "  Sleep         : %d\n", cpu->sleep);
  fprintf(fh, "  Counter       : %d\n", cpu->counter);
  fprintf(fh, "  Sync Counter  : %d\n", cpu->sync_counter);
  fprintf(fh, "  Shift Register: %d (0x%02x)\n",
    cpu->transmit_shift_register, cpu->transmit_shift_register);
  fprintf(fh, "  IRQ Pending   : %d\n", cpu->irq_pending);
  fprintf(fh, "    Vector Low  : 0x%04x\n", cpu->irq_pending_vector_low);
  fprintf(fh, "    Vector High : 0x%04x\n", cpu->irq_pending_vector_high);
}



static void op_aba(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t prev_a;
  uint8_t prev_b;
  hd6301_trace(cpu, "aba", NULL);
  prev_a = cpu->a;
  prev_b = cpu->b;
  cpu->a += cpu->b;
  cpu->h = (((prev_a & prev_b) | (prev_b & ~cpu->a) | (~cpu->a & prev_a)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev_a & prev_b & ~cpu->a) | (~prev_a & ~prev_b & cpu->a)) >> 7;
  cpu->c = ((prev_a & prev_b) | (prev_b & ~cpu->a) | (~cpu->a & prev_a)) >> 7;
}

static void op_abx(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "abx", NULL);
  cpu->x += cpu->b;
}

static void op_adca_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adca", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a += value;
  cpu->a += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_adca_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adca", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a += value;
  cpu->a += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_adca_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adca", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a += value;
  cpu->a += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_adca_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "adca", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->a;
  cpu->a += value;
  cpu->a += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_adcb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adcb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b += value;
  cpu->b += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_adcb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adcb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b += value;
  cpu->b += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_adcb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adcb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b += value;
  cpu->b += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_adcb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "adcb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->b;
  cpu->b += value;
  cpu->b += cpu->c;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_adda_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adda", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a += value;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_adda_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adda", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a += value;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_adda_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "adda", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a += value;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_adda_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "adda", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->a;
  cpu->a += value;
  cpu->h = (((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 3) & 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->a) | (~prev & ~value & cpu->a)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->a) | (~cpu->a & prev)) >> 7;
}

static void op_addb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "addb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b += value;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_addb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "addb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b += value;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_addb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "addb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b += value;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_addb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "addb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->b;
  cpu->b += value;
  cpu->h = (((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 3) & 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->b) | (~prev & ~value & cpu->b)) >> 7;
  cpu->c = ((prev & value) | (value & ~cpu->b) | (~cpu->b & prev)) >> 7;
}

static void op_addd_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  uint16_t address;
  hd6301_trace(cpu, "addd", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  prev = cpu->d;
  cpu->d += value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->d) | (~prev & ~value & cpu->d)) >> 15;
  cpu->c = ((prev & value) | (value & ~cpu->d) | (~cpu->d & prev)) >> 15;
}

static void op_addd_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  uint16_t address;
  hd6301_trace(cpu, "addd", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  prev = cpu->d;
  cpu->d += value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->d) | (~prev & ~value & cpu->d)) >> 15;
  cpu->c = ((prev & value) | (value & ~cpu->d) | (~cpu->d & prev)) >> 15;
}

static void op_addd_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  uint16_t address;
  hd6301_trace(cpu, "addd", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  prev = cpu->d;
  cpu->d += value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->d) | (~prev & ~value & cpu->d)) >> 15;
  cpu->c = ((prev & value) | (value & ~cpu->d) | (~cpu->d & prev)) >> 15;
}

static void op_addd_imm(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  hd6301_trace(cpu, "addb", "#%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++) * 0x100;
  value += mem_read(mem, cpu->pc++);
  prev = cpu->d;
  cpu->d += value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = ((prev & value & ~cpu->d) | (~prev & ~value & cpu->d)) >> 15;
  cpu->c = ((prev & value) | (value & ~cpu->d) | (~cpu->d & prev)) >> 15;
}

static void op_aim_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "aim", "#%02x, %02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++);
  value &= mem_read(mem, address);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_aim_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "aim", "#%02x, %02x,x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value &= mem_read(mem, address);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_anda_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "anda", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->a &= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_anda_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "anda", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->a &= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_anda_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "anda", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  cpu->a &= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_anda_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  hd6301_trace(cpu, "anda", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  cpu->a &= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_andb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "andb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->b &= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_andb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "andb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->b &= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_andb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "andb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  cpu->b &= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_andb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  hd6301_trace(cpu, "andb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  cpu->b &= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_asl_ext(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "asl", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  carry = (value & 0x80) >> 7;
  value <<= 1;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_asl_idx(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "asl", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  carry = (value & 0x80) >> 7;
  value <<= 1;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_asla(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "asla", NULL);
  carry = (cpu->a & 0x80) >> 7;
  cpu->a <<= 1;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_aslb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "aslb", NULL);
  carry = (cpu->b & 0x80) >> 7;
  cpu->b <<= 1;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_asld(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "asld", NULL);
  carry = (cpu->d & 0x8000) >> 15;
  cpu->d <<= 1;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_asr_ext(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  uint8_t value;
  uint8_t keep;
  uint16_t address;
  hd6301_trace(cpu, "asr", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  carry = value & 1;
  keep = value & 0x80;
  value >>= 1;
  value |= keep;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_asr_idx(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  uint8_t value;
  uint8_t keep;
  uint16_t address;
  hd6301_trace(cpu, "asr", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  carry = value & 1;
  keep = value & 0x80;
  value >>= 1;
  value |= keep;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_asra(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  uint8_t keep;
  hd6301_trace(cpu, "asra", NULL);
  carry = cpu->a & 1;
  keep = cpu->a & 0x80;
  cpu->a >>= 1;
  cpu->a |= keep;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_asrb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  uint8_t keep;
  hd6301_trace(cpu, "asrb", NULL);
  carry = cpu->b & 1;
  keep = cpu->b & 0x80;
  cpu->b >>= 1;
  cpu->b |= keep;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_bcc(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bcc", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->c == 0) {
    cpu->pc += relative;
  }
}

static void op_bcs(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bcs", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->c == 1) {
    cpu->pc += relative;
  }
}

static void op_beq(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "beq", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->z == 1) {
    cpu->pc += relative;
  }
}

static void op_bge(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bge", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if ((cpu->n ^ cpu->v) == 0) {
    cpu->pc += relative;
  }
}

static void op_bgt(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bgt", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if ((cpu->z + (cpu->n ^ cpu->v)) == 0) {
    cpu->pc += relative;
  }
}

static void op_bhi(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bhi", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if ((cpu->c + cpu->z) == 0) {
    cpu->pc += relative;
  }
}

static void op_bita_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "bita", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->a & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_bita_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "bita", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->a & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_bita_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "bita", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  result = cpu->a & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_bita_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  hd6301_trace(cpu, "bita", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  result = cpu->a & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_bitb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "bitb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->b & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_bitb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "bitb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->b & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_bitb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "bitb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  result = cpu->b & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_bitb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  hd6301_trace(cpu, "bitb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  result = cpu->b & value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ble(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "ble", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if ((cpu->z + (cpu->n ^ cpu->v)) == 1) {
    cpu->pc += relative;
  }
}

static void op_bls(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bls", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if ((cpu->c + cpu->z) == 1) {
    cpu->pc += relative;
  }
}

static void op_blt(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "blt", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if ((cpu->n ^ cpu->v) == 1) {
    cpu->pc += relative;
  }
}

static void op_bmi(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bmi", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->n == 1) {
    cpu->pc += relative;
  }
}

static void op_bne(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bne", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->z == 0) {
    cpu->pc += relative;
  }
}

static void op_bpl(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bpl", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->n == 0) {
    cpu->pc += relative;
  }
}

static void op_bra(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bra", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  cpu->pc += relative;
}

static void op_brn(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "brn", "%02x", mem_read(mem, cpu->pc));
}

static void op_bsr(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bsr", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  mem_write(mem, cpu->sp--, (cpu->pc) % 0x100);
  mem_write(mem, cpu->sp--, (cpu->pc) / 0x100);
  cpu->pc += relative;
}

static void op_bvc(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bvc", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->v == 0) {
    cpu->pc += relative;
  }
}

static void op_bvs(hd6301_t *cpu, mem_t *mem)
{
  int8_t relative;
  hd6301_trace(cpu, "bvs", "%02x", mem_read(mem, cpu->pc));
  relative = mem_read(mem, cpu->pc++);
  if (cpu->v == 1) {
    cpu->pc += relative;
  }
}

static void op_cba(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t result;
  hd6301_trace(cpu, "cba", NULL);
  result = cpu->a - cpu->b;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->a & ~cpu->b & ~result) | (~cpu->a & cpu->b & result)) >> 7;
  cpu->c = ((~cpu->a & cpu->b) | (cpu->b & result) | (result & ~cpu->a)) >> 7;
}

static void op_clc(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "clc", NULL);
  cpu->c = 0;
}

static void op_cli(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "cli", NULL);
  cpu->i = 0;
}

static void op_clr_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "clr", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  mem_write(mem, address, 0);
  cpu->n = 0;
  cpu->z = 1;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_clr_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "clr", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  mem_write(mem, address, 0);
  cpu->n = 0;
  cpu->z = 1;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_clra(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "clra", NULL);
  cpu->a = 0;
  cpu->n = 0;
  cpu->z = 1;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_clrb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "clrb", NULL);
  cpu->b = 0;
  cpu->n = 0;
  cpu->z = 1;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_clv(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "clv", NULL);
  cpu->v = 0;
}

static void op_cmpa_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "cmpa", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->a - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->a & ~value & ~result) | (~cpu->a & value & result)) >> 7;
  cpu->c = ((~cpu->a & value) | (value & result) | (result & ~cpu->a)) >> 7;
}

static void op_cmpa_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "cmpa", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->a - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->a & ~value & ~result) | (~cpu->a & value & result)) >> 7;
  cpu->c = ((~cpu->a & value) | (value & result) | (result & ~cpu->a)) >> 7;
}

static void op_cmpa_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "cmpa", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  result = cpu->a - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->a & ~value & ~result) | (~cpu->a & value & result)) >> 7;
  cpu->c = ((~cpu->a & value) | (value & result) | (result & ~cpu->a)) >> 7;
}

static void op_cmpa_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  hd6301_trace(cpu, "cmpa", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  result = cpu->a - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->a & ~value & ~result) | (~cpu->a & value & result)) >> 7;
  cpu->c = ((~cpu->a & value) | (value & result) | (result & ~cpu->a)) >> 7;
}

static void op_cmpb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "cmpb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->b - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->b & ~value & ~result) | (~cpu->b & value & result)) >> 7;
  cpu->c = ((~cpu->b & value) | (value & result) | (result & ~cpu->b)) >> 7;
}

static void op_cmpb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "cmpb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  result = cpu->b - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->b & ~value & ~result) | (~cpu->b & value & result)) >> 7;
  cpu->c = ((~cpu->b & value) | (value & result) | (result & ~cpu->b)) >> 7;
}

static void op_cmpb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  uint16_t address;
  hd6301_trace(cpu, "cmpb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  result = cpu->b - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->b & ~value & ~result) | (~cpu->b & value & result)) >> 7;
  cpu->c = ((~cpu->b & value) | (value & result) | (result & ~cpu->b)) >> 7;
}

static void op_cmpb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t result;
  hd6301_trace(cpu, "cmpb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  result = cpu->b - value;
  cpu->n = (result & 0x80) >> 7;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->b & ~value & ~result) | (~cpu->b & value & result)) >> 7;
  cpu->c = ((~cpu->b & value) | (value & result) | (result & ~cpu->b)) >> 7;
}

static void op_com_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "com", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  value = ~value;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 1;
}

static void op_com_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "com", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  value = ~value;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 1;
}

static void op_coma(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "coma", NULL);
  cpu->a = ~cpu->a;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 1;
}

static void op_comb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "comb", NULL);
  cpu->b = ~cpu->b;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 1;
}

static void op_cpx_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t result;
  uint16_t address;
  hd6301_trace(cpu, "cpx", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  result = cpu->x - value;
  cpu->n = (result & 0x8000) >> 15;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->x & ~value & ~result) | (~cpu->x & value & result)) >> 15;
  cpu->c = ((~cpu->x & value) | (value & result) | (result & ~cpu->x)) >> 15;
}

static void op_cpx_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t result;
  uint16_t address;
  hd6301_trace(cpu, "cpx", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  result = cpu->x - value;
  cpu->n = (result & 0x8000) >> 15;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->x & ~value & ~result) | (~cpu->x & value & result)) >> 15;
  cpu->c = ((~cpu->x & value) | (value & result) | (result & ~cpu->x)) >> 15;
}

static void op_cpx_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t result;
  uint16_t address;
  hd6301_trace(cpu, "cpx", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  result = cpu->x - value;
  cpu->n = (result & 0x8000) >> 15;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->x & ~value & ~result) | (~cpu->x & value & result)) >> 15;
  cpu->c = ((~cpu->x & value) | (value & result) | (result & ~cpu->x)) >> 15;
}

static void op_cpx_imm(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t result;
  hd6301_trace(cpu, "cpx", "#%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++) * 0x100;
  value += mem_read(mem, cpu->pc++);
  result = cpu->x - value;
  cpu->n = (result & 0x8000) >> 15;
  cpu->z = result == 0 ? 1 : 0;
  cpu->v = ((cpu->x & ~value & ~result) | (~cpu->x & value & result)) >> 15;
  cpu->c = ((~cpu->x & value) | (value & result) | (result & ~cpu->x)) >> 15;
}

static void op_daa(hd6301_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
  panic("DAA not implemented!\n");
}

static void op_dec_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "dec", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  value -= 1;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = value == 0x7f ? 1 : 0;
}

static void op_dec_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "dec", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  value -= 1;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = value == 0x7f ? 1 : 0;
}

static void op_deca(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "deca", NULL);
  cpu->a--;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->a == 0x7f ? 1 : 0;
}

static void op_decb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "decb", NULL);
  cpu->b--;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->b == 0x7f ? 1 : 0;
}

static void op_des(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "des", NULL);
  cpu->sp--;
}

static void op_dex(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "dex", NULL);
  cpu->x--;
  cpu->z = cpu->x == 0 ? 1 : 0;
}

static void op_eim_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eim", "#%02x, %02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++);
  value ^= mem_read(mem, address);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eim_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eim", "#%02x, %02x,x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value ^= mem_read(mem, address);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eora_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eora", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->a ^= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eora_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eora", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->a ^= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eora_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eora", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  cpu->a ^= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eora_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  hd6301_trace(cpu, "eora", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  cpu->a ^= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eorb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eorb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->b ^= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eorb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eorb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->b ^= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eorb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "eorb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  cpu->b ^= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_eorb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  hd6301_trace(cpu, "eorb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  cpu->b ^= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_inc_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "inc", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  value += 1;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = value == 0x80 ? 1 : 0;
}

static void op_inc_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "inc", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  value += 1;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = value == 0x80 ? 1 : 0;
}

static void op_inca(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "inca", NULL);
  cpu->a++;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->a == 0x80 ? 1 : 0;
}

static void op_incb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "incb", NULL);
  cpu->b++;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->b == 0x80 ? 1 : 0;
}

static void op_ins(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "ins", NULL);
  cpu->sp++;
}

static void op_inx(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "inx", NULL);
  cpu->x++;
  cpu->z = cpu->x == 0 ? 1 : 0;
}

static void op_jmp_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "jmp", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  cpu->pc = address;
}

static void op_jmp_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "jmp", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  cpu->pc = address;
}

static void op_jsr_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "jsr", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  mem_write(mem, cpu->sp--, (cpu->pc) % 0x100);
  mem_write(mem, cpu->sp--, (cpu->pc) / 0x100);
  cpu->pc = address;
}

static void op_jsr_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "jsr", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  mem_write(mem, cpu->sp--, (cpu->pc) % 0x100);
  mem_write(mem, cpu->sp--, (cpu->pc) / 0x100);
  cpu->pc = address;
}

static void op_jsr_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "jsr", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  mem_write(mem, cpu->sp--, (cpu->pc) % 0x100);
  mem_write(mem, cpu->sp--, (cpu->pc) / 0x100);
  cpu->pc = address;
}

static void op_ldaa_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldaa", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  cpu->a = mem_read(mem, address);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldaa_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldaa", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  cpu->a = mem_read(mem, address);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldaa_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldaa", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  cpu->a = mem_read(mem, address);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldaa_imm(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "ldaa", "#%02x", mem_read(mem, cpu->pc));
  cpu->a = mem_read(mem, cpu->pc++);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldab_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldab", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  cpu->b = mem_read(mem, address);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldab_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldab", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  cpu->b = mem_read(mem, address);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldab_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldab", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  cpu->b = mem_read(mem, address);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldab_imm(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "ldab", "#%02x", mem_read(mem, cpu->pc));
  cpu->b = mem_read(mem, cpu->pc++);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldd_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldd", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  cpu->d = mem_read(mem, address) * 0x100;
  cpu->d += mem_read(mem, address + 1);
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldd_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldd", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  cpu->d = mem_read(mem, address) * 0x100;
  cpu->d += mem_read(mem, address + 1);
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldd_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldd", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  cpu->d = mem_read(mem, address) * 0x100;
  cpu->d += mem_read(mem, address + 1);
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldd_imm(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "ldd", "#%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  cpu->d = mem_read(mem, cpu->pc++) * 0x100;
  cpu->d += mem_read(mem, cpu->pc++);
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_lds_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "lds", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  cpu->sp = mem_read(mem, address) * 0x100;
  cpu->sp += mem_read(mem, address + 1);
  cpu->n = (cpu->sp & 0x8000) >> 15;
  cpu->z = cpu->sp == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_lds_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "lds", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  cpu->sp = mem_read(mem, address) * 0x100;
  cpu->sp += mem_read(mem, address + 1);
  cpu->n = (cpu->sp & 0x8000) >> 15;
  cpu->z = cpu->sp == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_lds_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "lds", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  cpu->sp = mem_read(mem, address) * 0x100;
  cpu->sp += mem_read(mem, address + 1);
  cpu->n = (cpu->sp & 0x8000) >> 15;
  cpu->z = cpu->sp == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_lds_imm(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "lds", "#%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  cpu->sp = mem_read(mem, cpu->pc++) * 0x100;
  cpu->sp += mem_read(mem, cpu->pc++);
  cpu->n = (cpu->sp & 0x8000) >> 15;
  cpu->z = cpu->sp == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldx_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldx", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  cpu->x = mem_read(mem, address) * 0x100;
  cpu->x += mem_read(mem, address + 1);
  cpu->n = (cpu->x & 0x8000) >> 15;
  cpu->z = cpu->x == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldx_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldx", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  cpu->x = mem_read(mem, address) * 0x100;
  cpu->x += mem_read(mem, address + 1);
  cpu->n = (cpu->x & 0x8000) >> 15;
  cpu->z = cpu->x == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldx_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "ldx", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  cpu->x = mem_read(mem, address) * 0x100;
  cpu->x += mem_read(mem, address + 1);
  cpu->n = (cpu->x & 0x8000) >> 15;
  cpu->z = cpu->x == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_ldx_imm(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "ldx", "#%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  cpu->x = mem_read(mem, cpu->pc++) * 0x100;
  cpu->x += mem_read(mem, cpu->pc++);
  cpu->n = (cpu->x & 0x8000) >> 15;
  cpu->z = cpu->x == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_lsr_ext(hd6301_t *cpu, mem_t *mem)
{
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "lsr", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  carry = value & 1;
  value >>= 1;
  mem_write(mem, address, value);
  cpu->n = 0;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_lsr_idx(hd6301_t *cpu, mem_t *mem)
{
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "lsr", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  carry = value & 1;
  value >>= 1;
  mem_write(mem, address, value);
  cpu->n = 0;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_lsra(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "lsra", NULL);
  carry = cpu->a & 1;
  cpu->a >>= 1;
  cpu->n = 0;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_lsrb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "lsrb", NULL);
  carry = cpu->b & 1;
  cpu->b >>= 1;
  cpu->n = 0;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_lsrd(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "lsrd", NULL);
  carry = cpu->d & 1;
  cpu->d >>= 1;
  cpu->n = 0;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_mul(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  uint16_t result;
  hd6301_trace(cpu, "mul", NULL);
  result = cpu->a * cpu->b;
  cpu->a = result / 0x100;
  cpu->b = result % 0x100;
  cpu->c = (cpu->b & 0x80) >> 7;
}

static void op_neg_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "neg", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  value = (value == 0x80 ? 0x80 : -value);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = value == 0x80 ? 1 : 0;
  cpu->c = value == 0 ? 0 : 1;
}

static void op_neg_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "neg", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  value = (value == 0x80 ? 0x80 : -value);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = value == 0x80 ? 1 : 0;
  cpu->c = value == 0 ? 0 : 1;
}

static void op_nega(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "nega", NULL);
  cpu->a = (cpu->a == 0x80 ? 0x80 : -cpu->a);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->a == 0x80 ? 1 : 0;
  cpu->c = cpu->a == 0 ? 0 : 1;
}

static void op_negb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "negb", NULL);
  cpu->b = (cpu->b == 0x80 ? 0x80 : -cpu->b);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->b == 0x80 ? 1 : 0;
  cpu->c = cpu->b == 0 ? 0 : 1;
}

static void op_nop(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "nop", NULL);
}

static void op_oim_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "oim", "#%02x, %02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++);
  value |= mem_read(mem, address);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_oim_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "oim", "#%02x, %02x,x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value |= mem_read(mem, address);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_oraa_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "oraa", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->a |= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_oraa_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "oraa", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->a |= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_oraa_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "oraa", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  cpu->a |= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_oraa_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  hd6301_trace(cpu, "oraa", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  cpu->a |= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_orab_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "orab", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->b |= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_orab_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "orab", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->b |= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_orab_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "orab", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  cpu->b |= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_orab_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  hd6301_trace(cpu, "orab", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  cpu->b |= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_psha(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "psha", NULL);
  mem_write(mem, cpu->sp--, cpu->a);
}

static void op_pshb(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "pshb", NULL);
  mem_write(mem, cpu->sp--, cpu->b);
}

static void op_pshx(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "pshx", NULL);
  mem_write(mem, cpu->sp--, (cpu->x) % 0x100);
  mem_write(mem, cpu->sp--, (cpu->x) / 0x100);
}

static void op_pula(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "pula", NULL);
  cpu->a = mem_read(mem, ++cpu->sp);
}

static void op_pulb(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "pulb", NULL);
  cpu->b = mem_read(mem, ++cpu->sp);
}

static void op_pulx(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "pulx", NULL);
  cpu->x = mem_read(mem, ++cpu->sp) * 0x100;
  cpu->x += mem_read(mem, ++cpu->sp);
}

static void op_rol_ext(hd6301_t *cpu, mem_t *mem)
{
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "rol", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  carry = (value & 0x80) >> 7;
  value <<= 1;
  value |= cpu->c;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_rol_idx(hd6301_t *cpu, mem_t *mem)
{
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "rol", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  carry = (value & 0x80) >> 7;
  value <<= 1;
  value |= cpu->c;
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_rola(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "rola", NULL);
  carry = (cpu->a & 0x80) >> 7;
  cpu->a <<= 1;
  cpu->a |= cpu->c;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_rolb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "rolb", NULL);
  carry = (cpu->b & 0x80) >> 7;
  cpu->b <<= 1;
  cpu->b |= cpu->c;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_ror_ext(hd6301_t *cpu, mem_t *mem)
{
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "ror", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  carry = value & 1;
  value >>= 1;
  value |= (cpu->c << 7);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_ror_idx(hd6301_t *cpu, mem_t *mem)
{
  bool carry;
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "ror", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  carry = value & 1;
  value >>= 1;
  value |= (cpu->c << 7);
  mem_write(mem, address, value);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_rora(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "rora", NULL);
  carry = cpu->a & 1;
  cpu->a >>= 1;
  cpu->a |= (cpu->c << 7);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_rorb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  bool carry;
  hd6301_trace(cpu, "rorb", NULL);
  carry = cpu->b & 1;
  cpu->b >>= 1;
  cpu->b |= (cpu->c << 7);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = cpu->n ^ carry;
  cpu->c = carry;
}

static void op_rti(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "rti", NULL);
  cpu->ccr = mem_read(mem, ++cpu->sp);
  cpu->b = mem_read(mem, ++cpu->sp);
  cpu->a = mem_read(mem, ++cpu->sp);
  cpu->x = mem_read(mem, ++cpu->sp) * 0x100;
  cpu->x += mem_read(mem, ++cpu->sp);
  cpu->pc = mem_read(mem, ++cpu->sp) * 0x100;
  cpu->pc += mem_read(mem, ++cpu->sp);
}

static void op_rts(hd6301_t *cpu, mem_t *mem)
{
  hd6301_trace(cpu, "rts", NULL);
  cpu->pc = mem_read(mem, ++cpu->sp) * 0x100;
  cpu->pc += mem_read(mem, ++cpu->sp);
}

static void op_sec(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "sec", NULL);
  cpu->c = 1;
}

static void op_sei(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "sei", NULL);
  cpu->i = 1;
}

static void op_sev(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "sev", NULL);
  cpu->v = 1;
}

static void op_slp(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "slp", NULL);
  cpu->sleep = true;
}

static void op_staa_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "staa", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->a);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_staa_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "staa", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->a);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_staa_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "staa", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  mem_write(mem, address, cpu->a);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_stab_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "stab", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->b);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_stab_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "stab", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->b);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_stab_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "stab", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  mem_write(mem, address, cpu->b);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_sba(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  uint8_t prev;
  hd6301_trace(cpu, "sba", NULL);
  prev = cpu->a;
  cpu->a = cpu->a - cpu->b;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~cpu->b & ~cpu->a) | (~prev & cpu->b & cpu->a)) >> 7;
  cpu->c = ((~prev & cpu->b) | (cpu->b & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_sbca_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "sbca", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a -= value;
  cpu->a -= cpu->c;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_sbca_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "sbca", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a -= value;
  cpu->a -= cpu->c;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_sbca_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "sbca", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a -= value;
  cpu->a -= cpu->c;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_sbca_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "sbca", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->a;
  cpu->a -= value;
  cpu->a -= cpu->c;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_sbcb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "sbcb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b -= value;
  cpu->b -= cpu->c;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_sbcb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "sbcb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b -= value;
  cpu->b -= cpu->c;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_sbcb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "sbcb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b -= value;
  cpu->b -= cpu->c;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_sbcb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "sbcb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->b;
  cpu->b -= value;
  cpu->b -= cpu->c;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_std_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "std", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->a);
  mem_write(mem, address + 1, cpu->b);
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_std_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "std", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->a);
  mem_write(mem, address + 1, cpu->b);
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_std_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "std", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  mem_write(mem, address, cpu->a);
  mem_write(mem, address + 1, cpu->b);
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_sts_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "sts", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->sp / 0x100);
  mem_write(mem, address + 1, cpu->sp % 0x100);
  cpu->n = (cpu->sp & 0x8000) >> 15;
  cpu->z = cpu->sp == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_sts_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "sts", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->sp / 0x100);
  mem_write(mem, address + 1, cpu->sp % 0x100);
  cpu->n = (cpu->sp & 0x8000) >> 15;
  cpu->z = cpu->sp == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_sts_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "sts", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  mem_write(mem, address, cpu->sp / 0x100);
  mem_write(mem, address + 1, cpu->sp % 0x100);
  cpu->n = (cpu->sp & 0x8000) >> 15;
  cpu->z = cpu->sp == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_stx_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "stx", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->x / 0x100);
  mem_write(mem, address + 1, cpu->x % 0x100);
  cpu->n = (cpu->x & 0x8000) >> 15;
  cpu->z = cpu->x == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_stx_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "stx", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  mem_write(mem, address, cpu->x / 0x100);
  mem_write(mem, address + 1, cpu->x % 0x100);
  cpu->n = (cpu->x & 0x8000) >> 15;
  cpu->z = cpu->x == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_stx_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t address;
  hd6301_trace(cpu, "stx", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  mem_write(mem, address, cpu->x / 0x100);
  mem_write(mem, address + 1, cpu->x % 0x100);
  cpu->n = (cpu->x & 0x8000) >> 15;
  cpu->z = cpu->x == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_suba_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "suba", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a -= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_suba_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "suba", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a -= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_suba_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "suba", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->a;
  cpu->a -= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_suba_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "suba", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->a;
  cpu->a -= value;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->a) | (~prev & value & cpu->a)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->a) | (cpu->a & ~prev)) >> 7;
}

static void op_subb_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "subb", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b -= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_subb_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "subb", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b -= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_subb_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  uint16_t address;
  hd6301_trace(cpu, "subb", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  prev = cpu->b;
  cpu->b -= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_subb_imm(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint8_t prev;
  hd6301_trace(cpu, "subb", "#%02x", mem_read(mem, cpu->pc));
  value = mem_read(mem, cpu->pc++);
  prev = cpu->b;
  cpu->b -= value;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = ((prev & ~value & ~cpu->b) | (~prev & value & cpu->b)) >> 7;
  cpu->c = ((~prev & value) | (value & cpu->b) | (cpu->b & ~prev)) >> 7;
}

static void op_subd_dir(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  uint16_t address;
  hd6301_trace(cpu, "subd", "%02x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++);
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  prev = cpu->d;
  cpu->d -= value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = (prev & ~value & ~cpu->d & ~prev & value & cpu->d) >> 15;
  cpu->c = ((~prev & value) | (value & cpu->d) | (cpu->d & ~prev)) >> 15;
}

static void op_subd_ext(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  uint16_t address;
  hd6301_trace(cpu, "subd", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  prev = cpu->d;
  cpu->d -= value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = (prev & ~value & ~cpu->d & ~prev & value & cpu->d) >> 15;
  cpu->c = ((~prev & value) | (value & cpu->d) | (cpu->d & ~prev)) >> 15;
}

static void op_subd_idx(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  uint16_t address;
  hd6301_trace(cpu, "subd", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address) * 0x100;
  value += mem_read(mem, address + 1);
  prev = cpu->d;
  cpu->d -= value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = (prev & ~value & ~cpu->d & ~prev & value & cpu->d) >> 15;
  cpu->c = ((~prev & value) | (value & cpu->d) | (cpu->d & ~prev)) >> 15;
}

static void op_subd_imm(hd6301_t *cpu, mem_t *mem)
{
  uint16_t value;
  uint16_t prev;
  hd6301_trace(cpu, "subd", "#%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++) * 0x100;
  value += mem_read(mem, cpu->pc++);
  prev = cpu->d;
  cpu->d -= value;
  cpu->n = (cpu->d & 0x8000) >> 15;
  cpu->z = cpu->d == 0 ? 1 : 0;
  cpu->v = (prev & ~value & ~cpu->d & ~prev & value & cpu->d) >> 15;
  cpu->c = ((~prev & value) | (value & cpu->d) | (cpu->d & ~prev)) >> 15;
}

static void op_swi(hd6301_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
  panic("SWI not implemented!\n");
}

static void op_tab(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "tab", NULL);
  cpu->b = cpu->a;
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_tap(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "tap", NULL);
  cpu->ccr = cpu->a;
}

static void op_tba(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "tba", NULL);
  cpu->a = cpu->b;
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_tim_dir(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "tim", "#%02x, %02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++);
  value &= mem_read(mem, address);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_tim_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "tim", "#%02x, %02x,x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  value = mem_read(mem, cpu->pc++);
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value &= mem_read(mem, address);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
}

static void op_tpa(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "tpa", NULL);
  cpu->a = cpu->ccr;
}

static void op_tst_ext(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "tst", "%02x%02x", mem_read(mem, cpu->pc), mem_read(mem, cpu->pc + 1));
  address = mem_read(mem, cpu->pc++) * 0x100;
  address += mem_read(mem, cpu->pc++);
  value = mem_read(mem, address);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_tst_idx(hd6301_t *cpu, mem_t *mem)
{
  uint8_t value;
  uint16_t address;
  hd6301_trace(cpu, "tst", "%02x,x", mem_read(mem, cpu->pc));
  address = mem_read(mem, cpu->pc++) + cpu->x;
  value = mem_read(mem, address);
  cpu->n = (value & 0x80) >> 7;
  cpu->z = value == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_tsta(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "tsta", NULL);
  cpu->n = (cpu->a & 0x80) >> 7;
  cpu->z = cpu->a == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_tstb(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "tstb", NULL);
  cpu->n = (cpu->b & 0x80) >> 7;
  cpu->z = cpu->b == 0 ? 1 : 0;
  cpu->v = 0;
  cpu->c = 0;
}

static void op_tsx(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "tsx", NULL);
  cpu->x = cpu->sp + 1;
}

static void op_txs(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  hd6301_trace(cpu, "txs", NULL);
  cpu->sp = cpu->x - 1;
}

static void op_wai(hd6301_t *cpu, mem_t *mem)
{
  (void)cpu;
  (void)mem;
  panic("WAI not implemented!\n");
}

static void op_xgdx(hd6301_t *cpu, mem_t *mem)
{
  (void)mem;
  uint16_t temp;
  hd6301_trace(cpu, "xgdx", NULL);
  temp = cpu->d;
  cpu->d = cpu->x;
  cpu->x = temp;
}



static void op_trap(hd6301_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  opcode = mem_read(mem, cpu->pc - 1);
  hd6301_trace(cpu, "trap", "%02x", opcode);
  hd6301_irq(cpu, mem, HD6301_VECTOR_TRAP_LOW, HD6301_VECTOR_TRAP_HIGH);
}



static void op_none(hd6301_t *cpu, mem_t *mem)
{
  uint8_t opcode;
  opcode = mem_read(mem, cpu->pc - 1);
  panic("Panic! Unhandled opcode: 0x%02x @ %04x\n", opcode, cpu->pc - 1);
}



typedef void (*hd6301_operation_func_t)(hd6301_t *, mem_t *);

static hd6301_operation_func_t opcode_function[UINT8_MAX + 1] = {
  op_trap,     op_nop,      op_none,     op_none,     /* 0x00 -> 0x03 */
  op_lsrd,     op_asld,     op_tap,      op_tpa,      /* 0x04 -> 0x07 */
  op_inx,      op_dex,      op_clv,      op_sev,      /* 0x08 -> 0x0B */
  op_clc,      op_sec,      op_cli,      op_sei,      /* 0x0C -> 0x0F */
  op_sba,      op_cba,      op_none,     op_none,     /* 0x10 -> 0x13 */
  op_none,     op_none,     op_tab,      op_tba,      /* 0x14 -> 0x17 */
  op_xgdx,     op_daa,      op_slp,      op_aba,      /* 0x18 -> 0x1B */
  op_none,     op_none,     op_none,     op_none,     /* 0x1C -> 0x1F */
  op_bra,      op_brn,      op_bhi,      op_bls,      /* 0x20 -> 0x23 */
  op_bcc,      op_bcs,      op_bne,      op_beq,      /* 0x24 -> 0x27 */
  op_bvc,      op_bvs,      op_bpl,      op_bmi,      /* 0x28 -> 0x2B */
  op_bge,      op_blt,      op_bgt,      op_ble,      /* 0x2C -> 0x2F */
  op_tsx,      op_ins,      op_pula,     op_pulb,     /* 0x30 -> 0x33 */
  op_des,      op_txs,      op_psha,     op_pshb,     /* 0x34 -> 0x37 */
  op_pulx,     op_rts,      op_abx,      op_rti,      /* 0x38 -> 0x3B */
  op_pshx,     op_mul,      op_wai,      op_swi,      /* 0x3C -> 0x3F */
  op_nega,     op_none,     op_none,     op_coma,     /* 0x40 -> 0x43 */
  op_lsra,     op_none,     op_rora,     op_asra,     /* 0x44 -> 0x47 */
  op_asla,     op_rola,     op_deca,     op_none,     /* 0x48 -> 0x4B */
  op_inca,     op_tsta,     op_none,     op_clra,     /* 0x4C -> 0x4F */
  op_negb,     op_none,     op_none,     op_comb,     /* 0x50 -> 0x53 */
  op_lsrb,     op_none,     op_rorb,     op_asrb,     /* 0x54 -> 0x57 */
  op_aslb,     op_rolb,     op_decb,     op_none,     /* 0x58 -> 0x5B */
  op_incb,     op_tstb,     op_none,     op_clrb,     /* 0x5C -> 0x5F */
  op_neg_idx,  op_aim_idx,  op_oim_idx,  op_com_idx,  /* 0x60 -> 0x63 */
  op_lsr_idx,  op_eim_idx,  op_ror_idx,  op_asr_idx,  /* 0x64 -> 0x67 */
  op_asl_idx,  op_rol_idx,  op_dec_idx,  op_tim_idx,  /* 0x68 -> 0x6B */
  op_inc_idx,  op_tst_idx,  op_jmp_idx,  op_clr_idx,  /* 0x6C -> 0x6F */
  op_neg_ext,  op_aim_dir,  op_oim_dir,  op_com_ext,  /* 0x70 -> 0x73 */
  op_lsr_ext,  op_eim_dir,  op_ror_ext,  op_asr_ext,  /* 0x74 -> 0x77 */
  op_asl_ext,  op_rol_ext,  op_dec_ext,  op_tim_dir,  /* 0x78 -> 0x7B */
  op_inc_ext,  op_tst_ext,  op_jmp_ext,  op_clr_ext,  /* 0x7C -> 0x7F */
  op_suba_imm, op_cmpa_imm, op_sbca_imm, op_subd_imm, /* 0x80 -> 0x83 */
  op_anda_imm, op_bita_imm, op_ldaa_imm, op_none,     /* 0x84 -> 0x87 */
  op_eora_imm, op_adca_imm, op_oraa_imm, op_adda_imm, /* 0x88 -> 0x8B */
  op_cpx_imm,  op_bsr,      op_lds_imm,  op_none,     /* 0x8C -> 0x8F */
  op_suba_dir, op_cmpa_dir, op_sbca_dir, op_subd_dir, /* 0x90 -> 0x93 */
  op_anda_dir, op_bita_dir, op_ldaa_dir, op_staa_dir, /* 0x94 -> 0x97 */
  op_eora_dir, op_adca_dir, op_oraa_dir, op_adda_dir, /* 0x98 -> 0x9B */
  op_cpx_dir,  op_jsr_dir,  op_lds_dir,  op_sts_dir,  /* 0x9C -> 0x9F */
  op_suba_idx, op_cmpa_idx, op_sbca_idx, op_subd_idx, /* 0xA0 -> 0xA3 */
  op_anda_idx, op_bita_idx, op_ldaa_idx, op_staa_idx, /* 0xA4 -> 0xA7 */
  op_eora_idx, op_adca_idx, op_oraa_idx, op_adda_idx, /* 0xA8 -> 0xAB */
  op_cpx_idx,  op_jsr_idx,  op_lds_idx,  op_sts_idx,  /* 0xAC -> 0xAF */
  op_suba_ext, op_cmpa_ext, op_sbca_ext, op_subd_ext, /* 0xB0 -> 0xB3 */
  op_anda_ext, op_bita_ext, op_ldaa_ext, op_staa_ext, /* 0xB4 -> 0xB7 */
  op_eora_ext, op_adca_ext, op_oraa_ext, op_adda_ext, /* 0xB8 -> 0xBB */
  op_cpx_ext,  op_jsr_ext,  op_lds_ext,  op_sts_ext,  /* 0xBC -> 0xBF */
  op_subb_imm, op_cmpb_imm, op_sbcb_imm, op_addd_imm, /* 0xC0 -> 0xC3 */
  op_andb_imm, op_bitb_imm, op_ldab_imm, op_none,     /* 0xC4 -> 0xC7 */
  op_eorb_imm, op_adcb_imm, op_orab_imm, op_addb_imm, /* 0xC8 -> 0xCB */
  op_ldd_imm,  op_none,     op_ldx_imm,  op_none,     /* 0xCC -> 0xCF */
  op_subb_dir, op_cmpb_dir, op_sbcb_dir, op_addd_dir, /* 0xD0 -> 0xD3 */
  op_andb_dir, op_bitb_dir, op_ldab_dir, op_stab_dir, /* 0xD4 -> 0xD7 */
  op_eorb_dir, op_adcb_dir, op_orab_dir, op_addb_dir, /* 0xD8 -> 0xDB */
  op_ldd_dir,  op_std_dir,  op_ldx_dir,  op_stx_dir,  /* 0xDC -> 0xDF */
  op_subb_idx, op_cmpb_idx, op_sbcb_idx, op_addd_idx, /* 0xE0 -> 0xE3 */
  op_andb_idx, op_bitb_idx, op_ldab_idx, op_stab_idx, /* 0xE4 -> 0xE7 */
  op_eorb_idx, op_adcb_idx, op_orab_idx, op_addb_idx, /* 0xE8 -> 0xEB */
  op_ldd_idx,  op_std_idx,  op_ldx_idx,  op_stx_idx,  /* 0xEC -> 0xEF */
  op_subb_ext, op_cmpb_ext, op_sbcb_ext, op_addd_ext, /* 0xF0 -> 0xF3 */
  op_andb_ext, op_bitb_ext, op_ldab_ext, op_stab_ext, /* 0xF4 -> 0xF7 */
  op_eorb_ext, op_adcb_ext, op_orab_ext, op_addb_ext, /* 0xF8 -> 0xFB */
  op_ldd_ext,  op_std_ext,  op_ldx_ext,  op_stx_ext,  /* 0xFC -> 0xFF */
};



static int opcode_cycles[UINT8_MAX + 1] = {
/*
  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
*/
  0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x00 -> 0x0F */
  1, 1, 0, 0, 0, 0, 1, 1, 2, 2, 4, 1, 0, 0, 0, 0, /* 0x10 -> 0x1F */
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0x20 -> 0x2F */
  1, 1, 3, 3, 1, 1, 4, 4, 4, 5, 1,10, 5, 7, 9,12, /* 0x30 -> 0x3F */
  1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, /* 0x40 -> 0x4F */
  1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, /* 0x50 -> 0x5F */
  6, 7, 7, 6, 6, 7, 6, 6, 6, 6, 6, 5, 6, 4, 3, 5, /* 0x60 -> 0x6F */
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 4, 3, 5, /* 0x70 -> 0x7F */
  2, 2, 2, 3, 2, 2, 2, 0, 2, 2, 2, 2, 3, 5, 3, 0, /* 0x80 -> 0x8F */
  3, 3, 3, 4, 3, 3, 3, 3, 3, 3, 3, 3, 4, 5, 4, 4, /* 0x90 -> 0x9F */
  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, /* 0xA0 -> 0xAF */
  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 6, 5, 5, /* 0xB0 -> 0xBF */
  2, 2, 2, 3, 2, 2, 2, 0, 2, 2, 2, 2, 3, 0, 3, 0, /* 0xC0 -> 0xCF */
  3, 3, 3, 4, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, /* 0xD0 -> 0xDF */
  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, /* 0xE0 -> 0xEF */
  4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, /* 0xF0 -> 0xFF */
};



static void hd6301_counter_increment(hd6301_t *cpu, mem_t *mem, int cycles)
{
  uint16_t prev_counter;
  uint16_t ocr;

  cpu->sync_counter += cycles;

  prev_counter = cpu->counter;
  cpu->counter += cycles;
  mem->ram[HD6301_REG_FRC_HIGH] = cpu->counter / 0x100;
  mem->ram[HD6301_REG_FRC_LOW]  = cpu->counter % 0x100;

  /* Output compare: */
  ocr = mem->ram[HD6301_REG_OCR_LOW] + (mem->ram[HD6301_REG_OCR_HIGH] * 0x100);
  while (cycles > 0) {
    if (prev_counter == ocr) {
      mem->ram[HD6301_REG_TCSR] |= (1 << HD6301_TCSR_OCF);
      /* Generate IRQ if enabled: */
      if ((mem->ram[HD6301_REG_TCSR] >> HD6301_TCSR_EOCI) & 1) {
        hd6301_irq(cpu, mem, HD6301_VECTOR_OCF_LOW, HD6301_VECTOR_OCF_HIGH);
      }

      /* Set P21 based on OLVL as used by RS232C: */
      if (mem->ram[HD6301_REG_TCSR] & 1) {
        mem->ram[HD6301_REG_PORT_2] |= 0x02;
      } else {
        mem->ram[HD6301_REG_PORT_2] &= ~0x02;
      }
      cpu->p21_set = true;
    }
    prev_counter++;
    cycles--;
  }
}



void hd6301_execute(hd6301_t *cpu, mem_t *mem)
{
  uint8_t opcode;

  /* Pester CPU with SCI IRQ if there are still unread RDR contents: */
  if ((mem->ram[HD6301_REG_TRCSR] >> HD6301_TRCSR_RDRF) & 1) {
    if ((mem->ram[HD6301_REG_TRCSR] >> HD6301_TRCSR_RIE) & 1) {
      hd6301_irq(cpu, mem, HD6301_VECTOR_SCI_LOW, HD6301_VECTOR_SCI_HIGH);
    }
  }

  if (cpu->sleep) {
    hd6301_counter_increment(cpu, mem, 1);
    return;
  }

  /* Check for pending IRQ: */
  if (cpu->irq_pending && (cpu->i == 0)) {
    hd6301_irq(cpu, mem,
      cpu->irq_pending_vector_low,
      cpu->irq_pending_vector_high);
    cpu->irq_pending = false;
    cpu->irq_pending_vector_low  = 0x0;
    cpu->irq_pending_vector_high = 0x0;
  }

  opcode = mem_read(mem, cpu->pc++);
  (opcode_function[opcode])(cpu, mem);

  hd6301_counter_increment(cpu, mem, opcode_cycles[opcode]);

  /* RDRF clear: */
  if (cpu->rdr_flag) {
    mem->ram[HD6301_REG_TRCSR] &= ~(1 << HD6301_TRCSR_RDRF);
    cpu->rdr_flag = false;
  }

  /* Check for P20 input change and possibly ICR transfer: */
  if ((mem->ram[HD6301_REG_PORT_2] & 1) != cpu->p20_prev) {
    if ((mem->ram[HD6301_REG_TCSR] >> HD6301_TCSR_IEDG) & 1) {
      /* Low to High transition. */
      if (cpu->p20_prev == false) {
        mem->ram[HD6301_REG_ICR_HIGH] = cpu->counter / 0x100;
        mem->ram[HD6301_REG_ICR_LOW]  = cpu->counter % 0x100;
        mem->ram[HD6301_REG_TCSR] |= (1 << HD6301_TCSR_ICF);
      }
    } else {
      /* High to Low transition. */
      if (cpu->p20_prev == true) {
        mem->ram[HD6301_REG_ICR_HIGH] = cpu->counter / 0x100;
        mem->ram[HD6301_REG_ICR_LOW]  = cpu->counter % 0x100;
        mem->ram[HD6301_REG_TCSR] |= (1 << HD6301_TCSR_ICF);
      }
    }
    cpu->p20_prev = mem->ram[HD6301_REG_PORT_2] & 1;
  }
}



void hd6301_reset(hd6301_t *cpu, mem_t *mem, int id)
{
  cpu->id = id;

  cpu->pc  = mem_read(mem, HD6301_VECTOR_RESET_LOW);
  cpu->pc += mem_read(mem, HD6301_VECTOR_RESET_HIGH) * 0x100;
  cpu->d   = 0;
  cpu->x   = 0;
  cpu->sp  = 0;
  cpu->ccr = 0xD0;

  cpu->counter = 0;
  cpu->sync_counter = 0;

  cpu->tcsr_ocf_flag   = false;
  cpu->tcsr_icf_flag   = false;
  cpu->trcsr_orfe_flag = false;
  cpu->trcsr_rdrf_flag = false;
  cpu->rdr_flag        = false;

  cpu->p20_prev = false;

  cpu->transmit_shift_register = -1;
  cpu->sleep = false;

  cpu->irq_pending = false;
  cpu->irq_pending_vector_low  = 0x0;
  cpu->irq_pending_vector_high = 0x0;

  mem->ram[HD6301_REG_OCR_HIGH] = 0xFF;
  mem->ram[HD6301_REG_OCR_LOW]  = 0xFF;
  mem->ram[HD6301_REG_TCSR]     = 0x00;
  mem->ram[HD6301_REG_TRCSR]    = 0x20;
}



void hd6301_register_write(hd6301_t *cpu, mem_t *mem, 
  uint16_t address, uint8_t value)
{
  switch (address) {
  case HD6301_REG_TCSR:
    mem->ram[address] = (mem->ram[address] & 0b11100000) + (value & 0b11111);
    break;

  case HD6301_REG_TRCSR:
    mem->ram[address] = (mem->ram[address] & 0b11100000) + (value & 0b11111);
    break;

  case HD6301_REG_OCR_LOW:
  case HD6301_REG_OCR_HIGH:
    if (cpu->tcsr_ocf_flag) {
      mem->ram[HD6301_REG_TCSR] &= ~(1 << HD6301_TCSR_OCF);
      cpu->tcsr_ocf_flag = false;
    }
    mem->ram[address] = value;
    break;

  case HD6301_REG_TDR:
    /* HD6301_TRCSR_TDRE not manipulated since this happens instantly. */
    mem->ram[address] = value;
    cpu->transmit_shift_register = value;
    break;

  case HD6301_REG_PORT_1:
    /* Filter value based on data direction, don't write to inputs. */
    mem->ram[HD6301_REG_PORT_1] &= ~mem->ram[HD6301_REG_DDR_1];
    mem->ram[HD6301_REG_PORT_1] |= value;
    break;

  case HD6301_REG_PORT_2:
    mem->ram[HD6301_REG_PORT_2] &= ~mem->ram[HD6301_REG_DDR_2];
    mem->ram[HD6301_REG_PORT_2] |= value;
    break;

  case HD6301_REG_PORT_3:
    mem->ram[HD6301_REG_PORT_3] &= ~mem->ram[HD6301_REG_DDR_3];
    mem->ram[HD6301_REG_PORT_3] |= value;
    break;

  case HD6301_REG_PORT_4:
    mem->ram[HD6301_REG_PORT_4] &= ~mem->ram[HD6301_REG_DDR_4];
    mem->ram[HD6301_REG_PORT_4] |= value;
    break;

  default:
    mem->ram[address] = value;
    break;
  }
}



void hd6301_register_read_notify(hd6301_t *cpu, mem_t *mem, uint16_t address)
{
  switch (address) {
  case HD6301_REG_TCSR:
    cpu->tcsr_ocf_flag = true;
    cpu->tcsr_icf_flag = true;
    break;

  case HD6301_REG_ICR_HIGH:
    if (cpu->tcsr_icf_flag) {
      mem->ram[HD6301_REG_TCSR] &= ~(1 << HD6301_TCSR_ICF);
      cpu->tcsr_icf_flag = false;
    }
    break;

  case HD6301_REG_TRCSR:
    cpu->trcsr_orfe_flag = true;
    cpu->trcsr_rdrf_flag = true;
    break;

  case HD6301_REG_RDR:
    if (cpu->trcsr_rdrf_flag) {
      cpu->rdr_flag = true;
      cpu->trcsr_rdrf_flag = false;
    }
    break;
  }
}



void hd6301_irq(hd6301_t *cpu, mem_t *mem,
  uint16_t vector_low, uint16_t vector_high)
{
  cpu->sleep = false; /* Always taken out of sleep. */

  if (cpu->i) {
    /* Certain IRQs can be set to pending... */
    if (vector_high == HD6301_VECTOR_IRQ_HIGH ||
        vector_high == HD6301_VECTOR_OCF_HIGH) {
      cpu->irq_pending_vector_low  = vector_low;
      cpu->irq_pending_vector_high = vector_high;
      cpu->irq_pending = true;
      hd6301_trace(cpu, NULL, "IRQ pending %04x:%04x", vector_low, vector_high);
      return;
    } else {
      /* ...while others are neglected. */
      hd6301_trace(cpu, NULL, "IRQ ignored %04x:%04x", vector_low, vector_high);
      return;
    }
  }

  hd6301_trace(cpu, NULL, "IRQ execute %04x:%04x", vector_low, vector_high);

  mem_write(mem, cpu->sp--, cpu->pc % 0x100);
  mem_write(mem, cpu->sp--, cpu->pc / 0x100);
  mem_write(mem, cpu->sp--, cpu->x % 0x100);
  mem_write(mem, cpu->sp--, cpu->x / 0x100);
  mem_write(mem, cpu->sp--, cpu->a);
  mem_write(mem, cpu->sp--, cpu->b);
  mem_write(mem, cpu->sp--, cpu->ccr);
  cpu->i = 1;
  cpu->pc  = mem_read(mem, vector_low);
  cpu->pc += mem_read(mem, vector_high) * 0x100;
  hd6301_counter_increment(cpu, mem, 12);
}



void hd6301_sci_receive(hd6301_t *cpu, mem_t *mem, uint8_t value)
{
  mem->ram[HD6301_REG_RDR] = value;
  mem->ram[HD6301_REG_TRCSR] |= (1 << HD6301_TRCSR_RDRF);
  if ((mem->ram[HD6301_REG_TRCSR] >> HD6301_TRCSR_RIE) & 1) {
    hd6301_irq(cpu, mem, HD6301_VECTOR_SCI_LOW, HD6301_VECTOR_SCI_HIGH);
  }
}



