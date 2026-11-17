/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_aac_audio.h"
#include <formats/raac_encode.h>
#include <rthreads/rthreads.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uintptr_t aac_test_owner = 1;
uintptr_t sthread_get_current_thread_id(void)
{
   return aac_test_owner;
}

struct aac_test
{
   uint8_t expected[RAAC_ENCODE_MAX_FRAME], held[RAAC_ENCODE_MAX_FRAME];
   int16_t pcm[RAAC_ENCODE_FRAME * 2];
};

static int64_t aac_test_time(uint64_t frame, unsigned rate)
{
   return (int64_t)(frame / rate * 10000000 + frame % rate * 10000000 / rate);
}

static void aac_test_packet(record_aac_audio_t *encoder, struct aac_test *test,
      const uint8_t *expected, size_t bytes, uint64_t index, size_t frames,
      unsigned rate, unsigned channels, FILE *file)
{
   record_aac_audio_packet_t packet, again;
   int16_t *input;
   size_t capacity;
   uint8_t adts[7];
   int result;
   unsigned steps = 0;
   for (steps = 0; steps < 3; steps++)
   {
      result = record_aac_audio_pump(encoder, &packet);
      if (result != RECORD_AAC_AUDIO_PROGRESS)
         break;
   }
   assert(result == RECORD_AAC_AUDIO_PACKET);
   assert(packet.timestamp == aac_test_time(index, rate));
   assert(packet.duration == aac_test_time(index + RAAC_ENCODE_FRAME, rate) - packet.timestamp);
   assert(frames && frames <= RAAC_ENCODE_FRAME);
   assert(packet.data && packet.size == bytes && !memcmp(packet.data, expected, bytes));
   memcpy(test->held, packet.data, bytes);
   assert(record_aac_audio_input(encoder, &input, &capacity) == RECORD_AAC_AUDIO_BUSY);
   assert(!input && !capacity);
   assert(record_aac_audio_finish(encoder) == RECORD_AAC_AUDIO_BUSY);
   assert(!record_aac_audio_cancel(encoder));
   assert(!record_aac_audio_free(encoder));
   assert(record_aac_audio_pump(encoder, &again) == RECORD_AAC_AUDIO_PACKET);
   assert(again.data == packet.data && again.size == bytes
         && again.timestamp == packet.timestamp && again.duration == packet.duration
         && !memcmp(packet.data, test->held, bytes));
   if (file)
   {
      size_t length = bytes + 7;
      adts[0] = 255; adts[1] = 241;
      adts[2] = (uint8_t)(64 | ((rate == 48000 ? 3 : 4) << 2) | (channels >> 2));
      adts[3] = (uint8_t)((channels << 6) | (length >> 11));
      adts[4] = (uint8_t)(length >> 3); adts[5] = (uint8_t)((length << 5) | 31); adts[6] = 252;
      assert(fwrite(adts, 1, 7, file) == 7);
      assert(fwrite(packet.data, 1, bytes, file) == bytes);
   }
   assert(record_aac_audio_release(encoder));
   assert(!record_aac_audio_release(encoder));
}

