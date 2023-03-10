#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include "hd6301.h"
#include "mem.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_VOLUME 16 /* 0 -> 127 */

#define PIEZO_SAMPLE_FACTOR 14 /* MCU cycles for each sample in sample rate. */
#define PIEZO_FIFO_SIZE 32768 /* More than SDL sample size times factor. */
#define PIEZO_OFF_TICK_COUNT 2000 /* Ticks until piezo should be silenced. */



static int8_t piezo_fifo[PIEZO_FIFO_SIZE];
static int piezo_fifo_head = 0;
static int piezo_fifo_tail = 0;



static int8_t piezo_fifo_read(void)
{
  int8_t sample;

  if (piezo_fifo_tail == piezo_fifo_head) {
    return 0; /* Empty */
  }

  sample = piezo_fifo[piezo_fifo_tail];
  piezo_fifo_tail = (piezo_fifo_tail + 1) % PIEZO_FIFO_SIZE;

  return sample;
}



static void piezo_fifo_write(int8_t sample)
{
  if (((piezo_fifo_head + 1) % PIEZO_FIFO_SIZE) == piezo_fifo_tail) {
    return; /* Full */
  }

  piezo_fifo[piezo_fifo_head] = sample;
  piezo_fifo_head = (piezo_fifo_head + 1) % PIEZO_FIFO_SIZE;
}



static void piezo_callback(void *userdata, Uint8 *stream, int len)
{
  int i, j;
  int16_t sample;
  (void)userdata;

  for (i = 0; i < len; i++) {
    sample = 0;
    for (j = 0; j < PIEZO_SAMPLE_FACTOR; j++) {
      sample += piezo_fifo_read();
    }
    sample /= PIEZO_SAMPLE_FACTOR;
    stream[i] = (Uint8)(127 + (sample * AUDIO_VOLUME));
  }
}



static void piezo_exit_handler(void)
{
  SDL_PauseAudio(1);
  SDL_CloseAudio();
  SDL_Quit();
}



int piezo_init(void)
{
  SDL_AudioSpec desired, obtained;

  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "Unable to initalize SDL: %s\n", SDL_GetError());
    return -1;
  }
  atexit(piezo_exit_handler);

  desired.freq     = AUDIO_SAMPLE_RATE;
  desired.format   = AUDIO_U8;
  desired.channels = 1;
  desired.samples  = 2048;
  desired.userdata = 0;
  desired.callback = piezo_callback;

  if (SDL_OpenAudio(&desired, &obtained) != 0) {
    fprintf(stderr, "SDL_OpenAudio() failed: %s\n", SDL_GetError());
    return -1;
  }

  if (obtained.format != AUDIO_U8) {
    fprintf(stderr, "Did not get unsigned 8-bit audio format!\n");
    SDL_CloseAudio();
    return -1;
  }

  SDL_PauseAudio(0);
  return 0;
}



void piezo_execute(hd6301_t *slave_mcu, mem_t *slave_mem)
{
  static int off_ticks = PIEZO_OFF_TICK_COUNT;
  static uint16_t sync_catchup = 0;

  while (slave_mcu->sync_counter != sync_catchup) {
    if (slave_mem->ram[HD6301_REG_PORT_1] & 0x20) {
      piezo_fifo_write(1);
      off_ticks = 0;

    } else {
      if (off_ticks >= PIEZO_OFF_TICK_COUNT) {
        piezo_fifo_write(0);

      } else {
        piezo_fifo_write(-1);
        off_ticks++;
      }
    }
    sync_catchup++;
  }
}



