/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mixer_loop_test.c).
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

/* Regression test for a repeating mixer voice that stops producing
 * audio at its first loop point (libretro/RetroArch#19262).
 *
 * The menu's background music is a repeating stream voice, and the
 * mixer's repeat path is: read, and if nothing came out, seek the
 * stream back to zero and read again.  Two independent things have to
 * hold for that to terminate, and each has been broken separately:
 *
 *   - The decoder's seek has to actually put the stream somewhere
 *     readable.  audio_transfer's Vorbis arm bounds emission by the
 *     container's summed granule positions, and a seek that moved the
 *     stream without moving the emission cursor left the bound already
 *     spent, so every read after the loop point produced nothing.  The
 *     seek reported success, so the caller had no way to tell.
 *
 *   - The mixer has to give up if the seek did not help.  It looped on
 *     `goto again` with the seek's result discarded and no bound, so an
 *     unreadable stream spun the audio path at full speed with no
 *     frame ever emitted.  Not a deadlock and not a crash: the process
 *     stays alive, burns a core, and the frontend simply stops.
 *
 * Both halves are covered, and the failures they produce are
 * deliberately different, because that is what tells them apart.
 *
 *   1, 2  audio_transfer directly: after a seek to zero a stream hands
 *         out the same audio it just did, to the byte.  That is the
 *         contract the mixer's repeat path is built on.  WAV as well
 *         as Vorbis, so a second decoder holds the same line.
 *
 *   3     The mixer's repeat path on a real coded stream, which is the
 *         end-to-end shape of the report.  Two assertions, one per
 *         half: the run terminates, and the voice is still audible on
 *         the last call.  A broken decoder with the guard in place
 *         fails the second - a stopped voice, reported.  A broken
 *         decoder without it never reaches the assertion at all and
 *         the watchdog reports a spin.  Both are failures; which one
 *         appears says which half regressed.
 *
 *   4     A stream with no audio in it, which is the shape the guard
 *         exists for.  It does not currently get that far - the empty
 *         WAV is refused before a voice is made - so this records
 *         where the line is rather than crossing it.  Should some
 *         future arm let such a stream through, this is where it will
 *         show up, bounded by the watchdog instead of hanging.
 *
 *   5     The guard did not overshoot into cutting off sounds shorter
 *         than one mixing buffer, which legitimately wrap several
 *         times inside a single call.
 *
 * A spin is a hang, and a hang is not a test result, so the whole run
 * sits under a watchdog where one is available.  What it prints names
 * the case, because by then the stack is not going to.
 *
 * Self-contained: the Vorbis fixture is embedded and the WAV ones are
 * built in memory.  Takes no arguments.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audio/audio_mixer.h>
#include <formats/audio.h>

#include "ogg_fixture.h"

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <unistd.h>
#define HAVE_WATCHDOG 1
#endif

#define MIX_RATE       8000
#define MIX_FRAMES     1024
#define MIX_SAMPLES    (MIX_FRAMES * 2)

/* Enough mixing calls to pass the fixture's own length many times
 * over, so the repeat path is not merely reached but re-entered. */
#define MIX_CALLS      400

/* Seconds the whole run is allowed.  Generous by three orders of
 * magnitude against what it costs when it works, so this only ever
 * fires on a spin. */
#define WATCHDOG_SECS  30

static const char *current_case = "startup";

#ifdef HAVE_WATCHDOG
static void watchdog_fired(int sig)
{
   (void)sig;
   /* Deliberately write(2) and _exit(2): this runs from a signal
    * handler, and by definition the thing it interrupted was not
    * making progress. */
   {
      static const char msg[] =
         "\nFAIL: watchdog fired - the mixer is spinning in ";
      ssize_t ignored;
      ignored = write(2, msg, sizeof(msg) - 1);
      ignored = write(2, current_case, strlen(current_case));
      ignored = write(2, "\n", 1);
      (void)ignored;
   }
   _exit(1);
}
#endif

/* ------------------------------------------------------------------ */

/* A PCM16 WAV holding 'frames' frames of a triangle at 'rate', mono.
 * Zero frames is a legitimate ask: a header with an empty data chunk
 * is what case 4 needs. */
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

/* ------------------------------------------------------------------ */

/* Drain a stream to its end, into 'buf' when there is room for it.
 * A read writes channels samples per frame and leaves the rest of the
 * buffer alone, so the copy has to follow the channel count or it
 * carries stale samples from the previous read into the comparison. */
static size_t drain(void *h, enum audio_type_enum type, unsigned channels,
      float *buf, size_t cap)
{
   size_t total = 0;

   for (;;)
   {
      float  tmp[512 * 8];
      size_t got = 0;

      audio_transfer_read_f32(h, type, tmp, 512, &got);
      if (!got)
         break;
      if (buf && total + got * channels <= cap)
         memcpy(buf + total, tmp, got * channels * sizeof(float));
      total += got * channels;
   }

   return total / channels;
}

