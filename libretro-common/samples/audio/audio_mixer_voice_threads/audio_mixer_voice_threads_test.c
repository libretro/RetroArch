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

/* A voice a caller holds - building its decoder in play, tearing it
 * down in stop - never holds the mix up: the mix passes it over and
 * returns. And voices played and stopped by one thread while the audio
 * thread mixes them: a decoder freed under the mix is a use after free.
 *
 * A stream voice's position read and its bound set by a feeder thread,
 * with no lock, while the audio thread mixes and loops the voice: every
 * position read must lie inside the stream.
 *
 * A voice's volume and gain, set by a control thread and read by a
 * second while the audio thread mixes the voice. Built under
 * ThreadSanitizer: a field two threads touch without ordering is a
 * reported race. Every read must give back one of the values set, and
 * every mixed block must come out at one of the volumes set - silence
 * or the full triangle - never anything a torn value would give.
 *
 * Two callers waiting on one voice while a mix takes it between them:
 * each holds it alone, and none is left parked when it is let go. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include <audio/audio_mixer.h>
#include <audio/audio_resampler.h>
#include <retro_atomic.h>

/* For audio_mixer_voice_own(): a caller's hold on a voice. */
#include "../../../audio/audio_mixer.c"

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
   size_t previous = 0;
   size_t serial = 0;
   (void)arg;
   while (retro_atomic_load_acquire_int(&mixing))
   {
      float   v = audio_mixer_voice_get_volume(voice);
      int32_t g = audio_mixer_voice_get_gain(voice);
      size_t frames;
      size_t generation;
      unsigned rate;
      if (!audio_mixer_voice_clock(voice, &frames, &rate, &generation)
            || rate != MIX_RATE || frames < previous || !generation
            || (serial && serial != generation)) odd_reads++;
      previous = frames;
      serial = generation;
      if (     (v != 0.0f && v != 1.0f)
            || (g != 0 && g != AUDIO_MIXER_GAIN_UNITY))
         odd_reads++;
   }
   return NULL;
}

static audio_mixer_voice_t *stream_voice;
static size_t               stream_size;
static unsigned             odd_tells;
static unsigned             tells;

static void *feeder_thread(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&mixing))
   {
      size_t at = audio_mixer_voice_buffer_tell(stream_voice);
      if (at > stream_size)
         odd_tells++;
      audio_mixer_voice_set_avail(stream_voice, at + 4096);
      audio_mixer_voice_set_paused(stream_voice, (tells & 1) != 0);
      tells++;
   }
   return NULL;
}

static audio_mixer_sound_t *churn_sound;
static retro_atomic_int_t   churned;
#define CHURN_ROUNDS 300

static void *churn_thread(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&mixing))
   {
      audio_mixer_voice_t *v = audio_mixer_play(churn_sound, true, 0.0f,
            "nearest", RESAMPLER_QUALITY_DONTCARE, NULL);
      if (v)
      {
         audio_mixer_stop(v);
         retro_atomic_fetch_add_int(&churned, 1);
      }
   }
   return NULL;
}

/* What the holders of one contended voice share, unordered but for
 * their hold on it. */
#define CONTEND_ROUNDS 20000
static audio_mixer_voice_t *contended;
static int                  inside;
static unsigned             overlaps;
static retro_atomic_int_t   callers_done;

static void contend_hold(void)
{
   if (++inside != 1)
      overlaps++;
   sched_yield();
   inside--;
}

static void *contend_caller(void *arg)
{
   unsigned i;
   (void)arg;
   for (i = 0; i < CONTEND_ROUNDS; i++)
   {
      audio_mixer_voice_own(contended);
      contend_hold();
      audio_mixer_voice_disown(contended);
   }
   retro_atomic_fetch_add_int(&callers_done, 1);
   return NULL;
}

static void *contend_mix(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&callers_done) < 2)
      if (audio_mixer_voice_try_own(contended))
      {
         contend_hold();
         audio_mixer_voice_disown(contended);
      }
   return NULL;
}

/* A caller left parked never finishes: the run fails rather than hangs. */
static void *watchdog_thread(void *arg)
{
   unsigned s;
   (void)arg;
   for (s = 0; s < 300; s++)
      sleep(1);
   printf("FAIL  still running after 300 s: a caller waiting on a voice was never woken\n");
   fflush(stdout);
   _exit(1);
   return NULL;
}

