/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <formats/audio.h>
#include <formats/rmp4.h>
static void *load(const char *path, size_t *size)
{
   FILE *f = fopen(path, "rb");
   long length;
   void *p;
   assert(f && !fseek(f, 0, SEEK_END));
   length = ftell(f);
   assert(length > 0 && !fseek(f, 0, SEEK_SET));
   *size = (size_t)length;
   p = malloc(*size);
   assert(p && fread(p, 1, *size, f) == *size);
   fclose(f);
   return p;
}
int main(int argc, char **argv)
{
   size_t size, bytes, count = 0, got, i;
   unsigned channels, rate;
   uint8_t *data;
   float *reference, *pcm, seeked[400];
   rmp4_t *mp4;
   void *transfer;
   int track = -1, result;
   double signal = 0, noise = 0, expected;
   assert(argc == 4);
   data = (uint8_t*)load(argv[1], &size);
   reference = (float*)load(argv[2], &bytes);
   expected = atof(argv[3]);
   mp4 = rmp4_open_memory(data, size);
   assert(mp4 && fabs(rmp4_duration_ns(mp4) / 1e9 - expected) < .00001);
   for (i = 0; i < (size_t)rmp4_num_tracks(mp4); i++)
      if (rmp4_get_track(mp4, (int)i)->codec == RMP4_CODEC_AC3) track = (int)i;
   assert(track >= 0);
   rmp4_close(mp4);
   assert(audio_transfer_mp4_audio_track_type(data, size, size, track) == AUDIO_TYPE_AC3);
   transfer = audio_transfer_new(AUDIO_TYPE_AC3);
   assert(transfer);
   audio_transfer_set_buffer_ptr(transfer, AUDIO_TYPE_AC3, data, size);
   assert(audio_transfer_set_container_track(transfer, AUDIO_TYPE_AC3, track));
   assert(audio_transfer_start(transfer, AUDIO_TYPE_AC3));
   assert(audio_transfer_info(transfer, AUDIO_TYPE_AC3, &channels, &rate, NULL));
   assert(channels == 2 && rate == 48000);
   pcm = (float*)calloc(bytes / sizeof(float) + 8192, sizeof(float));
   assert(pcm);
   do
   {
      result = audio_transfer_read_f32(transfer, AUDIO_TYPE_AC3, pcm + count * 2, 733, &got);
      count += got;
      assert(count * 2 <= bytes / sizeof(float) + 8192);
   } while (result == AUDIO_PROCESS_NEXT && got);
   assert(count * 2 * sizeof(float) == bytes);
   for (i = 0; i < count * 2; i++)
   {
      double delta = pcm[i] - reference[i];
      signal += reference[i] * reference[i];
      noise += delta * delta;
   }
   assert(signal > .01 && (noise == 0 || 10 * log10(signal / noise) > 40));
   assert(audio_transfer_seek(transfer, AUDIO_TYPE_AC3, 1735));
   audio_transfer_read_f32(transfer, AUDIO_TYPE_AC3, seeked, 200, &got);
   assert(got == 200);
   for (i = 0; i < 400; i++) assert(fabs(seeked[i] - pcm[1735 * 2 + i]) < .0001);
   audio_transfer_free(transfer, AUDIO_TYPE_AC3);
   free(pcm); free(reference); free(data);
   puts("MP4 AC-3 playback, fragmented duration, PCM reference and seek PASS");
   return 0;
}