/* The contract the mixer's repeat path depends on: a stream that has
 * been played out and seeked back to zero plays out again, identically.
 * A decoder that reports a successful seek and then hands out nothing
 * is what turns the mixer's loop into a spin. */
static int check_seek_replays(void *buf, size_t size,
      enum audio_type_enum type, const char *what)
{
   void    *h;
   float   *a, *b;
   size_t   first, second;
   size_t   cap    = 1u << 20;
   int      ret    = 1;
   unsigned ch     = 0;
   unsigned rate   = 0;
   uint64_t frames = 0;

   if (!(h = audio_transfer_new(type)))
   {
      printf("  %s: audio_transfer_new failed\n", what);
      return 0;
   }
   audio_transfer_set_buffer_ptr(h, type, buf, size);
   if (!audio_transfer_start(h, type))
   {
      printf("  %s: audio_transfer_start failed\n", what);
      audio_transfer_free(h, type);
      return 0;
   }

   if (     !audio_transfer_info(h, type, &ch, &rate, &frames)
         || ch == 0 || ch > 8)
   {
      printf("  %s: audio_transfer_info gave %u channels\n", what, ch);
      audio_transfer_free(h, type);
      return 0;
   }

   a = (float*)malloc(cap * sizeof(float));
   b = (float*)malloc(cap * sizeof(float));
   if (!a || !b)
   {
      free(a);
      free(b);
      audio_transfer_free(h, type);
      return 0;
   }

   first = drain(h, type, ch, a, cap);
   if (!first)
   {
      printf("  %s: nothing decoded on the first pass\n", what);
      ret = 0;
   }
   else if (!audio_transfer_seek(h, type, 0))
   {
      printf("  %s: seek to 0 failed\n", what);
      ret = 0;
   }
   else
   {
      second = drain(h, type, ch, b, cap);

      if (!second)
      {
         printf("  %s: the repeat yielded nothing - "
                "the mixer would spin here\n", what);
         ret = 0;
      }
      else if (second != first)
      {
         printf("  %s: the repeat is %lu frames against %lu\n", what,
               (unsigned long)second, (unsigned long)first);
         ret = 0;
      }
      else if (first * ch <= cap
            && memcmp(a, b, first * ch * sizeof(float)))
      {
         printf("  %s: the repeat is not the same audio\n", what);
         ret = 0;
      }
      else
         printf("  %s: %lu frames, replayed byte-exact\n", what,
               (unsigned long)first);
   }

   free(a);
   free(b);
   audio_transfer_free(h, type);
   return ret;
}

/* The mixer takes ownership of the buffer it is handed and frees it in
 * audio_mixer_destroy, so every sound gets its own copy and the
 * originals stay ours. */
static void *dup_bytes(const void *src, size_t size)
{
   void *d = malloc(size);
   if (d)
      memcpy(d, src, size);
   return d;
}

/* ------------------------------------------------------------------ */

/* Run a repeating voice through the mixer for longer than the sound
 * lasts.  Returns 0 only on a wrong answer; a spin never returns at
 * all and is the watchdog's to report. */
/* Set from the mixer's own callback so the sound is not destroyed
 * while a voice still points into its bytes: a voice that ended by
 * itself must not be stopped again, and one that did not must not be
 * left running. */
static int voice_ended;

static void voice_stopped(audio_mixer_sound_t *sound, unsigned reason)
{
   (void)sound;
   if (     reason == AUDIO_MIXER_SOUND_FINISHED
         || reason == AUDIO_MIXER_SOUND_STOPPED)
      voice_ended = 1;
}

static int mix_repeating(audio_mixer_sound_t *sound, int s16,
      int expect_audio, const char *what)
{
   audio_mixer_voice_t *voice;
   float                out[MIX_SAMPLES];
   int16_t              out16[MIX_SAMPLES];
   int                  i;
   int                  nonzero = 0;

   voice_ended = 0;

   voice = s16
      ? audio_mixer_play_s16(sound, true, 0x10000,
            RESAMPLER_QUALITY_DONTCARE, voice_stopped)
      : audio_mixer_play(sound, true, 1.0f, NULL,
            RESAMPLER_QUALITY_DONTCARE, voice_stopped);

   if (!voice)
   {
      printf("  %s: the voice would not play\n", what);
      return 0;
   }

   for (i = 0; i < MIX_CALLS; i++)
   {
      int k;

      if (s16)
      {
         memset(out16, 0, sizeof(out16));
         audio_mixer_mix_s16(out16, MIX_FRAMES, 0x10000, false);
      }
      else
      {
         memset(out, 0, sizeof(out));
         audio_mixer_mix(out, MIX_FRAMES, 1.0f, false);
      }

      /* Only the last call is judged: what matters is whether the
       * voice is still alive after every loop point it has crossed,
       * not whether it was alive at the start. */
      if (i + 1 == MIX_CALLS)
         for (k = 0; k < MIX_SAMPLES; k++)
            if (s16 ? (out16[k] != 0) : (out[k] != 0.0f))
            {
               nonzero = 1;
               break;
            }
   }

   if (!voice_ended)
      audio_mixer_stop(voice);

   if (expect_audio && !nonzero)
   {
      printf("  %s: the voice went silent before the run ended\n", what);
      return 0;
   }
   if (!expect_audio && nonzero)
   {
      printf("  %s: a stream with no audio in it produced some\n", what);
      return 0;
   }

   printf("  %s: %d mixing calls, no spin%s\n", what, MIX_CALLS,
         expect_audio ? ", still audible" : "");
   return 1;
}