static void aac_test_run(unsigned rate, unsigned channels, unsigned bitrate, const char *directory)
{
   record_aac_audio_t *encoder = record_aac_audio_new(rate, channels, bitrate);
   raac_encoder_config_t config;
   raac_encoder_t *reference;
   struct aac_test *test = (struct aac_test*)calloc(1, sizeof(*test));
   uint64_t origin = ((uint64_t)1 << 48) + rate * 3600u + 7, position = 0, previous = 0;
   unsigned block, channel;
   FILE *file = NULL, *pcm = NULL;
   const void *asc;
   size_t asc_size, allocations;
   assert(encoder && test);
   config.sample_rate = rate;
   config.channels = channels;
   config.bitrate = bitrate;
   reference = raac_encoder_new(&config);
   assert(reference);
   assert(record_aac_audio_config(encoder, &asc, &asc_size) && asc_size == 2);
   if (directory)
   {
      char path[1024];
      assert(strlen(directory) < 900);
      sprintf(path, "%s/aac-%u-%u.aac", directory, rate, channels);
      file = fopen(path, "wb");
      assert(file);
      sprintf(path, "%s/aac-%u-%u.s16", directory, rate, channels);
      pcm = fopen(path, "wb");
      assert(pcm);
   }
   allocations = pressure_calls;
   for (block = 0; block < 9; block++)
   {
      int16_t *input, *again;
      size_t frames, repeated, i, count = block == 8 ? 173 : RAAC_ENCODE_FRAME, bytes;
      assert(record_aac_audio_input(encoder, &input, &frames) == RECORD_AAC_AUDIO_PROGRESS);
      assert(frames == RAAC_ENCODE_FRAME);
      assert(record_aac_audio_input(encoder, &again, &repeated) == RECORD_AAC_AUDIO_PROGRESS);
      assert(input == again && frames == repeated);
      for (i = 0; i < count; i++)
         for (channel = 0; channel < channels; channel++)
            input[i * channels + channel] = position + i < 500 ? 0 :
               (int16_t)(12000 * sin(6.283185307179586 * (channel ? 880 : 440) * (position + i) / rate));
      if (pcm)
         assert(fwrite(input, channels * sizeof(*input), count, pcm) == count);
      memcpy(test->pcm, input, count * channels * sizeof(*input));
      memset(test->pcm + count * channels, 0, (RAAC_ENCODE_FRAME - count) * channels * sizeof(*input));
      assert(record_aac_audio_submit(encoder, 0, origin + position) == RECORD_AAC_AUDIO_ERROR);
      assert(record_aac_audio_submit(encoder, RAAC_ENCODE_FRAME + 1, origin + position) == RECORD_AAC_AUDIO_ERROR);
      if (block)
         assert(record_aac_audio_submit(encoder, count, origin + position + 1) == RECORD_AAC_AUDIO_ERROR);
#ifndef AAC_TEST_THREADLESS
      aac_test_owner = 2;
      assert(record_aac_audio_submit(encoder, count, origin + position) == RECORD_AAC_AUDIO_ERROR);
      assert(!record_aac_audio_cancel(encoder) && !record_aac_audio_free(encoder));
      aac_test_owner = 1;
#endif
      assert(record_aac_audio_submit(encoder, count, origin + position) == RECORD_AAC_AUDIO_PROGRESS);
      bytes = raac_encoder_encode_s16(reference, test->pcm, test->expected, sizeof(test->expected), false);
      assert(bytes);
      if (count < RAAC_ENCODE_FRAME)
         assert(record_aac_audio_finish(encoder) == RECORD_AAC_AUDIO_PROGRESS);
      if (block)
         aac_test_packet(encoder, test, test->expected, bytes, origin + position - previous,
               previous, rate, channels, file);
      else
      {
         record_aac_audio_packet_t packet;
         assert(record_aac_audio_pump(encoder, &packet) == RECORD_AAC_AUDIO_BUSY);
         assert(!packet.data && !packet.size && !record_aac_audio_release(encoder));
      }
      previous = count;
      position += count;
   }
   assert(record_aac_audio_finish(encoder) == RECORD_AAC_AUDIO_PROGRESS);
   {
      size_t bytes = raac_encoder_encode_s16(reference, NULL, test->expected, sizeof(test->expected), false);
      assert(bytes);
      aac_test_packet(encoder, test, test->expected, bytes, origin + position - previous,
            previous, rate, channels, file);
   }
   assert(record_aac_audio_finish(encoder) == RECORD_AAC_AUDIO_PROGRESS);
   {
      record_aac_audio_packet_t packet;
      assert(record_aac_audio_pump(encoder, &packet) == RECORD_AAC_AUDIO_END);
      assert(record_aac_audio_pump(encoder, &packet) == RECORD_AAC_AUDIO_END);
   }
   assert(pressure_calls == allocations && !record_aac_audio_error(encoder));
   assert(record_aac_audio_free(encoder));
   raac_encoder_free(reference);
   free(test);
   if (file)
      assert(!fclose(file));
   if (pcm)
      assert(!fclose(pcm));
   printf("PASS builtin AAC owner %u Hz %u channels %u bps: source clock, priming, partial tail, loans and wrong owner\n", rate, channels, bitrate);
}

