/* SPDX-License-Identifier: MIT */
/* Synthetic tracks have distinct independently encoded 440/880 Hz tones. */
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <formats/audio.h>
#include <formats/rwebm.h>
static float pcm[512];
static size_t tone(void *decoder, enum audio_type_enum type, unsigned wanted)
{
   double real[2] = {0, 0}, imag[2] = {0, 0};
   unsigned rate, channels, step, i;
   size_t count, frames = 0, measured = 0;
   bool ended = false;
   assert(audio_transfer_info(decoder, type, &channels, &rate, NULL));
   assert(channels == 1 && rate == 48000);
   for (step = 0; step < 256; step++)
   {
      int result = audio_transfer_read_f32(decoder, type, pcm, 512, &count);
      assert(result != AUDIO_PROCESS_ERROR && count <= 512);
      for (i = 0; i < count; i++, frames++)
      {
         unsigned j;
         if (frames < 2048)
            continue;
         for (j = 0; j < 2; j++)
         {
            double phase = 6.283185307179586 * (j ? 880 : 440) * frames / rate;
            real[j] += pcm[i] * cos(phase);
            imag[j] += pcm[i] * sin(phase);
         }
         measured++;
      }
      if (result == AUDIO_PROCESS_END)
      {
         ended = true;
         break;
      }
   }
   if (!ended || measured <= 10000)
      fprintf(stderr, "tone frames=%lu measured=%lu ended=%u steps=%u\n",
            (unsigned long)frames, (unsigned long)measured, (unsigned)ended, step);
   if (!frames)
      fprintf(stderr, "decoder frontier=%lu need=%lu track=%d\n",
            (unsigned long)audio_transfer_buffer_tell(decoder, type),
            (unsigned long)audio_transfer_buffer_read_end(decoder, type),
            audio_transfer_container_track(decoder, type));
   assert(ended && measured > 10000);
   for (i = 0; i < 2; i++)
   {
      double amplitude = 2 * sqrt(real[i] * real[i] + imag[i] * imag[i]) / measured;
      assert(i == wanted ? amplitude > .08 : amplitude < .02);
   }
   return frames;
}
static uint8_t *load(const char *path, size_t *size)
{
   FILE *file = fopen(path, "rb");
   long length;
   uint8_t *data;
   assert(file && !fseek(file, 0, SEEK_END));
   length = ftell(file);
   assert(length > 0 && !fseek(file, 0, SEEK_SET));
   data = (uint8_t*)malloc((size_t)length);
   assert(data && fread(data, 1, (size_t)length, file) == (size_t)length);
   assert(!fclose(file));
   *size = (size_t)length;
   return data;
}
static void fixture(const char *path, enum audio_type_enum type)
{
   size_t length;
   uint8_t *data = load(path, &length);
   int choice;
   for (choice = -1; choice <= 2; choice++)
   {
      void *decoder = audio_transfer_new(type);
      assert(decoder && audio_transfer_container_track(decoder, type) == -1);
      audio_transfer_set_buffer_ptr(decoder, type, data, (size_t)length);
      audio_transfer_set_avail(decoder, type, 3);
      assert(!audio_transfer_set_container_track(decoder, type, choice < 0 ? 1 : choice));
      audio_transfer_set_avail(decoder, type, (size_t)length);
      assert(audio_transfer_set_container_track(decoder, type, choice));
      assert(!audio_transfer_set_container_track(decoder, type, -2));
      if (!choice)
         assert(!audio_transfer_start(decoder, type)); /* video is track 0 */
      else
      {
         size_t before, after;
         uint64_t target = type == AUDIO_TYPE_FLAC ? 2400 : 0;
         assert(audio_transfer_start(decoder, type));
         assert(audio_transfer_container_track(decoder, type) == (choice < 0 ? 1 : choice));
         assert(!audio_transfer_set_container_track(decoder, type, -1));
         before = tone(decoder, type, choice == 2 ? 1 : 0);
         if (type == AUDIO_TYPE_FLAC)
         {
            unsigned steps;
            int result = 0;
            assert(audio_transfer_seek_begin(decoder, type, target));
            for (steps = 0; steps < 256 && !result; steps++)
            {
               audio_transfer_set_avail(decoder, type, length);
               result = audio_transfer_seek_step(decoder, type);
            }
            assert(result == 1 && steps < 256);
         }
         else assert(audio_transfer_seek(decoder, type, target));
         /* A windowed seek withdraws the old resident media bound. */
         audio_transfer_set_avail(decoder, type, (size_t)length);
         assert(audio_transfer_container_track(decoder, type) == (choice < 0 ? 1 : choice));
         after = tone(decoder, type, choice == 2 ? 1 : 0);
         if (type == AUDIO_TYPE_FLAC)
            assert(after + (size_t)target == before);
      }
      audio_transfer_free(decoder, type);
   }
   for (choice = 0; choice < 2; choice++)
   {
      void *decoder = audio_transfer_new(type);
      assert(decoder);
      audio_transfer_set_buffer_ptr(decoder, type, data, (size_t)length);
      assert(audio_transfer_set_container_track(decoder, type, choice ? INT_MAX : 99));
      assert(!audio_transfer_start(decoder, type));
      assert(audio_transfer_container_track(decoder, type) == -1);
      audio_transfer_free(decoder, type);
   }
   free(data);
   printf("PASS selected container audio, tones, seek and invalid choices: %s\n", path);
}
static void flac_bad_packet(void)
{
   size_t length, got = 0;
   uint8_t *data = load("fixtures/flac.bin", &length);
   void *decoder = audio_transfer_new(AUDIO_TYPE_FLAC);
   rwebm_t *demux = rwebm_open_memory(data, length);
   rwebm_packet packet;
   assert(decoder && demux && rwebm_read_packet_track(demux, 2, &packet) == 1);
   assert(packet.size && packet.data >= data && packet.data < data + length);
   data[packet.data - data] ^= 1;
   rwebm_close(demux);
   audio_transfer_set_buffer_ptr(decoder, AUDIO_TYPE_FLAC, data, length);
   assert(audio_transfer_set_container_track(decoder, AUDIO_TYPE_FLAC, 2));
   assert(audio_transfer_start(decoder, AUDIO_TYPE_FLAC));
   assert(audio_transfer_read_f32(decoder, AUDIO_TYPE_FLAC, pcm, 512, &got)
         == AUDIO_PROCESS_ERROR && !got);
   audio_transfer_free(decoder, AUDIO_TYPE_FLAC);
   free(data);
   puts("PASS malformed FLAC packet is an error, not movie-tail silence");
}
static void flac_bounded_seek(void)
{
   size_t length, full, remaining, wall;
   rwebm_t *demux;
   rwebm_packet packet;
   uint8_t *data = load("fixtures/flac.bin", &length);
   void *decoder = audio_transfer_new(AUDIO_TYPE_FLAC);
   unsigned steps;
   int result = 0;
   assert(decoder);
   demux = rwebm_open_memory(data, length);
   assert(demux && rwebm_read_packet_track(demux, 2, &packet) == 1
         && packet.track == 2 && packet.size > 1);
   wall = (size_t)(packet.data - data) + packet.size - 1;
   rwebm_close(demux);
   audio_transfer_set_buffer_ptr(decoder, AUDIO_TYPE_FLAC, data, length);
   assert(audio_transfer_set_container_track(decoder, AUDIO_TYPE_FLAC, 2));
   assert(audio_transfer_start(decoder, AUDIO_TYPE_FLAC));
   full = tone(decoder, AUDIO_TYPE_FLAC, 1);
   assert(audio_transfer_seek_begin(decoder, AUDIO_TYPE_FLAC, 7000));
   assert(audio_transfer_seek_begin_format(decoder, AUDIO_TYPE_FLAC, 5000, true));
   assert(!audio_transfer_seek_begin(decoder, AUDIO_TYPE_FLAC, (uint64_t)-1));
   audio_transfer_set_avail(decoder, AUDIO_TYPE_FLAC, wall);
   for (steps = 0; steps < 8; steps++)
      assert(!audio_transfer_seek_step(decoder, AUDIO_TYPE_FLAC));
   assert(audio_transfer_buffer_tell(decoder, AUDIO_TYPE_FLAC) > 0
         && audio_transfer_buffer_read_end(decoder, AUDIO_TYPE_FLAC)
            > audio_transfer_buffer_tell(decoder, AUDIO_TYPE_FLAC));
   audio_transfer_set_avail(decoder, AUDIO_TYPE_FLAC, length);
   for (steps = 0; steps < 256 && !result; steps++)
      result = audio_transfer_seek_step(decoder, AUDIO_TYPE_FLAC);
   assert(result == 1 && steps > 1 && steps < 256);
   assert(audio_transfer_seek_step(decoder, AUDIO_TYPE_FLAC) == -1);
   remaining = tone(decoder, AUDIO_TYPE_FLAC, 1);
   assert(remaining + 5000 == full);
   assert(audio_transfer_seek_begin(decoder, AUDIO_TYPE_FLAC, 0));
   result = 0;
   for (steps = 0; steps < 256 && !result; steps++)
   {
      audio_transfer_set_avail(decoder, AUDIO_TYPE_FLAC, length);
      result = audio_transfer_seek_step(decoder, AUDIO_TYPE_FLAC);
   }
   assert(result == 1 && steps < 256);
   audio_transfer_set_avail(decoder, AUDIO_TYPE_FLAC, length);
   assert(tone(decoder, AUDIO_TYPE_FLAC, 1) == full);
   assert(!audio_transfer_seek_begin(decoder, AUDIO_TYPE_FLAC, full + 1));
   assert(audio_transfer_seek_begin(decoder, AUDIO_TYPE_FLAC, full));
   audio_transfer_set_avail(decoder, AUDIO_TYPE_FLAC, length);
   result = 0;
   for (steps = 0; steps < 256 && !result; steps++)
      result = audio_transfer_seek_step(decoder, AUDIO_TYPE_FLAC);
   assert(result == 1 && steps < 256);
   assert(audio_transfer_read_f32(decoder, AUDIO_TYPE_FLAC, pcm, 512, &remaining)
         == AUDIO_PROCESS_END && !remaining);
   audio_transfer_free(decoder, AUDIO_TYPE_FLAC);
   free(data);
   puts("PASS bounded FLAC seek, replacement, resident wall, rejection, EOF and rewind");
}
static void mixed(void)
{
   static const enum audio_type_enum types[] = {
      AUDIO_TYPE_OPUS, AUDIO_TYPE_VORBIS, AUDIO_TYPE_AAC, AUDIO_TYPE_FLAC
   };
   size_t length;
   uint8_t *data = load("fixtures/mixed.bin", &length);
   unsigned i;
   assert(audio_transfer_webm_audio_type(data, length) == AUDIO_TYPE_OPUS);
   assert(audio_transfer_webm_audio_track_type(data, length, 0) == AUDIO_TYPE_NONE);
   assert(audio_transfer_webm_audio_track_type(data, length, INT_MAX) == AUDIO_TYPE_NONE);
   for (i = 0; i < 4; i++)
   {
      int choice;
      assert(audio_transfer_webm_audio_track_type(data, length, (int)i + 1) == types[i]);
      for (choice = -1; choice <= 4; choice++)
      {
         void *decoder = audio_transfer_new(types[i]);
         assert(decoder);
         audio_transfer_set_buffer_ptr(decoder, types[i], data, length);
         assert(audio_transfer_set_container_track(decoder, types[i], choice));
         if (choice < 0 || choice == (int)i + 1)
         {
            assert(audio_transfer_start(decoder, types[i]));
            assert(audio_transfer_container_track(decoder, types[i]) == (int)i + 1);
            tone(decoder, types[i], i & 1);
         }
         else
            assert(!audio_transfer_start(decoder, types[i]));
         audio_transfer_free(decoder, types[i]);
      }
   }
   free(data);
   puts("PASS mixed-codec track sniff and explicit admission without fallback");
}
int main(void)
{
   assert(!audio_transfer_set_container_track(NULL, AUDIO_TYPE_AAC, 0));
   assert(audio_transfer_container_track(NULL, AUDIO_TYPE_AAC) == -1);
   fixture("fixtures/aac-mp4.bin", AUDIO_TYPE_AAC);
   fixture("fixtures/aac-webm.bin", AUDIO_TYPE_AAC);
   fixture("fixtures/opus.bin", AUDIO_TYPE_OPUS);
   fixture("fixtures/vorbis.bin", AUDIO_TYPE_VORBIS);
   fixture("fixtures/flac.bin", AUDIO_TYPE_FLAC);
   flac_bad_packet();
   flac_bounded_seek();
   mixed();
   return 0;
}
