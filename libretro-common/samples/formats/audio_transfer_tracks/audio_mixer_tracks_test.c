/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/audio_mixer.h>
#include <formats/audio.h>
static float output[512];
static int16_t output_s16[512];
static unsigned released;
static void source_release(void *source)
{
   assert(source);
   released++;
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
static void mix(bool s16)
{
   if (s16)
   {
      unsigned i;
      memset(output_s16, 0, sizeof(output_s16));
      audio_mixer_mix_s16(output_s16, 256, AUDIO_MIXER_GAIN_UNITY, false);
      for (i = 0; i < 512; i++)
         output[i] = output_s16[i] * (1.0f / 32768.0f);
   }
   else
   {
      memset(output, 0, sizeof(output));
      audio_mixer_mix(output, 256, 1.0f, false);
   }
}
static void tone(bool s16, unsigned wanted)
{
   double real[2] = {0, 0}, imag[2] = {0, 0};
   unsigned block, i, j;
   for (block = 0; block < 32; block++)
   {
      mix(s16);
      for (i = 0; i < 256; i++)
         for (j = 0; j < 2; j++)
         {
            double phase = 6.283185307179586 * (j ? 880 : 440)
               * (block * 256 + i) / 48000;
            real[j] += output[2 * i] * cos(phase);
            imag[j] += output[2 * i] * sin(phase);
         }
   }
   for (j = 0; j < 2; j++)
   {
      double amplitude = 2 * sqrt(real[j] * real[j] + imag[j] * imag[j]) / 8192;
      if (!(j == wanted ? amplitude > .05 : amplitude < .02))
         fprintf(stderr, "tone %u wanted %u amplitude=%f s16=%u\n",
               j, wanted, amplitude, (unsigned)s16);
      assert(j == wanted ? amplitude > .05 : amplitude < .02);
   }
}
static audio_mixer_voice_t *play(audio_mixer_sound_t *sound, bool s16)
{
   return s16 ? audio_mixer_play_s16(sound, true, AUDIO_MIXER_GAIN_UNITY,
         NULL, RESAMPLER_QUALITY_DONTCARE, NULL)
      : audio_mixer_play(sound, true, 1.0f, NULL, RESAMPLER_QUALITY_DONTCARE, NULL);
}
static void check_sound(audio_mixer_sound_t *sound, void *data,
      bool s16, unsigned wanted)
{
   audio_mixer_voice_t *voice;
   unsigned i, old_released = released;
   assert(sound);
   audio_mixer_sound_set_data_owner(sound, data, source_release);
   voice = play(sound, s16);
   assert(voice);
   for (i = 0; i < 8; i++) mix(s16);
   tone(s16, wanted);
   /* Repeat must preserve the selected track, in both PCM pipelines. */
   for (i = 0; i < 180; i++) mix(s16);
   tone(s16, wanted);
   audio_mixer_stop(voice);
   /* A second play opens the same immutable choice after a one-shot
    * prepared decoder has already been consumed and retired. */
   voice = play(sound, s16);
   assert(voice);
   for (i = 0; i < 8; i++) mix(s16);
   tone(s16, wanted);
   audio_mixer_stop(voice);
   audio_mixer_destroy(sound);
   assert(released == old_released + 1);
}
static void fixture(const char *path, bool mp4)
{
   size_t length;
   uint8_t *data = load(path, &length);
   unsigned s16;
   for (s16 = 0; s16 < 2; s16++)
   {
      int track;
      for (track = -1; track <= 2; track++)
      {
         audio_mixer_sound_t *sound;
         if (!track) continue;
         if (track < 0)
            sound = mp4 ? audio_mixer_load_m4a(data, length)
               : audio_mixer_load_weba_avail(data, length, 0);
         else
            sound = mp4 ? audio_mixer_load_m4a_track(data, length, track)
               : audio_mixer_load_weba_track(data, length, 0, track);
         check_sound(sound, data, s16 != 0, track == 2 ? 1 : 0);
      }
   }
   if (mp4)
   {
      audio_mixer_sound_t *bad = audio_mixer_load_m4a_track(data, length, 0);
      unsigned before = released;
      assert(bad);
      audio_mixer_sound_set_data_owner(bad, data, source_release);
      assert(!play(bad, false) && !play(bad, true));
      audio_mixer_destroy(bad);
      assert(released == before + 1);
      assert(!audio_mixer_load_m4a_track(data, length, -2));
   }
   else
   {
      assert(!audio_mixer_load_weba_track(data, length, 3, 2));
      assert(!audio_mixer_load_weba_track(data, length, 0, 0));
      assert(!audio_mixer_load_weba_track(data, length, 0, 99));
      assert(!audio_mixer_load_weba_track(data, length, 0, -2));
   }
   free(data);
   printf("PASS mixer selected tracks, float/S16, repeat and source retirement: %s\n", path);
}
static void mixed(void)
{
   size_t length;
   uint8_t *data = load("fixtures/mixed.bin", &length);
   unsigned track, s16;
   for (s16 = 0; s16 < 2; s16++)
      for (track = 1; track <= 4; track++)
         check_sound(audio_mixer_load_weba_track(data, length, 0, (int)track),
               data, s16 != 0, (track - 1) & 1);
   free(data);
   puts("PASS mixed-codec mixer track dispatch, float and S16");
}
static void prepared(void)
{
   size_t length;
   uint8_t *data = load("fixtures/aac-mp4.bin", &length);
   unsigned s16;
   for (s16 = 0; s16 < 2; s16++)
   {
      audio_mixer_sound_t *sound = audio_mixer_load_m4a_track(data, length, 2);
      void *decoder = audio_transfer_new(AUDIO_TYPE_AAC);
      assert(sound && decoder);
      audio_transfer_set_buffer_ptr(decoder, AUDIO_TYPE_AAC, data, length);
      assert(audio_transfer_start(decoder, AUDIO_TYPE_AAC));
      assert(!audio_mixer_sound_set_prepared_transfer(sound, decoder, 48000, s16 != 0));
      audio_transfer_free(decoder, AUDIO_TYPE_AAC);
      decoder = audio_transfer_new(AUDIO_TYPE_AAC);
      assert(decoder);
      audio_transfer_set_buffer_ptr(decoder, AUDIO_TYPE_AAC, data, length);
      assert(audio_transfer_set_container_track(decoder, AUDIO_TYPE_AAC, 2));
      assert(audio_transfer_start(decoder, AUDIO_TYPE_AAC));
      assert(audio_mixer_sound_set_prepared_transfer(sound, decoder, 48000, s16 != 0));
      /* A format mismatch must retain the prepared decoder for the right play. */
      assert(!play(sound, s16 == 0));
      check_sound(sound, data, s16 != 0, 1);
   }
   free(data);
   puts("PASS prepared transfer track/format admission and one-time adoption");
}
int main(void)
{
   audio_mixer_init(48000);
   fixture("fixtures/aac-mp4.bin", true);
   fixture("fixtures/aac-webm.bin", false);
   fixture("fixtures/opus.bin", false);
   fixture("fixtures/vorbis.bin", false);
   fixture("fixtures/flac.bin", false);
   mixed();
   prepared();
   audio_mixer_done();
   return 0;
}
