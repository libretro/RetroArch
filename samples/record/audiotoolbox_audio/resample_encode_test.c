/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main unused_aac_test_main
#include "encode_test.c"
#undef main
#include "../../../record/record_at_resample.h"

int main(int argc, char **argv)
{
   record_audio_queue_t *q;
   record_at_resample_t *src;
   record_at_audio_t *aac;
   record_at_audio_packet_t packet;
   int16_t source[512];
   int16_t *output;
   FILE *file, *pcm;
   double input_rate;
   uint64_t accepted, produced, total = 0;
   size_t frames, capacity, i;
   unsigned rate, channels, offset = 0, turns, packets = 0, leading, trailing;
   int result = RECORD_AT_AUDIO_BUSY, converted;
   bool closed = false, ended = false, finishing = false;
   assert(argc == 6);
   input_rate = strtod(argv[1], NULL);
   rate = (unsigned)strtoul(argv[2], NULL, 10);
   channels = (unsigned)strtoul(argv[3], NULL, 10);
   assert(channels == 1 || channels == 2);
   q = record_audio_queue_new(channels, 256, 8192);
   assert(q);
   src = record_at_resample_new(q, input_rate, rate);
   aac = record_at_audio_new(rate, channels, channels == 1 ? 96000 : 128000);
   assert(src && aac);
   file = fopen(argv[4], "wb");
   pcm = fopen(argv[5], "wb");
   assert(file && pcm);
   for (turns = 0; turns < 200000; turns++)
   {
      if (offset < SOURCE_FRAMES)
      {
         frames = SOURCE_FRAMES - offset;
         if (frames > 256)
            frames = 256;
         for (i = 0; i < frames * channels; i++)
         {
            double phase = (offset + i / channels) * 6.283185307179586
               * (i % channels ? 1499.0 : 997.0) / input_rate;
            source[i] = (int16_t)(sin(phase) * 12000.0);
         }
         converted = record_audio_queue_push(q, source, frames, offset);
         assert(converted == RECORD_AUDIO_OK || converted == RECORD_AUDIO_BUSY);
         if (converted == RECORD_AUDIO_OK)
            offset += (unsigned)frames;
      }
      if (offset == SOURCE_FRAMES && !closed)
      {
         assert(record_audio_queue_finish(q));
         closed = true;
      }
      if (!ended)
      {
         result = record_at_audio_input(aac, &output, &capacity);
         assert(result != RECORD_AT_AUDIO_ERROR);
         if (result == RECORD_AT_AUDIO_PROGRESS)
         {
            converted = record_at_resample_read(src, output, capacity, &frames);
            if (converted == RECORD_AT_RESAMPLE_ERROR)
               fprintf(stderr, "SRC error %ld\n", (long)record_at_resample_error(src));
            assert(converted != RECORD_AT_RESAMPLE_ERROR);
            if (frames)
            {
               assert(fwrite(output, channels * sizeof(int16_t), frames, pcm) == frames);
               assert(record_at_audio_submit(aac, frames, total) == RECORD_AT_AUDIO_PROGRESS);
               total += frames;
            }
            else
               assert(record_at_audio_input_release(aac));
            ended = converted == RECORD_AT_RESAMPLE_END;
         }
      }
      if (ended && !finishing)
      {
         result = record_at_audio_finish(aac);
         assert(result != RECORD_AT_AUDIO_ERROR);
         finishing = result == RECORD_AT_AUDIO_PROGRESS;
      }
      result = record_at_audio_pump(aac, &packet);
      assert(result != RECORD_AT_AUDIO_ERROR);
      if (result == RECORD_AT_AUDIO_PACKET)
      {
         assert(record_at_audio_timing(aac, &leading, &trailing, &accepted));
         assert(packet.timestamp == (int64_t)packets * 1024 - leading);
         write_packet(file, &packet, rate, channels);
         packets++;
         assert(record_at_audio_release(aac));
      }
      if (result == RECORD_AT_AUDIO_END)
         break;
   }
   assert(result == RECORD_AT_AUDIO_END && ended && closed);
   assert(record_at_resample_timing(src, &accepted, &produced));
   assert(accepted == SOURCE_FRAMES && produced == total);
   assert(record_at_audio_timing(aac, &leading, &trailing, &accepted));
   assert(accepted == total);
   assert(!fclose(file) && !fclose(pcm));
   assert(record_at_resample_free(src) && record_audio_queue_free(q));
   assert(record_at_audio_free(aac));
   printf("rate=%u channels=%u leading=%u trailing=%u frames=%lu packets=%u source=%u\n",
         rate, channels, leading, trailing, (unsigned long)total, packets, SOURCE_FRAMES);
   return 0;
}
