/* A transport pause must retain queued PCM and resampler history,
 * rather than muting and continuing to consume the compressed stream. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/audio_mixer.h>
#include <formats/audio.h>

#define FRAMES 512
#define BLOCK 32
static unsigned failures;
static size_t previous_serial;
static float baseline_f[BLOCK * 2 * 4], actual_f[BLOCK * 2 * 4];
static int16_t baseline_i[BLOCK * 2 * 4], actual_i[BLOCK * 2 * 4];
static unsigned char *wav(void)
{
   unsigned char *p = (unsigned char*)calloc(1, 44 + FRAMES * 4);
   unsigned i;
   if (!p) return NULL;
   memcpy(p, "RIFF", 4); p[4] = 36; p[5] = 8;
   memcpy(p + 8, "WAVEfmt ", 8); p[16] = 16; p[20] = 1;
   p[22] = 2; p[24] = 0x80; p[25] = 0xbb;
   p[28] = 0; p[29] = 0xee; p[30] = 2;
   p[32] = 4; p[34] = 16;
   memcpy(p + 36, "data", 4); p[41] = 8;
   for (i = 0; i < FRAMES * 2; i++)
   {
      unsigned v = (unsigned)(int16_t)((int)i * 23 - 12000);
      p[44 + i * 2] = (unsigned char)v;
      p[45 + i * 2] = (unsigned char)(v >> 8);
   }
   return p;
}

static void render(bool s16, bool stream, bool pause, unsigned rate)
{
   unsigned char *data = wav();
   audio_mixer_sound_t *sound;
   audio_mixer_voice_t *v;
   unsigned block, i;
   size_t serial = 0;
   static float silence_f[BLOCK * 2];
   static int16_t silence_i[BLOCK * 2];
   if (!data) { failures++; return; }
   audio_mixer_init(rate);
   sound = stream ? audio_mixer_load_wav_stream(data, 44 + FRAMES * 4)
      : audio_mixer_load_wav(data, 44 + FRAMES * 4, "sinc",
            RESAMPLER_QUALITY_NORMAL, s16);
   if (!sound) { free(data); failures++; audio_mixer_done(); return; }
   v = s16 ? audio_mixer_play_s16(sound, false, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
      : audio_mixer_play(sound, false, 1.0f, "sinc",
            RESAMPLER_QUALITY_NORMAL, NULL);
   if (!v) failures++;
   if (v)
   {
      size_t frames;
      unsigned hz;
      if (!audio_mixer_voice_clock(v, &frames, &hz, &serial)
            || frames || hz != rate || !serial || serial == previous_serial)
         failures++;
      previous_serial = serial;
   }
   for (block = 0; block < 4 && v; block++)
   {
      if (pause && (block == 0 || block == 2))
      {
         size_t before = audio_mixer_voice_buffer_tell(v);
         size_t clock_before, clock_after;
         unsigned hz;
         if (!audio_mixer_voice_clock(v, &clock_before, &hz, NULL)
               || hz != rate) failures++;
         audio_mixer_voice_set_paused(v, true);
         audio_mixer_voice_set_paused(v, true);
         for (i = 0; i < 8; i++)
         {
            unsigned j;
            memset(silence_f, 0, sizeof(silence_f));
            memset(silence_i, 0, sizeof(silence_i));
            if (s16) audio_mixer_mix_s16(silence_i, BLOCK,
                  AUDIO_MIXER_GAIN_UNITY, false);
            else audio_mixer_mix(silence_f, BLOCK, 1.0f, false);
            for (j = 0; j < BLOCK * 2; j++)
               if (silence_i[j] || silence_f[j] != 0.0f) failures++;
         }
         if (before != audio_mixer_voice_buffer_tell(v)) failures++;
         if (!audio_mixer_voice_clock(v, &clock_after, &hz, NULL)
               || clock_after != clock_before) failures++;
         audio_mixer_voice_set_paused(v, false);
         audio_mixer_voice_set_paused(v, false);
      }
      if (s16) audio_mixer_mix_s16((pause ? actual_i : baseline_i)
            + block * BLOCK * 2, BLOCK, AUDIO_MIXER_GAIN_UNITY, false);
      else audio_mixer_mix((pause ? actual_f : baseline_f)
            + block * BLOCK * 2, BLOCK, 1.0f, false);
      {
         size_t frames = 0;
         size_t generation = 0;
         unsigned hz = 0;
         if (!audio_mixer_voice_clock(v, &frames, &hz, &generation)
               || frames != (block + 1) * BLOCK || hz != rate
               || generation != serial)
         {
            printf("clock block: s16=%u stream=%u pause=%u rate=%u block=%u frames=%lu hz=%u\n",
                  s16, stream, pause, rate, block, (unsigned long)frames, hz);
            failures++;
         }
      }
   }
   if (v) audio_mixer_stop(v);
   audio_mixer_destroy(sound);
   if (!stream) free(data);
   audio_mixer_done();
}

static void prepared_voice(bool s16, bool cancel)
{
   unsigned char *data = wav();
   audio_mixer_sound_t *sound = NULL;
   audio_mixer_voice_t *voice = NULL;
   void *transfer = NULL;
   size_t got = 0, frames = 0;
   unsigned i, before = failures;
   int result;
   if (!data) { failures++; return; }
   audio_mixer_init(48000);
   sound = audio_mixer_load_wav_stream(data, 44 + FRAMES * 4);
   if (!sound) { free(data); failures++; goto done; }
   transfer = audio_transfer_new(AUDIO_TYPE_WAV);
   if (!transfer) { failures++; goto done; }
   audio_transfer_set_buffer_ptr(transfer, AUDIO_TYPE_WAV,
         data, 44 + FRAMES * 4);
   if (!audio_transfer_start(transfer, AUDIO_TYPE_WAV))
   { failures++; goto done; }
   result = s16 ? audio_transfer_read_s16(transfer, AUDIO_TYPE_WAV,
         actual_i, BLOCK * 3, &got)
      : audio_transfer_read_f32(transfer, AUDIO_TYPE_WAV,
            actual_f, BLOCK * 3, &got);
   if (result < 0 || got != BLOCK * 3)
   { failures++; goto done; }
   if (!audio_mixer_sound_set_prepared_transfer(sound, transfer,
            cancel ? 44100 : 48000, s16))
   { failures++; goto done; }
   transfer = NULL;
   if (cancel)
   {
      voice = s16 ? audio_mixer_play_s16(sound, false, AUDIO_MIXER_GAIN_UNITY,
            "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
         : audio_mixer_play(sound, false, 1.0f, "sinc",
               RESAMPLER_QUALITY_NORMAL, NULL);
      if (voice) failures++;
      goto done;
   }
   /* A failed format match must not consume the prepared position. */
   voice = !s16 ? audio_mixer_play_s16(sound, false, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
      : audio_mixer_play(sound, false, 1.0f, "sinc",
            RESAMPLER_QUALITY_NORMAL, NULL);
   if (voice) { failures++; goto done; }
   audio_mixer_sound_set_initial_paused(sound, true);
   voice = s16 ? audio_mixer_play_s16(sound, false, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
      : audio_mixer_play(sound, false, 1.0f, "sinc",
            RESAMPLER_QUALITY_NORMAL, NULL);
   if (!voice) { failures++; goto done; }
   memset(actual_i, 0, sizeof(actual_i));
   memset(actual_f, 0, sizeof(actual_f));
   if (s16) audio_mixer_mix_s16(actual_i, BLOCK, AUDIO_MIXER_GAIN_UNITY, false);
   else audio_mixer_mix(actual_f, BLOCK, 1.0f, false);
   for (i = 0; i < BLOCK * 2; i++)
      if (actual_i[i] || actual_f[i] != 0.0f) failures++;
   if (!audio_mixer_voice_clock(voice, &frames, NULL, NULL) || frames)
      failures++;
   audio_mixer_voice_set_paused(voice, false);
   if (s16) audio_mixer_mix_s16(actual_i, BLOCK, AUDIO_MIXER_GAIN_UNITY, false);
   else audio_mixer_mix(actual_f, BLOCK, 1.0f, false);
   if (s16 ? memcmp(actual_i, baseline_i + BLOCK * 2 * 3,
            BLOCK * 2 * sizeof(*actual_i)) != 0
         : memcmp(actual_f, baseline_f + BLOCK * 2 * 3,
               BLOCK * 2 * sizeof(*actual_f)) != 0)
      failures++;
   if (!audio_mixer_voice_clock(voice, &frames, NULL, NULL) || frames != BLOCK)
      failures++;
done:
   if (voice) audio_mixer_stop(voice);
   if (transfer) audio_transfer_free(transfer, AUDIO_TYPE_WAV);
   audio_mixer_destroy(sound);
   audio_mixer_done();
   if (failures != before)
      printf("prepared voice: s16=%u cancel=%u failed\n", s16, cancel);
}

static unsigned repeat_ends, repeat_laps;
static void repeat_stopped(audio_mixer_sound_t *sound, unsigned reason)
{
   (void)sound;
   if (reason == AUDIO_MIXER_SOUND_FINISHED) repeat_ends++;
   if (reason == AUDIO_MIXER_SOUND_REPEATED) repeat_laps++;
}

static void repeat_policy(bool s16, bool stream, bool enable)
{
   unsigned char *data = wav();
   audio_mixer_sound_t *sound;
   audio_mixer_voice_t *voice;
   static float f[BLOCK * 2];
   static int16_t pcm[BLOCK * 2];
   unsigned i;
   if (!data) { failures++; return; }
   audio_mixer_init(48000);
   sound = stream ? audio_mixer_load_wav_stream(data, 44 + FRAMES * 4)
      : audio_mixer_load_wav(data, 44 + FRAMES * 4, "sinc",
            RESAMPLER_QUALITY_NORMAL, s16);
   if (!sound) { free(data); failures++; audio_mixer_done(); return; }
   repeat_ends = repeat_laps = 0;
   voice = s16 ? audio_mixer_play_s16(sound, !enable, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_NORMAL, repeat_stopped)
      : audio_mixer_play(sound, !enable, 1.0f, "sinc",
            RESAMPLER_QUALITY_NORMAL, repeat_stopped);
   if (!voice) failures++;
   for (i = 0; voice && i < 96; i++)
   {
      if (i == 1) audio_mixer_voice_set_repeat(voice, enable);
      if (i == 48 && enable) audio_mixer_voice_set_repeat(voice, false);
      memset(f, 0, sizeof(f));
      memset(pcm, 0, sizeof(pcm));
      if (s16) audio_mixer_mix_s16(pcm, BLOCK, AUDIO_MIXER_GAIN_UNITY, false);
      else audio_mixer_mix(f, BLOCK, 1.0f, false);
      if (repeat_ends) break;
   }
   if (repeat_ends != 1 || (enable ? !repeat_laps : repeat_laps != 0))
   {
      printf("repeat policy failed: s16=%u stream=%u enable=%u ends=%u laps=%u\n",
            s16, stream, enable, repeat_ends, repeat_laps);
      failures++;
   }
   if (voice) audio_mixer_stop(voice);
   audio_mixer_destroy(sound);
   if (!stream) free(data);
   audio_mixer_done();
}

int main(void)
{
   unsigned s16, stream, rate;
   audio_mixer_voice_set_paused(NULL, true);
   audio_mixer_voice_set_repeat(NULL, false);
   for (s16 = 0; s16 < 2; s16++)
      for (stream = 0; stream < 2; stream++)
      {
         repeat_policy(s16 != 0, stream != 0, false);
         repeat_policy(s16 != 0, stream != 0, true);
      }
   for (s16 = 0; s16 < 2; s16++)
      for (stream = 0; stream < 2; stream++)
      for (rate = 44100; rate <= 48000; rate += 3900)
      {
         unsigned i, active = 0;
         memset(baseline_f, 0, sizeof(baseline_f));
         memset(actual_f, 0, sizeof(actual_f));
         memset(baseline_i, 0, sizeof(baseline_i));
         memset(actual_i, 0, sizeof(actual_i));
         render(s16 != 0, stream != 0, false, rate);
         render(s16 != 0, stream != 0, true, rate);
         if (memcmp(baseline_f, actual_f, sizeof(actual_f))
               || memcmp(baseline_i, actual_i, sizeof(actual_i)))
         {
            printf("PCM mismatch: s16=%u stream=%u rate=%u\n",s16,stream,rate);
            failures++;
         }
         for (i = 0; i < BLOCK * 2 * 4; i++)
            if (s16 ? baseline_i[i] != 0 : baseline_f[i] != 0.0f) active++;
         if (!active) failures++;
      }
   for (s16 = 0; s16 < 2; s16++)
   {
      memset(baseline_i, 0, sizeof(baseline_i));
      memset(baseline_f, 0, sizeof(baseline_f));
      render(s16 != 0, true, false, 48000);
      prepared_voice(s16 != 0, false);
      prepared_voice(s16 != 0, true);
   }
   printf("audio_mixer pause: %u failures\n", failures);
   return failures != 0;
}
