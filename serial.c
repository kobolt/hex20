#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#include "hd6301.h"
#include "mem.h"
#include "debugger.h"
#include "panic.h"

#define SERIAL_RX_FIFO_SIZE 16384
#define SERIAL_TX_FIFO_SIZE 1024



static int serial_tty_fd = -1;

static uint8_t serial_rx_fifo[SERIAL_RX_FIFO_SIZE];
static uint8_t serial_tx_fifo[SERIAL_TX_FIFO_SIZE];
static int serial_rx_fifo_head = 0;
static int serial_tx_fifo_head = 0;
static int serial_rx_fifo_tail = 0;
static int serial_tx_fifo_tail = 0;



static bool serial_rx_fifo_read(uint8_t *byte)
{
  if (serial_rx_fifo_tail == serial_rx_fifo_head) {
    return false; /* Empty */
  }

  *byte = serial_rx_fifo[serial_rx_fifo_tail];
  serial_rx_fifo_tail = (serial_rx_fifo_tail + 1) % SERIAL_RX_FIFO_SIZE;

  return true;
}



static void serial_rx_fifo_write(uint8_t byte)
{
  if (((serial_rx_fifo_head + 1) % SERIAL_RX_FIFO_SIZE)
    == serial_rx_fifo_tail) {
    return; /* Full */
  }

  serial_rx_fifo[serial_rx_fifo_head] = byte;
  serial_rx_fifo_head = (serial_rx_fifo_head + 1) % SERIAL_RX_FIFO_SIZE;
}



static bool serial_tx_fifo_read(uint8_t *byte)
{
  if (serial_tx_fifo_tail == serial_tx_fifo_head) {
    return false; /* Empty */
  }

  *byte = serial_tx_fifo[serial_tx_fifo_tail];
  serial_tx_fifo_tail = (serial_tx_fifo_tail + 1) % SERIAL_TX_FIFO_SIZE;

  return true;
}



static void serial_tx_fifo_write(uint8_t byte)
{
  if (((serial_tx_fifo_head + 1) % SERIAL_TX_FIFO_SIZE)
    == serial_tx_fifo_tail) {
    return; /* Full */
  }

  serial_tx_fifo[serial_tx_fifo_head] = byte;
  serial_tx_fifo_head = (serial_tx_fifo_head + 1) % SERIAL_TX_FIFO_SIZE;
}



static void serial_exit(void)
{
  if (serial_tty_fd != -1) {
    close(serial_tty_fd);
  }
}



int serial_init(const char *tty_device)
{
  struct termios tios;

  serial_tty_fd = open(tty_device, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (serial_tty_fd == -1) {
    fprintf(stderr, "open() failed with errno: %d\n", errno);
    return -1;
  }

  if (tcgetattr(serial_tty_fd, &tios) == -1) {
    fprintf(stderr, "tcgetattr() failed with errno: %d\n", errno);
    close(serial_tty_fd);
    serial_tty_fd = -1;
    return -1;
  }

  cfmakeraw(&tios);
  cfsetispeed(&tios, B38400);
  cfsetospeed(&tios, B38400);

  if (tcsetattr(serial_tty_fd, TCSANOW, &tios) == -1) {
    fprintf(stderr, "tcsetattr() failed with errno: %d\n", errno);
    close(serial_tty_fd);
    serial_tty_fd = -1;
    return -1;
  }

  atexit(serial_exit);
  return 0;
}



void serial_execute(hd6301_t *master_mcu, mem_t *master_mem)
{
  uint8_t byte;

  if (serial_tty_fd == -1) {
    return;
  }

  if (master_mcu->transmit_shift_register >= 0) {
    /* SCI transfer from master MCU to external interface: */
    debugger_sci_trace_add(SCI_TRACE_DIR_MASTER_TO_EXT,
      master_mcu->transmit_shift_register, master_mcu->counter);
    serial_tx_fifo_write(master_mcu->transmit_shift_register);
    master_mcu->transmit_shift_register = -1;
  }

  /* Sync to 8 bits with 38400 baudrate: */
  if (master_mcu->sync_counter % 128 == 0) {
    if (serial_rx_fifo_read(&byte)) {
      /* SCI transfer from external interface to master MCU: */
      debugger_sci_trace_add(SCI_TRACE_DIR_EXT_TO_MASTER,
        byte, master_mcu->counter);
      hd6301_sci_receive(master_mcu, master_mem, byte);
    }

    /* Send data to real TTY: */
    if (serial_tx_fifo_read(&byte)) {
      write(serial_tty_fd, &byte, 1);
    }
  }

  /* Check if real TTY has data available: */
  if (read(serial_tty_fd, &byte, 1) == 1) {
    serial_rx_fifo_write(byte);
  }
}