/* ------------------------------------------------------------------ */

int main(void)
{
   unsigned char       *wav      = NULL;
   unsigned char       *wav_short= NULL;
   unsigned char       *wav_empty= NULL;
   size_t               wav_size, wav_short_size, wav_empty_size;
   audio_mixer_sound_t *snd;
   int                  fails    = 0;

#ifdef HAVE_WATCHDOG
   signal(SIGALRM, watchdog_fired);
   alarm(WATCHDOG_SECS);
#endif

   /* Half a second at the mixing rate: several mixing buffers long,
    * so it loops a handful of times over the run rather than once. */
   wav       = make_wav(MIX_RATE / 2, MIX_RATE, &wav_size);
   /* Shorter than one mixing buffer, so it wraps repeatedly inside a
    * single call to the mixer. */
   wav_short = make_wav(200,          MIX_RATE, &wav_short_size);
   /* No audio at all. */
   wav_empty = make_wav(0,            MIX_RATE, &wav_empty_size);

   if (!wav || !wav_short || !wav_empty)
   {
      printf("out of memory\n");
      return 1;
   }

   audio_mixer_init(MIX_RATE);

   printf("1. audio_transfer: a WAV stream replays after a seek to 0\n");
   current_case = "case 1 (WAV seek contract)";
   if (!check_seek_replays(wav, wav_size, AUDIO_TYPE_WAV, "wav"))
      fails++;

   printf("2. audio_transfer: an Ogg Vorbis stream replays after a seek to 0\n");
   current_case = "case 2 (Vorbis seek contract)";
   if (!check_seek_replays((void*)ogg_fixture, sizeof(ogg_fixture),
            AUDIO_TYPE_VORBIS, "ogg"))
      fails++;

   printf("3. audio_mixer: a repeating Ogg voice keeps playing past its loop point\n");
   current_case = "case 3 (mixer repeat, float)";
   if (!(snd = audio_mixer_load_ogg(
               dup_bytes(ogg_fixture, sizeof(ogg_fixture)),
               OGG_FIXTURE_SIZE)))
   {
      printf("  the fixture would not load\n");
      fails++;
   }
   else
   {
      if (!mix_repeating(snd, 0, 1, "float"))
         fails++;
      audio_mixer_destroy(snd);
   }

   current_case = "case 3 (mixer repeat, int16)";
   if (!(snd = audio_mixer_load_ogg(
               dup_bytes(ogg_fixture, sizeof(ogg_fixture)),
               OGG_FIXTURE_SIZE)))
   {
      printf("  the fixture would not load\n");
      fails++;
   }
   else
   {
      if (!mix_repeating(snd, 1, 1, "int16"))
         fails++;
      audio_mixer_destroy(snd);
   }

   printf("4. audio_mixer: a stream with no audio in it never becomes a voice\n");
   current_case = "case 4 (empty stream)";
   if (!(snd = audio_mixer_load_wav_stream(
               dup_bytes(wav_empty, wav_empty_size),
               (int32_t)wav_empty_size)))
      printf("  empty: refused at load\n");
   else
   {
      audio_mixer_voice_t *v;

      voice_ended = 0;
      v = audio_mixer_play(snd, true, 1.0f, NULL,
            RESAMPLER_QUALITY_DONTCARE, voice_stopped);

      if (v)
      {
         /* It played, so the repeat path is reachable with nothing to
          * hand out and the guard is what has to end it.  Bounded by
          * the watchdog either way. */
         printf("  empty: playing; the repeat has nothing to hand out\n");
         if (!voice_ended)
            audio_mixer_stop(v);
         if (!mix_repeating(snd, 0, 0, "empty"))
            fails++;
      }
      else
         printf("  empty: refused at play\n");

      audio_mixer_destroy(snd);
   }

   printf("5. audio_mixer: a sound shorter than one mixing buffer is not cut off\n");
   current_case = "case 5 (short sound liveness)";
   if (!(snd = audio_mixer_load_wav_stream(
               dup_bytes(wav_short, wav_short_size),
               (int32_t)wav_short_size)))
   {
      printf("  the short WAV would not load\n");
      fails++;
   }
   else
   {
      if (!mix_repeating(snd, 0, 1, "short"))
         fails++;
      audio_mixer_destroy(snd);
   }

   audio_mixer_done();
   free(wav);
   free(wav_short);
   free(wav_empty);

   if (fails)
   {
      printf("\nFAIL: %d case(s)\n", fails);
      return 1;
   }

   printf("\nOK\n");
   return 0;
}
