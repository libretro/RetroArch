/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_mf_audio.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static unsigned packets;
static int64_t previous;

#ifdef _WIN32
static DWORD WINAPI wrong_owner(void *opaque)
{
   record_mf_audio_t *e = (record_mf_audio_t*)opaque;
   record_mf_audio_packet_t packet;
   const void *config = NULL;
   int16_t *samples = NULL;
   size_t frames = 0, bytes = 0;
   assert(record_mf_audio_input(e, &samples, &frames) == RECORD_MF_AUDIO_ERROR);
   assert(record_mf_audio_submit(e, 1, 0) == RECORD_MF_AUDIO_ERROR);
   assert(record_mf_audio_pump(e, &packet) == RECORD_MF_AUDIO_ERROR);
   assert(record_mf_audio_finish(e) == RECORD_MF_AUDIO_ERROR);
   assert(!record_mf_audio_config(e, &config, &bytes));
   assert(!record_mf_audio_cancel(e) && !record_mf_audio_release(e));
   assert(!record_mf_audio_free(e));
   return 0;
}
#endif

static int lifecycle(void)
{
   record_mf_audio_t *e;
   record_mf_audio_packet_t packet;
   int16_t *samples, *again;
   size_t frames, repeated;
#ifdef _WIN32
   HANDLE thread;
#endif
   assert(!record_mf_audio_new(32000, 2, 128000));
   assert(!record_mf_audio_new(48000, 6, 128000));
   assert(!record_mf_audio_new(48000, 2, 64000));
   e = record_mf_audio_new(48000, 2, 128000);
   if (!e)
   {
      fputs("Native Windows AAC encoder is unavailable\n", stderr);
      return 77;
   }
#ifdef _WIN32
   thread = CreateThread(NULL, 0, wrong_owner, e, 0, NULL);
   assert(thread && WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
   assert(CloseHandle(thread));
#endif
   assert(record_mf_audio_pump(e, &packet) == RECORD_MF_AUDIO_BUSY);
   assert(record_mf_audio_input(e, &samples, &frames) == RECORD_MF_AUDIO_PROGRESS);
   assert(record_mf_audio_submit(e, 1025, 0) == RECORD_MF_AUDIO_ERROR);
   assert(record_mf_audio_submit(e, 1, ~(uint64_t)0) == RECORD_MF_AUDIO_ERROR);
   assert(record_mf_audio_input(e, &again, &repeated) == RECORD_MF_AUDIO_PROGRESS);
   assert(again == samples && repeated == frames && !record_mf_audio_error(e));
   assert(record_mf_audio_cancel(e) && record_mf_audio_cancel(e));
   assert(record_mf_audio_input(e, &samples, &frames) == RECORD_MF_AUDIO_ERROR);
   assert(record_mf_audio_pump(e, &packet) == RECORD_MF_AUDIO_ERROR);
   assert(record_mf_audio_free(e));
   e = record_mf_audio_new(44100, 1, 128000);
   assert(e && record_mf_audio_finish(e) == RECORD_MF_AUDIO_PROGRESS);
   assert(record_mf_audio_pump(e, &packet) == RECORD_MF_AUDIO_END);
   assert(record_mf_audio_free(e));
   e = record_mf_audio_new(48000, 2, 128000);
   assert(e && record_mf_audio_input(e, &samples, &frames) == RECORD_MF_AUDIO_PROGRESS);
   memset(samples, 0, frames * 2 * sizeof(*samples));
   assert(record_mf_audio_submit(e, frames, 0) == RECORD_MF_AUDIO_PROGRESS);
   assert(record_mf_audio_cancel(e) && record_mf_audio_cancel(e));
   assert(record_mf_audio_free(e));
   puts("PASS: native AAC cancellation, owner, empty stream and clock bounds");
   return 0;
}

static int output(record_mf_audio_t *e, FILE *file, unsigned rate, unsigned channels, bool draining)
{
   record_mf_audio_packet_t p, again;
   unsigned index = rate == 48000 ? 3 : 4;
   uint8_t header[7];
   size_t size;
   int16_t *input = NULL;
   size_t frames = 0;
   int result = record_mf_audio_pump(e, &p);
   if (result != RECORD_MF_AUDIO_PACKET)
      return result;
   assert(p.data && p.size && p.size < 8192 - 7 && p.duration > 0);
   assert(!packets || p.timestamp > previous);
   previous = p.timestamp;
   assert(record_mf_audio_pump(e, &again) == RECORD_MF_AUDIO_PACKET);
   assert(again.data == p.data && again.size == p.size
         && again.timestamp == p.timestamp && again.duration == p.duration);
   assert(!record_mf_audio_cancel(e) && !record_mf_audio_free(e));
   if (!draining)
      assert(record_mf_audio_finish(e) == RECORD_MF_AUDIO_BUSY);
   assert(record_mf_audio_input(e, &input, &frames)
         == (draining ? RECORD_MF_AUDIO_ERROR : RECORD_MF_AUDIO_BUSY));
   assert(!input && !frames);
   size = p.size + 7;
   header[0] = 255;
   header[1] = 241;
   header[2] = (uint8_t)(64 | (index << 2) | (channels >> 2));
   header[3] = (uint8_t)((channels << 6) | (size >> 11));
   header[4] = (uint8_t)(size >> 3);
   header[5] = (uint8_t)((size << 5) | 31);
   header[6] = 252;
   assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
   assert(fwrite(p.data, 1, p.size, file) == p.size);
   printf("packet %u: time=%lu:%lu duration=%lu:%lu bytes=%u\n", packets,
         (unsigned long)((uint64_t)p.timestamp >> 32), (unsigned long)(uint32_t)p.timestamp,
         (unsigned long)((uint64_t)p.duration >> 32), (unsigned long)(uint32_t)p.duration,
         (unsigned)p.size);
   packets++;
   assert(record_mf_audio_release(e) && !record_mf_audio_release(e));
   return result;
}

int main(int argc, char **argv)
{
   record_mf_audio_t *e;
   const void *config;
   size_t config_size, capacity;
   uint64_t epoch = 0;
   unsigned rate, channels, bitrate, at, i, c, attempts = 0, total = 32768;
   FILE *file;
   int result;
   if (argc == 2 && !strcmp(argv[1], "--lifecycle"))
      return lifecycle();
   assert(argc >= 5 && argc <= 7);
   rate = (unsigned)strtoul(argv[2], NULL, 10);
   channels = (unsigned)strtoul(argv[3], NULL, 10);
   bitrate = (unsigned)strtoul(argv[4], NULL, 10);
   if (argc >= 6) total = (unsigned)strtoul(argv[5], NULL, 10);
   assert(total && total <= 65536);
   if (argc == 7)
   {
      assert(!strcmp(argv[6], "large"));
      epoch = ((uint64_t)1 << 33) + 17;
   }
   e = record_mf_audio_new(rate, channels, bitrate);
   if (!e)
   {
      fprintf(stderr, "Native AAC encoder unavailable for %u/%u/%u\n", rate, channels, bitrate);
      return 77;
   }
   assert(record_mf_audio_config(e, &config, &config_size));
   assert(config_size >= 2 && ((const uint8_t*)config)[0] >> 3 == 2);
   assert(!record_mf_audio_free(e));
   file = fopen(argv[1], "wb");
   assert(file);
   for (at = 0; at < total; at += (unsigned)capacity)
   {
      int16_t *input = NULL, *again = NULL;
      size_t repeated = 0;
      assert(record_mf_audio_input(e, &input, &capacity) == RECORD_MF_AUDIO_PROGRESS);
      assert(input && capacity == 1024);
      assert(record_mf_audio_input(e, &again, &repeated) == RECORD_MF_AUDIO_PROGRESS);
      assert(again == input && repeated == capacity);
      if (capacity > total - at) capacity = total - at;
      assert(record_mf_audio_submit(e, 0, epoch + at) == RECORD_MF_AUDIO_ERROR);
      assert(record_mf_audio_finish(e) == RECORD_MF_AUDIO_BUSY);
      assert(!record_mf_audio_free(e));
      for (i = 0; i < capacity; i++)
         for (c = 0; c < channels; c++)
         {
            double position = at + i;
            double phase = (c ? 1543 : 997) * position / rate
               + (c ? 900.0 : 500.0) * position * position / (2.0 * rate * 32768);
            input[i * channels + c] = (int16_t)(12000.0 * sin(6.283185307179586 * phase));
         }
      do
      {
         assert(++attempts < 10000);
         result = record_mf_audio_submit(e, capacity, epoch + at);
         if (result == RECORD_MF_AUDIO_BUSY)
            assert(output(e, file, rate, channels, false) == RECORD_MF_AUDIO_PACKET);
      } while (result == RECORD_MF_AUDIO_BUSY);
      assert(result == RECORD_MF_AUDIO_PROGRESS);
      again = NULL;
      repeated = 0;
      assert(record_mf_audio_input(e, &again, &repeated) == RECORD_MF_AUDIO_BUSY);
      assert(!again && !repeated);
      while ((result = output(e, file, rate, channels, false)) == RECORD_MF_AUDIO_PACKET)
         assert(++attempts < 10000);
      if (result != RECORD_MF_AUDIO_BUSY)
      {
         fprintf(stderr, "pump result %d HRESULT %08lx\n", result,
               (unsigned long)record_mf_audio_error(e));
         return 1;
      }
   }
   assert(record_mf_audio_finish(e) == RECORD_MF_AUDIO_PROGRESS);
   assert(record_mf_audio_finish(e) == RECORD_MF_AUDIO_PROGRESS);
   do
   {
      assert(++attempts < 10000);
      result = output(e, file, rate, channels, true);
      assert(result != RECORD_MF_AUDIO_ERROR && result != RECORD_MF_AUDIO_BUSY);
   } while (result != RECORD_MF_AUDIO_END);
   assert(record_mf_audio_finish(e) == RECORD_MF_AUDIO_END);
   assert(!record_mf_audio_error(e) && packets == (total + 1023) / 1024);
   assert(!fclose(file));
   assert(record_mf_audio_free(e));
   printf("PASS: native AAC %u Hz/%u channels/%u bit/s, %u packets\n",
         rate, channels, bitrate, packets);
   return 0;
}