static void aac_test_edges(void)
{
   record_aac_audio_t *e;
   record_aac_audio_packet_t packet;
   int16_t *input;
   size_t frames;
   assert(!record_aac_audio_new(0, 2, 128000));
   assert(!record_aac_audio_new(32000, 2, 128000));
   assert(!record_aac_audio_new(48000, 6, 128000));
   assert(!record_aac_audio_new(48000, 2, 1));
   assert(record_aac_audio_free(NULL));
   e = record_aac_audio_new(48000, 2, 96000);
   assert(e);
   assert(record_aac_audio_finish(e) == RECORD_AAC_AUDIO_PROGRESS);
   assert(record_aac_audio_pump(e, &packet) == RECORD_AAC_AUDIO_END);
   assert(record_aac_audio_free(e));
   e = record_aac_audio_new(48000, 2, 192000);
   assert(e);
   assert(record_aac_audio_input(e, &input, &frames) == RECORD_AAC_AUDIO_PROGRESS);
   memset(input, 0, frames * 2 * sizeof(*input));
   assert(record_aac_audio_submit(e, 1024, UINT64_MAX - 1) == RECORD_AAC_AUDIO_ERROR);
   assert(record_aac_audio_submit(e, 1024, 0) == RECORD_AAC_AUDIO_PROGRESS);
   assert(record_aac_audio_input(e, &input, &frames) == RECORD_AAC_AUDIO_PROGRESS);
   assert(record_aac_audio_submit(e, 1024, 1024) == RECORD_AAC_AUDIO_PROGRESS);
   /* Unpublished output may be cancelled without manufacturing a borrow. */
   assert(record_aac_audio_cancel(e));
   assert(record_aac_audio_cancel(e));
   assert(record_aac_audio_pump(e, &packet) == RECORD_AAC_AUDIO_ERROR);
   assert(record_aac_audio_free(e));
   e = record_aac_audio_new(44100, 1, 160000);
   assert(e);
   {
      size_t filled = 0, part;
      int16_t *base = NULL;
      for (part = 0; part < 3; part++)
      {
         size_t count = part == 0 ? 17 : part == 1 ? 200 : 807;
         assert(record_aac_audio_input(e, &input, &frames) == RECORD_AAC_AUDIO_PROGRESS);
         if (!base)
            base = input;
         assert(input == base + filled && frames == 1024 - filled);
         assert(record_aac_audio_submit(e, frames + 1, filled) == RECORD_AAC_AUDIO_ERROR);
         memset(input, 0, count * sizeof(*input));
         assert(record_aac_audio_submit(e, count, filled) == RECORD_AAC_AUDIO_PROGRESS);
         filled += count;
      }
      assert(record_aac_audio_finish(e) == RECORD_AAC_AUDIO_PROGRESS);
      assert(record_aac_audio_pump(e, &packet) == RECORD_AAC_AUDIO_PROGRESS);
      assert(record_aac_audio_pump(e, &packet) == RECORD_AAC_AUDIO_PACKET);
      assert(packet.timestamp == 0 && packet.duration == aac_test_time(1024, 44100));
      assert(record_aac_audio_release(e));
      assert(record_aac_audio_pump(e, &packet) == RECORD_AAC_AUDIO_END);
      assert(record_aac_audio_free(e));
   }
   puts("PASS builtin AAC empty drain, overflow refusal and cancellation");
}

int main(int argc, char **argv)
{
   unsigned rate, channels, bitrate;
   aac_test_edges();
   for (rate = 44100; rate <= 48000; rate += 3900)
      for (channels = 1; channels <= 2; channels++)
         for (bitrate = 96000; bitrate <= 192000; bitrate += 32000)
            aac_test_run(rate, channels, bitrate, argc > 1 && bitrate == 128000 ? argv[1] : NULL);
   assert(!pressure_live && !pressure_bytes);
   return 0;
}
