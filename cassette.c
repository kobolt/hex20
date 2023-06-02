#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "hd6301.h"
#include "mem.h"



#define CASSETTE_SAVE_IDLE_STOP 500000 /* Until save is stopped. */
#define CASSETTE_INTERNAL_SAMPLE_RATE 612900 /* HX-20 Clock Speed */
#define CASSETTE_WAV_SAMPLE_RATE 44100



typedef struct wav_header_s {
  uint8_t riff_string[4];
  uint32_t chunk_size;
  uint8_t wave_string[4];
  uint8_t fmt_string[4];
  uint32_t subchunk1_size;
  uint16_t audio_format;
  uint16_t channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
  uint8_t data_string[4];
  uint32_t subchunk2_size;
} wav_header_t;



static FILE *cassette_save_fh = NULL;
static FILE *cassette_load_fh = NULL;
static uint32_t cassette_save_sample_count = 0;



int cassette_load_file(const char *filename)
{
  wav_header_t header;

  if (cassette_load_fh != NULL) {
    return -2; /* Load already in progress. */
  }

  cassette_load_fh = fopen(filename, "rb");
  if (cassette_load_fh == NULL) {
    return -1; /* File not found. */
  }

  if (fread(&header, sizeof(wav_header_t), 1, cassette_load_fh) != 1) {
    fclose(cassette_load_fh);
    cassette_load_fh = NULL;
    return -3; /* Unable to read header. */
  }

  if (header.riff_string[0] != 'R') {
    fclose(cassette_load_fh);
    cassette_load_fh = NULL;
    return -4; /* Not a WAV file? */
  }

  if (header.sample_rate != CASSETTE_WAV_SAMPLE_RATE) {
    fclose(cassette_load_fh);
    cassette_load_fh = NULL;
    return -5; /* Unsupported sample rate. */
  }

  if (header.channels != 1) {
    fclose(cassette_load_fh);
    cassette_load_fh = NULL;
    return -6; /* Unsupported channels. */
  }

  if (header.bits_per_sample != 8) {
    fclose(cassette_load_fh);
    cassette_load_fh = NULL;
    return -7; /* Unsupported BPS. */
  }

  return 0;
}



int cassette_save_file(const char *filename)
{
  wav_header_t header;

  if (cassette_save_fh != NULL) {
    return -2; /* Save already in progress. */
  }

  cassette_save_fh = fopen(filename, "wb");
  if (cassette_save_fh == NULL) {
    return -1; /* File not found. */
  }

  cassette_save_sample_count = 0;

  /* Prepare and write WAV header: */
  header.riff_string[0] = 'R';
  header.riff_string[1] = 'I';
  header.riff_string[2] = 'F';
  header.riff_string[3] = 'F';
  header.chunk_size = 0; /* Unknown until finished. */
  header.wave_string[0] = 'W';
  header.wave_string[1] = 'A';
  header.wave_string[2] = 'V';
  header.wave_string[3] = 'E';
  header.fmt_string[0] = 'f';
  header.fmt_string[1] = 'm';
  header.fmt_string[2] = 't';
  header.fmt_string[3] = ' ';
  header.subchunk1_size = 16;
  header.audio_format = 1; /* PCM */
  header.channels = 1; /* Mono */
  header.sample_rate = CASSETTE_WAV_SAMPLE_RATE;
  header.byte_rate = CASSETTE_WAV_SAMPLE_RATE;
  header.block_align = 1;
  header.bits_per_sample = 8;
  header.data_string[0] = 'd';
  header.data_string[1] = 'a';
  header.data_string[2] = 't';
  header.data_string[3] = 'a';
  header.subchunk2_size = 0; /* Unknown until finished. */

  fwrite(&header, sizeof(wav_header_t), 1, cassette_save_fh);
  return 0;
}



static void cassette_save_file_stop(void)
{
  uint32_t chunk_size;
  uint32_t subchunk2_size;

  subchunk2_size = cassette_save_sample_count;
  chunk_size = subchunk2_size + 36;

  /* Update WAV header with chunk sizes before closing: */
  fseek(cassette_save_fh, 4, SEEK_SET);
  fwrite(&chunk_size, sizeof(uint32_t), 1, cassette_save_fh);
  fseek(cassette_save_fh, 40, SEEK_SET);
  fwrite(&subchunk2_size, sizeof(uint32_t), 1, cassette_save_fh);

  fclose(cassette_save_fh);
  cassette_save_fh = NULL;
}



static void cassette_save_sample(bool level)
{
  static uint32_t internal_sample_count = 0;
  uint8_t sample;

  if (internal_sample_count %
    (CASSETTE_INTERNAL_SAMPLE_RATE / CASSETTE_WAV_SAMPLE_RATE) == 0) {
    if (level == true) {
      sample = UINT8_MAX;
    } else {
      sample = 0;
    }
    fwrite(&sample, sizeof(uint8_t), 1, cassette_save_fh);
    cassette_save_sample_count++;
  }

  internal_sample_count++;
}



static bool cassette_load_sample(void)
{
  static uint32_t internal_sample_count = 0;
  static uint8_t sample = 0;

  if (internal_sample_count %
    (CASSETTE_INTERNAL_SAMPLE_RATE / CASSETTE_WAV_SAMPLE_RATE) == 0) {
    if (fread(&sample, sizeof(uint8_t), 1, cassette_load_fh) != 1) {
      fclose(cassette_load_fh);
      cassette_load_fh = NULL;
      sample = 0;
    }
  }

  internal_sample_count++;

  if (sample > 128) {
    return true;
  } else {
    return false;
  }
}



void cassette_execute(hd6301_t *slave_mcu, mem_t *slave_mem)
{
  static uint16_t sync_catchup = 0;
  static uint32_t save_idle_count;
  static bool save_high_seen = false;

  while (slave_mcu->sync_counter != sync_catchup) {
    /* Saving */
    if (cassette_save_fh != NULL) {
      if (slave_mem->ram[HD6301_REG_PORT_3] & 0x08) { /* Port P33 high. */
        cassette_save_sample(true);
        save_idle_count = 0;
        save_high_seen = true;

      } else { /* Port P33 low. */
        /* Don't save initial low samples. */
        if (save_high_seen) {
          cassette_save_sample(false);
          save_idle_count++;
          if (save_idle_count >= CASSETTE_SAVE_IDLE_STOP) {
            /* Stop saving automatically. */
            cassette_save_file_stop();
            save_high_seen = false;
          }
        }
      }
    }

    /* Loading */
    if (cassette_load_fh != NULL) {
      if (cassette_load_sample() == true) {
        slave_mem->ram[HD6301_REG_PORT_3] |= 0x04; /* Set port P32. */
      } else {
        slave_mem->ram[HD6301_REG_PORT_3] &= ~0x04; /* Reset port P32. */
      }
    }
    sync_catchup++;
  }
}



