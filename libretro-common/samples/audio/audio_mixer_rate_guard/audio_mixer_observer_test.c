#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_mixer.c"

static audio_mixer_voice_t *observed;
static unsigned callbacks, retains, releases;
static bool expect_s16, expect_stream;
static float output_f[64];
static int16_t output_i[64];

static void observer_retain(void *user) { assert(user == &callbacks); retains++; }
static void observer_release(void *user) { assert(user == &callbacks); releases++; }
static void observer_samples(void *user, const void *pcm, size_t frames, bool floating)
{
   const void *expected;
   unsigned position;
   assert(user == &callbacks && frames && floating != expect_s16);
   assert(pcm != output_f && pcm != output_i);
   if (expect_stream)
   {
      position = observed->types.stream.position;
      if (expect_s16)
         expected = (observed->types.stream.resampler_int16.data
               ? observed->types.stream.buffer_s16 : observed->types.stream.decode_buf_s16) + position;
      else
         expected = (observed->types.stream.resampler
               ? observed->types.stream.buffer : observed->types.stream.decode_buf) + position;
   }
   else
   {
      position = observed->types.wav.position;
      expected = expect_s16 ? (const void*)(observed->sound->types.wav.pcm_s16 + position)
         : (const void*)(observed->sound->types.wav.pcm + position);
   }
   assert(pcm == expected);
   callbacks++;
}

static uint8_t *observer_wav(void)
{
   uint8_t *data = (uint8_t*)calloc(1, 44 + 2048);
   unsigned i;
   assert(data);
   memcpy(data, "RIFF", 4); data[4] = 36; data[5] = 8;
   memcpy(data + 8, "WAVEfmt ", 8); data[16] = 16; data[20] = 1;
   data[22] = 2; data[24] = 0x80; data[25] = 0xbb;
   data[29] = 0xee; data[30] = 2; data[32] = 4; data[34] = 16;
   memcpy(data + 36, "data", 4); data[41] = 8;
   for (i = 0; i < 1024; i++)
   {
      unsigned value = (unsigned)(int16_t)((int)i * 23 - 12000);
      data[44 + i * 2] = (uint8_t)value;
      data[45 + i * 2] = (uint8_t)(value >> 8);
   }
   return data;
}

static void observer_mix(void)
{
   memset(output_f, 0, sizeof(output_f));
   memset(output_i, 0, sizeof(output_i));
   if (expect_s16) audio_mixer_mix_s16(output_i, 32, AUDIO_MIXER_GAIN_UNITY, false);
   else audio_mixer_mix(output_f, 32, 1.0f, false);
}

int main(void)
{
   unsigned s16, stream;
   for (s16 = 0; s16 < 2; s16++)
      for (stream = 0; stream < 2; stream++)
      {
         uint8_t *first = observer_wav(), *second = observer_wav();
         audio_mixer_sound_t *sound, *background;
         audio_mixer_voice_t *other;
         audio_mixer_observer_t observer;
         expect_s16 = s16 != 0;
         expect_stream = stream != 0;
         callbacks = retains = releases = 0;
         audio_mixer_init(48000);
         sound = stream ? audio_mixer_load_wav_stream(first, 2092)
            : audio_mixer_load_wav(first, 2092, "sinc", RESAMPLER_QUALITY_NORMAL, expect_s16);
         background = stream ? audio_mixer_load_wav_stream(second, 2092)
            : audio_mixer_load_wav(second, 2092, "sinc", RESAMPLER_QUALITY_NORMAL, expect_s16);
         assert(sound && background);
         memset(&observer, 0, sizeof(observer));
         observer.user = &callbacks;
         observer.samples = observer_samples;
         observer.retain = observer_retain;
         observer.release = observer_release;
         audio_mixer_sound_set_observer(sound, &observer);
         assert(retains == 1 && !releases);
         observed = s16 ? audio_mixer_play_s16(sound, false, AUDIO_MIXER_GAIN_UNITY / 4,
               "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
            : audio_mixer_play(sound, false, 0.25f, "sinc", RESAMPLER_QUALITY_NORMAL, NULL);
         other = s16 ? audio_mixer_play_s16(background, false, AUDIO_MIXER_GAIN_UNITY / 2,
               "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
            : audio_mixer_play(background, false, 0.5f, "sinc", RESAMPLER_QUALITY_NORMAL, NULL);
         assert(observed && other);
         observer_mix();
         assert(callbacks == 1);
         audio_mixer_voice_set_paused(observed, true);
         observer_mix();
         assert(callbacks == 1 && (s16 ? output_i[0] != 0 : output_f[0] != 0));
         audio_mixer_voice_set_paused(observed, false);
         observer_mix();
         assert(callbacks == 2);
         audio_mixer_stop(observed);
         observer_mix();
         assert(callbacks == 2 && !releases);
         audio_mixer_destroy(sound);
         assert(releases == 1);
         audio_mixer_stop(other);
         audio_mixer_destroy(background);
         if (!stream) { free(first); free(second); }
         audio_mixer_done();
      }
   puts("PASS: per-sound observers borrow native float/S16 WAV and streaming PCM; pause, background exclusion and lifetime");
   return 0;
}
