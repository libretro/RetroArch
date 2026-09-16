/* The mixer's resample ratio is s_rate / source_rate, and a zero on
 * either side is not a ratio. Two ways a zero gets there:
 *
 *  - The mixer was never given its rate. audio_mixer_init() is what
 *    sets s_rate, and the frontend reaches it only after the audio
 *    device has opened; when the device fails to open, s_rate stays 0
 *    and menu sounds are loaded anyway. A ratio of 0 sized the one-shot
 *    output as samples * 0 plus the pad, and the sinc resampler, run on
 *    phases / 0.0, wrote frames past it - a heap overflow on every
 *    start with a broken audio device and menu sounds on.
 *  - The source header claims no rate. A WAV with samplerate 0 is
 *    valid input to the loader and gives a ratio of infinity, with the
 *    same result, from a file the user chose.
 *
 * Every ratio site now fails the load or play instead. These cases pin
 * that, and pin that an initialised mixer still loads everything it
 * did: a matrix of mixer rates, source rates, lengths and channel
 * counts, all of which must come back as sounds. Run under ASan so a
 * reintroduced write past the buffer is caught by the allocator
 * boundary and not by luck. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audio/audio_mixer.h>
#include <audio/audio_resampler.h>

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

/* A complete PCM16 WAV with a square-ish nonzero payload. */
static unsigned char *make_wav(unsigned frames, unsigned rate,
      unsigned ch, size_t *size_out)
{
   unsigned char *w;
   unsigned       i;
   size_t         data_bytes = (size_t)frames * 2 * ch;
   size_t         total      = 44 + data_bytes;
   unsigned long  byte_rate  = (unsigned long)rate * 2 * ch;

   if (!(w = (unsigned char*)calloc(1, total)))
      return NULL;

   memcpy(w, "RIFF", 4);
   w[4]  = (unsigned char)( (total - 8)        & 0xff);
   w[5]  = (unsigned char)(((total - 8) >> 8)  & 0xff);
   w[6]  = (unsigned char)(((total - 8) >> 16) & 0xff);
   w[7]  = (unsigned char)(((total - 8) >> 24) & 0xff);
   memcpy(w + 8, "WAVEfmt ", 8);
   w[16] = 16;
   w[20] = 1;
   w[22] = (unsigned char)ch;
   w[24] = (unsigned char)( rate        & 0xff);
   w[25] = (unsigned char)((rate >> 8)  & 0xff);
   w[26] = (unsigned char)((rate >> 16) & 0xff);
   w[27] = (unsigned char)((rate >> 24) & 0xff);
   w[28] = (unsigned char)( byte_rate        & 0xff);
   w[29] = (unsigned char)((byte_rate >> 8)  & 0xff);
   w[30] = (unsigned char)((byte_rate >> 16) & 0xff);
   w[31] = (unsigned char)((byte_rate >> 24) & 0xff);
   w[32] = (unsigned char)(2 * ch);
   w[34] = 16;
   memcpy(w + 36, "data", 4);
   w[40] = (unsigned char)( data_bytes        & 0xff);
   w[41] = (unsigned char)((data_bytes >> 8)  & 0xff);
   w[42] = (unsigned char)((data_bytes >> 16) & 0xff);
   w[43] = (unsigned char)((data_bytes >> 24) & 0xff);

   for (i = 0; i < frames * ch; i++)
   {
      short s = (short)((i & 1) ? 3000 : -3000);
      w[44 + 2 * i] = (unsigned char)(s & 0xff);
      w[45 + 2 * i] = (unsigned char)((s >> 8) & 0xff);
   }

   *size_out = total;
   return w;
}

/* Load one WAV and report whether a sound came back. */
static bool load_one(unsigned frames, unsigned rate, unsigned ch,
      enum resampler_quality q, bool s16)
{
   size_t size;
   unsigned char *w = make_wav(frames, rate, ch, &size);
   audio_mixer_sound_t *s;

   if (!w)
      return false;
   s = audio_mixer_load_wav(w, size, "sinc", q, s16);
   if (s)
      audio_mixer_destroy(s);
   free(w);
   return s != NULL;
}

int main(void)
{
   static const unsigned mix_rates[] = { 44100, 32000, 22050, 96000, 8000, 48000, 47999 };
   static const unsigned wav_rates[] = { 48000, 44100, 22050, 11025 };
   static const unsigned lens[]      = { 1, 7, 12000, 12001, 44100, 65535 };
   unsigned q, mi, wi, li, ch;
   unsigned loads = 0;

   /* 1. Mixer never initialised: s_rate is 0. Every quality, both
    *    sample formats, must be turned away rather than resampled. */
   for (q = 0; q <= RESAMPLER_QUALITY_HIGHEST; q++)
   {
      CHECK(!load_one(12000, 48000, 2, (enum resampler_quality)q, false),
            "uninitialised mixer loaded a float WAV at quality %u", q);
      CHECK(!load_one(12000, 48000, 2, (enum resampler_quality)q, true),
            "uninitialised mixer loaded an s16 WAV at quality %u", q);
   }

   /* 2. Initialised mixer, source header claiming no rate. */
   audio_mixer_init(48000);
   for (q = 0; q <= RESAMPLER_QUALITY_HIGHEST; q++)
   {
      CHECK(!load_one(12000, 0, 2, (enum resampler_quality)q, false),
            "zero-rate header loaded as float at quality %u", q);
      CHECK(!load_one(12000, 0, 2, (enum resampler_quality)q, true),
            "zero-rate header loaded as s16 at quality %u", q);
   }
   audio_mixer_done();

   /* 3. Everything legitimate still loads: the guard must not reach
    *    past the zero it is for. Includes the same-rate case, where no
    *    resample happens at all, and an off-by-one rate that does. */
   for (mi = 0; mi < sizeof(mix_rates) / sizeof(*mix_rates); mi++)
   {
      audio_mixer_init(mix_rates[mi]);
      for (wi = 0; wi < sizeof(wav_rates) / sizeof(*wav_rates); wi++)
         for (li = 0; li < sizeof(lens) / sizeof(*lens); li++)
            for (ch = 1; ch <= 2; ch++)
            {
               CHECK(load_one(lens[li], wav_rates[wi], ch,
                        RESAMPLER_QUALITY_NORMAL, false),
                     "mixer %u Hz refused a %u Hz, %u-frame, %u-channel WAV",
                     mix_rates[mi], wav_rates[wi], lens[li], ch);
               loads++;
            }
      audio_mixer_done();
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("audio_mixer rate guard: zero rates refused, %u legitimate loads accepted\n",
         loads);
   return 0;
}
