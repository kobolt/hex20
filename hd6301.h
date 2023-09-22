#ifndef _HD6301_H
#define _HD6301_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> /* FILE */
#include "mem.h"

typedef struct hd6301_s {
  union {
    struct {
      uint8_t b; /* Accumulator B */
      uint8_t a; /* Accumulator A */
    };
    uint16_t d;  /* Combined Accumulator D */
  };
  uint16_t x;    /* Index Register X */
  uint16_t sp;   /* Stack Pointer */
  uint16_t pc;   /* Program Counter */

  union {
    struct {
      uint8_t c : 1; /* Carry      */
      uint8_t v : 1; /* Overflow   */
      uint8_t z : 1; /* Zero       */
      uint8_t n : 1; /* Negative   */
      uint8_t i : 1; /* Interrupt  */
      uint8_t h : 1; /* Half Carry */
      uint8_t u : 2; /* Unused     */
    };
    uint8_t ccr; /* Condition Code Register */
  };

  uint16_t counter; /* Free Running Counter */
  uint16_t sync_counter; /* Extra counter for synchronization. */
  int id; /* Identification (used in trace) */

  /* Flags used for read notification then clearing: */
  bool tcsr_ocf_flag;
  bool tcsr_icf_flag;
  bool trcsr_orfe_flag;
  bool trcsr_rdrf_flag;
  bool rdr_flag;

  bool p20_prev; /* Previous state of P20 input pin. */
  bool p21_set; /* To easily track that P21 output pin has changed. */

  int transmit_shift_register;
  bool sleep;

  bool irq_pending;
  uint16_t irq_pending_vector_low;
  uint16_t irq_pending_vector_high;
} hd6301_t;

#define HD6301_VECTOR_TRAP_HIGH  0xFFEE
#define HD6301_VECTOR_TRAP_LOW   0xFFEF
#define HD6301_VECTOR_SCI_HIGH   0xFFF0
#define HD6301_VECTOR_SCI_LOW    0xFFF1
#define HD6301_VECTOR_TOF_HIGH   0xFFF2
#define HD6301_VECTOR_TOF_LOW    0xFFF3
#define HD6301_VECTOR_OCF_HIGH   0xFFF4
#define HD6301_VECTOR_OCF_LOW    0xFFF5
#define HD6301_VECTOR_ICF_HIGH   0xFFF6
#define HD6301_VECTOR_ICF_LOW    0xFFF7
#define HD6301_VECTOR_IRQ_HIGH   0xFFF8
#define HD6301_VECTOR_IRQ_LOW    0xFFF9
#define HD6301_VECTOR_SWI_HIGH   0xFFFA
#define HD6301_VECTOR_SWI_LOW    0xFFFB
#define HD6301_VECTOR_NMI_HIGH   0xFFFC
#define HD6301_VECTOR_NMI_LOW    0xFFFD
#define HD6301_VECTOR_RESET_HIGH 0xFFFE
#define HD6301_VECTOR_RESET_LOW  0xFFFF

#define HD6301_REG_DDR_1    0x0000 /* Port 1 Data Direction */
#define HD6301_REG_DDR_2    0x0001 /* Port 2 Data Direction */
#define HD6301_REG_PORT_1   0x0002 /* Port 1 Data */
#define HD6301_REG_PORT_2   0x0003 /* Port 2 Data */
#define HD6301_REG_DDR_3    0x0004 /* Port 3 Data Direction */
#define HD6301_REG_DDR_4    0x0005 /* Port 4 Data Direction */
#define HD6301_REG_PORT_3   0x0006 /* Port 3 Data */
#define HD6301_REG_PORT_4   0x0007 /* Port 4 Data */
#define HD6301_REG_TCSR     0x0008 /* Timer Control/Status */
#define HD6301_REG_FRC_HIGH 0x0009 /* Counter (High Byte) */
#define HD6301_REG_FRC_LOW  0x000A /* Counter (Low Byte) */
#define HD6301_REG_OCR_HIGH 0x000B /* Output Compare (High Byte) */
#define HD6301_REG_OCR_LOW  0x000C /* Output Compare (Low Byte) */
#define HD6301_REG_ICR_HIGH 0x000D /* Input Capture (High Byte) */
#define HD6301_REG_ICR_LOW  0x000E /* Input Capture (Low Byte) */
#define HD6301_REG_P3CSR    0x000F /* Port 3 Control and Status */
#define HD6301_REG_RMCR     0x0010 /* Rate and Mode Control */
#define HD6301_REG_TRCSR    0x0011 /* Transmit/Receive Control and Status */
#define HD6301_REG_RDR      0x0012 /* Receive Data */
#define HD6301_REG_TDR      0x0013 /* Transmit Data */
#define HD6301_REG_RAM_CTRL 0x0014 /* RAM Control */

#define HD6301_TCSR_OLVL 0 /* Output Level */
#define HD6301_TCSR_IEDG 1 /* Input Edge */
#define HD6301_TCSR_ETOI 2 /* Enable Timer Overflow Interrupt */
#define HD6301_TCSR_EOCI 3 /* Enable Output Compare Interrupt */
#define HD6301_TCSR_EICI 4 /* Enable Input Capture Interrupt */
#define HD6301_TCSR_TOF  5 /* Timer Overflow Flag */
#define HD6301_TCSR_OCF  6 /* Output Compare Flag */
#define HD6301_TCSR_ICF  7 /* Input Capture Flag */

#define HD6301_P3CSR_LATCH 3 /* Latch Enable */
#define HD6301_P3CSR_OSS   4 /* Output Strobe Select */
#define HD6301_P3CSR_IS3I  6 /* IS3 IRQ Enable */
#define HD6301_P3CSR_IS3   7 /* IS3 Flag */

#define HD6301_RMCR_SS0 0 /* Speed Select (Low Bit) */
#define HD6301_RMCR_SS1 1 /* Speed Select (High Bit) */
#define HD6301_RMCR_CC0 2 /* Clock Control / Format Select (Low Bit) */
#define HD6301_RMCR_CC1 3 /* Clock Control / Format Select (High Bit) */

#define HD6301_TRCSR_WU   0 /* Wake Up */
#define HD6301_TRCSR_TE   1 /* Transmit Enable */
#define HD6301_TRCSR_TIE  2 /* Transmit Interrupt Enable */
#define HD6301_TRCSR_RE   3 /* Receive Enable */
#define HD6301_TRCSR_RIE  4 /* Receive Interrupt Enable */
#define HD6301_TRCSR_TDRE 5 /* Transmit Data Register Empty */
#define HD6301_TRCSR_ORFE 6 /* Over Run Framing Error */
#define HD6301_TRCSR_RDRF 7 /* Receive Data Register Full */

#define HD6301_RAM_CTRL_RAME 6 /* RAM Enable */
#define HD6301_RAM_CTRL_STBY 7 /* Standby Bit */

void hd6301_trace_init(void);
void hd6301_trace_dump(FILE *fh, int cpu_id);

void hd6301_dump(FILE *fh, hd6301_t *cpu);

void hd6301_reset(hd6301_t *cpu, mem_t *mem, int id);
void hd6301_execute(hd6301_t *cpu, mem_t *mem);
void hd6301_register_write(hd6301_t *cpu, mem_t *mem,
  uint16_t address, uint8_t value);
void hd6301_register_read_notify(hd6301_t *cpu, mem_t *mem, uint16_t address);
void hd6301_sci_receive(hd6301_t *cpu, mem_t *mem, uint8_t value);
void hd6301_irq(hd6301_t *cpu, mem_t *mem,
  uint16_t vector_low, uint16_t vector_high);

#endif /* _HD6301_H */
