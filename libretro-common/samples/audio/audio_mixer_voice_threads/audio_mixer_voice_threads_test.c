/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mixer_voice_threads_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* A voice's volume and gain, set by a control thread and read by a
 * second while the audio thread mixes the voice. Built under
 * ThreadSanitizer: a field two threads touch without ordering is a
 * reported race. Every read must give back one of the values set, and
 * every mixed block must come out at one of the volumes set - silence
 * or the full triangle - never anything a torn value would give. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>

#include <audio/audio_mixer.h>
#include <audio/audio_resampler.h>
#include <retro_atomic.h>

#define MIX_RATE 48000
#define BLOCK    256
#define ROUNDS   4000

static unsigned char *make_wav(unsigned frames, unsigned rate,
      size_t *size_out)
{
   unsigned char *w;
   size_t         data_bytes = (size_t)frames * 2;
   size_t         total      = 44 + data_bytes;
   unsigned       i;
   unsigned long  byte_rate  = (unsigned long)rate * 2;

   if (!(w = (unsigned char*)calloc(1, total)))
      return NULL;

   memcpy(w,      "RIFF", 4);
   w[4]  = (unsigned char)((total - 8)       & 0xff);
   w[5]  = (unsigned char)(((total - 8) >> 8)  & 0xff);
   w[6]  = (unsigned char)(((total - 8) >> 16) & 0xff);
   w[7]  = (unsigned char)(((total - 8) >> 24) & 0xff);
   memcpy(w + 8,  "WAVEfmt ", 8);
   w[16] = 16;                            /* fmt chunk size      */
   w[20] = 1;                             /* PCM                 */
   w[22] = 1;                             /* channels            */
   w[24] = (unsigned char)( rate        & 0xff);
   w[25] = (unsigned char)((rate >> 8)  & 0xff);
   w[26] = (unsigned char)((rate >> 16) & 0xff);
   w[27] = (unsigned char)((rate >> 24) & 0xff);
   w[28] = (unsigned char)( byte_rate        & 0xff);
   w[29] = (unsigned char)((byte_rate >> 8)  & 0xff);
   w[30] = (unsigned char)((byte_rate >> 16) & 0xff);
   w[31] = (unsigned char)((byte_rate >> 24) & 0xff);
   w[32] = 2;                             /* block align         */
   w[34] = 16;                            /* bits per sample     */
   memcpy(w + 36, "data", 4);
   w[40] = (unsigned char)( data_bytes        & 0xff);
   w[41] = (unsigned char)((data_bytes >> 8)  & 0xff);
   w[42] = (unsigned char)((data_bytes >> 16) & 0xff);
   w[43] = (unsigned char)((data_bytes >> 24) & 0xff);

   for (i = 0; i < frames; i++)
   {
      /* Never zero, so "did any audio come out" is answerable by
       * looking for a nonzero sample. */
      int      tri = (int)(i % 64) * 500 - 16000;
      unsigned s   = (unsigned)(int16_t)tri;
      w[44 + i * 2]     = (unsigned char)( s       & 0xff);
      w[44 + i * 2 + 1] = (unsigned char)((s >> 8) & 0xff);
   }

   *size_out = total;
   return w;
}


static audio_mixer_voice_t *voice;
static retro_atomic_int_t   mixing;
static unsigned             odd_blocks;
static unsigned             odd_reads;

static void *reader_thread(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&mixing))
   {
      float   v = audio_mixer_voice_get_volume(voice);
      int32_t g = audio_mixer_voice_get_gain(voice);
      if (     (v != 0.0f && v != 1.0f)
            || (g != 0 && g != AUDIO_MIXER_GAIN_UNITY))
         odd_reads++;
   }
   return NULL;
}

static void *audio_thread(void *arg)
{
   float buf[BLOCK * 2];
   (void)arg;
   while (retro_atomic_load_acquire_int(&mixing))
   {
      unsigned i;
      float peak = 0.0f;
      memset(buf, 0, sizeof(buf));
      audio_mixer_mix(buf, BLOCK, 0.0f, false);
      for (i = 0; i < BLOCK * 2; i++)
         if (fabsf(buf[i]) > peak)
            peak = fabsf(buf[i]);
      /* the triangle peaks at 16000/32768 at volume 1, nothing at 0;
       * a block straddling a change may hold either */
      if (peak > 0.6f)
         odd_blocks++;
   }
   return NULL;
}

int main(void)
{
   pthread_t      t, r;
   size_t         wav_size;
   unsigned char *wav;
   audio_mixer_sound_t *snd;
   unsigned       i, bad = 0;

   audio_mixer_init(MIX_RATE);
   if (!(wav = make_wav(MIX_RATE / 4, MIX_RATE, &wav_size))
         || !(snd = audio_mixer_load_wav(wav, wav_size, "nearest",
               RESAMPLER_QUALITY_DONTCARE, false))
         || !(voice = audio_mixer_play(snd, true, 1.0f, "nearest",
               RESAMPLER_QUALITY_DONTCARE, NULL)))
   {
      printf("FAIL  could not set up a repeating voice\n");
      return 1;
   }

   retro_atomic_store_release_int(&mixing, 1);
   pthread_create(&t, NULL, audio_thread, NULL);
   pthread_create(&r, NULL, reader_thread, NULL);
   for (i = 0; i < ROUNDS; i++)
   {
      float   v = (i & 1) ? 1.0f : 0.0f;
      float   got;
      int32_t g;
      audio_mixer_voice_set_volume(voice, v);
      got = audio_mixer_voice_get_volume(voice);
      g   = audio_mixer_voice_get_gain(voice);
      if (got != v || g != (int32_t)(v * 65536.0f + 0.5f))
         bad++;
      audio_mixer_voice_set_gain(voice, AUDIO_MIXER_GAIN_UNITY);
   }
   retro_atomic_store_release_int(&mixing, 0);
   pthread_join(t, NULL);
   pthread_join(r, NULL);

   audio_mixer_stop(voice);
   audio_mixer_destroy(snd);
   audio_mixer_done();
   free(wav);

   if (bad || odd_reads || odd_blocks)
   {
      printf("FAIL  %u reads gave back another volume, %u values never set were read, "
            "%u blocks mixed louder than any set\n", bad, odd_reads, odd_blocks);
      return 1;
   }
   printf("ok    %u volume and gain changes made while the voice mixed\n", ROUNDS);
   return 0;
}