static float held_peak(void)
{
   float    buf[BLOCK * 2];
   float    peak = 0.0f;
   unsigned i;
   memset(buf, 0, sizeof(buf));
   audio_mixer_mix(buf, BLOCK, 0.0f, false);
   for (i = 0; i < BLOCK * 2; i++)
      if (fabsf(buf[i]) > peak)
         peak = fabsf(buf[i]);
   return peak;
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
   pthread_t      t, r, f, c, w;
   size_t         wav_size, wav3_size;
   unsigned char *wav, *wav2, *wav3;
   audio_mixer_sound_t *snd, *snd2;
   unsigned       i, bad = 0;

   pthread_create(&w, NULL, watchdog_thread, NULL);
   pthread_detach(w);
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

   /* the same triangle as a silent stream voice, short enough to loop
    * many times under the feeder */
   if (     !(wav2 = make_wav(MIX_RATE / 100, MIX_RATE, &stream_size))
         || !(snd2 = audio_mixer_load_wav_stream(wav2, stream_size))
         || !(stream_voice = audio_mixer_play(snd2, true, 0.0f, "nearest",
               RESAMPLER_QUALITY_DONTCARE, NULL)))
   {
      printf("FAIL  could not set up a repeating stream voice\n");
      return 1;
   }

   if (     !(wav3 = make_wav(MIX_RATE / 100, MIX_RATE, &wav3_size))
         || !(churn_sound = audio_mixer_load_wav_stream(wav3, wav3_size)))
   {
      printf("FAIL  could not set up the sound to play and stop\n");
      return 1;
   }

   /* Held by a caller, on this very thread: a mix that waited for the
    * voice would never return. It passes the voice over instead, and
    * mixes it again once the caller lets go. */
   audio_mixer_voice_own(voice);
   if (held_peak() != 0.0f)
   {
      printf("FAIL  a voice a caller held was mixed\n");
      return 1;
   }
   audio_mixer_voice_disown(voice);
   if (held_peak() == 0.0f)
   {
      printf("FAIL  a voice was not mixed once its caller let go\n");
      return 1;
   }

   retro_atomic_store_release_int(&mixing, 1);
   pthread_create(&t, NULL, audio_thread, NULL);
   pthread_create(&r, NULL, reader_thread, NULL);
   pthread_create(&f, NULL, feeder_thread, NULL);
   pthread_create(&c, NULL, churn_thread, NULL);
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
   while (retro_atomic_load_acquire_int(&churned) < CHURN_ROUNDS)
      sched_yield();
   retro_atomic_store_release_int(&mixing, 0);
   pthread_join(t, NULL);
   pthread_join(r, NULL);
   pthread_join(f, NULL);
   pthread_join(c, NULL);

   /* a slot none of the above plays */
   {
      pthread_t a, b, m;
      contended = &s_voices[AUDIO_MIXER_MAX_VOICES - 1];
      pthread_create(&m, NULL, contend_mix, NULL);
      pthread_create(&a, NULL, contend_caller, NULL);
      pthread_create(&b, NULL, contend_caller, NULL);
      pthread_join(a, NULL);
      pthread_join(b, NULL);
      pthread_join(m, NULL);
   }

   audio_mixer_stop(voice);
   audio_mixer_destroy(snd);
   audio_mixer_stop(stream_voice);
   audio_mixer_destroy(snd2);
   audio_mixer_destroy(churn_sound);
   audio_mixer_done();
   free(wav);

   if (overlaps)
   {
      printf("FAIL  %u holds of a contended voice overlapped\n", overlaps);
      return 1;
   }
   if (odd_tells)
   {
      printf("FAIL  %u of %u positions read lay outside the stream\n",
            odd_tells, tells);
      return 1;
   }
   if (bad || odd_reads || odd_blocks)
   {
      printf("FAIL  %u reads gave back another volume, %u values never set were read, "
            "%u blocks mixed louder than any set\n", bad, odd_reads, odd_blocks);
      return 1;
   }
   printf("ok    %u volume and gain changes made while the voice mixed, "
         "%u voices played and stopped under the mix, "
         "two callers x %u holds of one voice beside the mix\n", ROUNDS,
         (unsigned)retro_atomic_load_acquire_int(&churned), CONTEND_ROUNDS);
   return 0;
}
