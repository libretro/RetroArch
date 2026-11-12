/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../../record/record_at_audio.h"
#define SOURCE_FRAMES 32769u

static void write_packet(FILE *file, const record_at_audio_packet_t *packet,
      unsigned rate, unsigned channels)
{
   uint8_t header[7];
   size_t bytes = packet->size + sizeof(header);
   unsigned frequency = rate == 44100 ? 4 : 3;
   assert(bytes < 8192 && packet->frames == 1024);
   header[0] = 255;
   header[1] = 241;
   header[2] = (uint8_t)(64 | (frequency << 2) | (channels >> 2));
   header[3] = (uint8_t)((channels << 6) | (bytes >> 11));
   header[4] = (uint8_t)(bytes >> 3);
   header[5] = (uint8_t)((bytes << 5) | 31);
   header[6] = 252;
   assert(fwrite(header, 1, sizeof(header), file) == sizeof(header));
   assert(fwrite(packet->data, 1, packet->size, file) == packet->size);
}

int main(int argc, char **argv)
{
   record_at_audio_t *e;
   record_at_audio_packet_t packet, again;
   const void *asc;
   FILE *file;
   int16_t *input;
   size_t frames, size, i;
   uint64_t total;
   unsigned rate, channels, offset = 0, turns, packets = 0, leading, trailing;
   int result = RECORD_AT_AUDIO_BUSY;
   bool finishing = false;
   assert(argc == 4);
   rate = (unsigned)strtoul(argv[1], NULL, 10);
   channels = (unsigned)strtoul(argv[2], NULL, 10);
   e = record_at_audio_new(rate, channels, channels == 1 ? 96000 : 128000);
   assert(e && record_at_audio_config(e, &asc, &size) && size == 2);
   file = fopen(argv[3], "wb");
   assert(file);
   for (turns = 0; turns < 20000; turns++)
   {
      if (offset < SOURCE_FRAMES)
      {
         result = record_at_audio_input(e, &input, &frames);
         assert(result != RECORD_AT_AUDIO_ERROR);
         if (result == RECORD_AT_AUDIO_PROGRESS)
         {
            if (frames > SOURCE_FRAMES - offset)
               frames = SOURCE_FRAMES - offset;
            for (i = 0; i < frames * channels; i++)
            {
               unsigned channel = (unsigned)(i % channels);
               double phase = (offset + i / channels) * 6.283185307179586
                  * (channel ? 1499.0 : 997.0) / rate;
               input[i] = (int16_t)(sin(phase) * 12000.0);
            }
            assert(record_at_audio_submit(e, frames, offset + 123) == RECORD_AT_AUDIO_PROGRESS);
            offset += (unsigned)frames;
         }
      }
      if (offset == SOURCE_FRAMES && !finishing)
      {
         result = record_at_audio_finish(e);
         assert(result != RECORD_AT_AUDIO_ERROR);
         finishing = result == RECORD_AT_AUDIO_PROGRESS;
      }
      result = record_at_audio_pump(e, &packet);
      if (result == RECORD_AT_AUDIO_ERROR)
         fprintf(stderr, "AudioToolbox error %ld\n", (long)record_at_audio_error(e));
      assert(result != RECORD_AT_AUDIO_ERROR);
      if (result == RECORD_AT_AUDIO_PACKET)
      {
         assert(record_at_audio_timing(e, &leading, &trailing, &total));
         assert(packet.timestamp == (int64_t)123 + (int64_t)packets * 1024 - leading);
         assert(record_at_audio_pump(e, &again) == RECORD_AT_AUDIO_PACKET);
         assert(packet.data == again.data && packet.size == again.size);
         write_packet(file, &packet, rate, channels);
         packets++;
         assert(record_at_audio_release(e));
      }
      if (result == RECORD_AT_AUDIO_END)
         break;
   }
   assert(result == RECORD_AT_AUDIO_END && packets && offset == SOURCE_FRAMES);
   assert(record_at_audio_timing(e, &leading, &trailing, &total));
   assert(total == SOURCE_FRAMES && !fclose(file) && record_at_audio_free(e));
   printf("rate=%u channels=%u leading=%u trailing=%u frames=%u packets=%u\n",
         rate, channels, leading, trailing, SOURCE_FRAMES, packets);
   return 0;
}
