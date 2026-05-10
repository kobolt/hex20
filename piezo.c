#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

#include "hd6301.h"
#include "mem.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS 1
#define AUDIO_VOLUME 16 /* 0 -> 127 */

#define PIEZO_SAMPLE_RATE 612900 /* HX-20 Clock Speed */



static SDL_AudioStream *piezo_audio_stream = NULL;
static bool piezo_callback_done = false;
static bool piezo_silence = false;
static Uint16 piezo_samples = 0;



static void piezo_callback(void *userdata, Uint8 *stream, int len)
{
  int i;
  int bytes;
  (void)userdata;
  uint8_t sample;

  for (i = 0; i < len; i++) {
    bytes = SDL_AudioStreamGet(piezo_audio_stream, &sample, 1);
    if (bytes == 1) {
      stream[i] = sample;
    } else {
      stream[i] = 128; /* Silence */
    }
  }

  piezo_callback_done = true;
}



static void piezo_exit_handler(void)
{
  SDL_PauseAudio(1);
  SDL_CloseAudio();
  SDL_FreeAudioStream(piezo_audio_stream);
  SDL_Quit();
}



int piezo_init(bool silence)
{
  SDL_AudioSpec desired, obtained;

  if (SDL_Init(SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "Unable to initalize SDL: %s\n", SDL_GetError());
    return -1;
  }
  atexit(piezo_exit_handler);

  desired.freq     = AUDIO_SAMPLE_RATE;
  desired.format   = AUDIO_U8;
  desired.channels = AUDIO_CHANNELS;
  desired.samples  = 0;
  desired.userdata = 0;
  desired.callback = piezo_callback;

  if (SDL_OpenAudio(&desired, &obtained) != 0) {
    fprintf(stderr, "SDL_OpenAudio() failed: %s\n", SDL_GetError());
    return -1;
  }

  if (obtained.freq != AUDIO_SAMPLE_RATE) {
    fprintf(stderr, "Did not get %d audio sample rate!\n", AUDIO_SAMPLE_RATE);
    SDL_CloseAudio();
    return -1;
  }

  if (obtained.channels != AUDIO_CHANNELS) {
    fprintf(stderr, "Did not get %d audio channel(s)!\n", AUDIO_CHANNELS);
    SDL_CloseAudio();
    return -1;
  }

  if (obtained.format != AUDIO_U8) {
    fprintf(stderr, "Did not get unsigned 8-bit audio format!\n");
    SDL_CloseAudio();
    return -1;
  }

  piezo_samples = obtained.samples;
  piezo_silence = silence;
  piezo_audio_stream = SDL_NewAudioStream(
    obtained.format, AUDIO_CHANNELS, PIEZO_SAMPLE_RATE,
    obtained.format, AUDIO_CHANNELS, AUDIO_SAMPLE_RATE);
  if (piezo_audio_stream == NULL) {
    fprintf(stderr, "SDL_NewAudioStream() failed: %s\n", SDL_GetError());
    SDL_CloseAudio();
    return -1;
  }

  SDL_PauseAudio(0);
  return 0;
}



void piezo_execute(hd6301_t *slave_mcu, mem_t *slave_mem)
{
  static uint16_t sync_catchup = 0;
  uint8_t sample;
  bool piezo_wait = false;

  SDL_LockAudio();

  while (slave_mcu->sync_counter != sync_catchup) {
    if ((slave_mem->ram[HD6301_REG_PORT_1] & 0x20) && piezo_silence == false) {
      sample = 128 + AUDIO_VOLUME;
    } else {
      sample = 128; /* Silence */
    }
    SDL_AudioStreamPut(piezo_audio_stream, &sample, 1);

    if (SDL_AudioStreamAvailable(piezo_audio_stream) > piezo_samples) {
      piezo_callback_done = false;
      piezo_wait = true;
    }

    sync_catchup++;
  }

  SDL_UnlockAudio();

  if (piezo_wait) {
    while (piezo_callback_done == false) {
      SDL_Delay(10);
    }
  }
}



