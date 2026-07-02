/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_mixer.c).
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <audio/audio_mixer.h>
#include <audio/audio_resampler.h>
#include <audio/sinc_resampler_int16.h>

#ifdef HAVE_RWAV
#include <formats/rwav.h>
#endif
#include <memalign.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_STB_VORBIS
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_CRT

#include <stb/stb_vorbis.h>
#endif

#ifdef HAVE_RFLAC
#include <formats/audio.h>
#endif

#ifdef HAVE_DR_MP3
#define DR_MP3_IMPLEMENTATION
#include <retro_assert.h>
#define DRMP3_ASSERT(expression) retro_assert(expression)
#include <dr/dr_mp3.h>
#endif

#ifdef HAVE_IBXM
#include <ibxm/ibxm.h>
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#define AUDIO_MIXER_LOCK(voice)   slock_lock(voice->lock)
#define AUDIO_MIXER_UNLOCK(voice) slock_unlock(voice->lock)
#else
#define AUDIO_MIXER_LOCK(voice)   do {} while(0)
#define AUDIO_MIXER_UNLOCK(voice) do {} while(0)
#endif

#define AUDIO_MIXER_MAX_VOICES      8
#define AUDIO_MIXER_TEMP_BUFFER 8192

struct audio_mixer_sound
{
   enum audio_mixer_type type;
   void* user_data;

   union
   {
      struct
      {
         /* wav */
         const float* pcm;
         unsigned frames;
      } wav;

#ifdef HAVE_STB_VORBIS
      struct
      {
         /* ogg */
         const void* data;
         unsigned size;
      } ogg;
#endif

#ifdef HAVE_RFLAC
      struct
      {
          /* flac */
         const void* data;
         unsigned size;
      } flac;
#endif

#ifdef HAVE_DR_MP3
      struct
      {
          /* mp */
         const void* data;
         unsigned size;
      } mp3;
#endif

#ifdef HAVE_IBXM
      struct
      {
         /* mod/s3m/xm */
         const void* data;
         unsigned size;
      } mod;
#endif
   } types;
};

struct audio_mixer_voice
{
   struct
   {
      struct
      {
         unsigned position;
      } wav;

#ifdef HAVE_STB_VORBIS
      struct
      {
         stb_vorbis *stream;
         void       *resampler_data;
         const retro_resampler_t *resampler;
         float      *buffer;
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         float       ratio;
      } ogg;
#endif

#ifdef HAVE_RFLAC
      struct
      {
         float*      buffer;
         void        *stream; /* audio_transfer ctx (AUDIO_TYPE_FLAC) */
         void        *resampler_data;
         const retro_resampler_t *resampler;
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         float       ratio;
         /* s16 pipeline (parallel; used when voice->is_s16) */
         int16_t*    buffer_s16;
         void*       resampler_int16;
      } flac;
#endif

#ifdef HAVE_DR_MP3
      struct
      {
         drmp3       stream;
         void        *resampler_data;
         const retro_resampler_t *resampler;
         float*      buffer;
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         float       ratio;
      } mp3;
#endif

#ifdef HAVE_IBXM
      struct
      {
         int*              buffer;
         struct replay*    stream;
         struct module*    module;
         unsigned          position;
         unsigned          samples;
         unsigned          buf_samples;
      } mod;
#endif
   } types;
   audio_mixer_sound_t *sound;
   audio_mixer_stop_cb_t stop_cb;
   unsigned type;
   float    volume;
   bool     repeat;
   bool     is_s16;
#ifdef HAVE_THREADS
   slock_t *lock;
#endif
};

/* TODO/FIXME - static globals */
static struct audio_mixer_voice s_voices[AUDIO_MIXER_MAX_VOICES] = {0};
static unsigned s_rate = 0;

static void audio_mixer_release(audio_mixer_voice_t* voice);

#ifdef HAVE_RWAV
static bool wav_to_float(const rwav_t* wav, float** pcm, size_t len)
{
   size_t i;
   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes */
   float *f           = (float*)memalign_alloc(16,
         ((len + 15) & ~15) * sizeof(float));

   if (!f)
      return false;

   *pcm = f;

   /* Canonical PCM->float scaling, matching audio/conversion/s16_to_float
    * (s16 / 0x8000) and audio_mix's 8-bit path ((u8 - 128) / 128). The
    * previous (s + 32768) / 65535 * 2 - 1 mapping introduced a small
    * positive DC offset (0 -> +1.5e-5) and a non-canonical scale; using
    * the same factor as the rest of the pipeline keeps the mixer's float
    * representation consistent (and the result deterministic) across the
    * s16/float boundaries the voices are mixed and clamped at. The mono
    * channel-duplication below is why the audio/conversion helpers can't
    * be called verbatim here. */
   if (wav->bitspersample == 8)
   {
      float sample      = 0.0f;
      const uint8_t *u8 = (const uint8_t*)wav->samples;

      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            sample = ((float)*u8++ - 128.0f) * (1.0f / 128.0f);
            *f++   = sample;
            *f++   = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            *f++ = ((float)*u8++ - 128.0f) * (1.0f / 128.0f);
            *f++ = ((float)*u8++ - 128.0f) * (1.0f / 128.0f);
         }
      }
   }
   else
   {
      float sample       = 0.0f;
      const int16_t *s16 = (const int16_t*)wav->samples;

      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            sample = (float)*s16++ * (1.0f / 0x8000);
            *f++   = sample;
            *f++   = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            *f++ = (float)*s16++ * (1.0f / 0x8000);
            *f++ = (float)*s16++ * (1.0f / 0x8000);
         }
      }
   }

   return true;
}

static bool one_shot_resample(const float* in, size_t samples_in,
      unsigned rate, const char *resampler_ident, enum resampler_quality quality,
      float** out, size_t* samples_out)
{
   struct resampler_data info;
   void* data                         = NULL;
   const retro_resampler_t* resampler = NULL;
   float ratio                        = (double)s_rate / (double)rate;

   if (!retro_resampler_realloc(&data, &resampler,
         resampler_ident, quality, ratio))
      return false;

   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes. We
    * add 16 more samples in the formula below just as safeguard, because
    * resampler->process sometimes reports more output samples than the
    * formula below calculates. Ideally, audio resamplers should have a
    * function to return the number of samples they will output given a
    * count of input samples. */
   *samples_out                       = (size_t)(samples_in * ratio);
   *out                               = (float*)memalign_alloc(16,
         (((*samples_out + 16) + 15) & ~15) * sizeof(float));

   if (*out == NULL)
      return false;

   info.data_in                       = in;
   info.data_out                      = *out;
   info.input_frames                  = samples_in / 2;
   info.output_frames                 = 0;
   info.ratio                         = ratio;

   resampler->process(data, &info);
   resampler->free(data);
   return true;
}
#endif

void audio_mixer_init(unsigned rate)
{
   unsigned i;

   s_rate = rate;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++)
   {
      audio_mixer_voice_t *voice = &s_voices[i];

      voice->type = AUDIO_MIXER_TYPE_NONE;
#ifdef HAVE_THREADS
      if (!voice->lock)
         voice->lock = slock_new();
#endif
   }
}

void audio_mixer_done(void)
{
   unsigned i;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++)
   {
      audio_mixer_voice_t *voice = &s_voices[i];

      AUDIO_MIXER_LOCK(voice);
      audio_mixer_release(voice);
      AUDIO_MIXER_UNLOCK(voice);
#ifdef HAVE_THREADS
      slock_free(voice->lock);
      voice->lock = NULL;
#endif
   }
}

/* --------------------------------------------------------------------------
 * Fixed-point (s16) mixer pipeline.
 *
 * A full parallel to the float pipeline above: voices decode straight to
 * int16, resample with the deterministic integer SINC resampler, and are
 * summed with saturation into an int16 output buffer. Nothing crosses
 * between the two pipelines, so neither incurs an int16<->float round-trip.
 * ------------------------------------------------------------------------ */

static int16_t audio_mixer_sat_s16(int32_t v)
{
   if (v >  32767)
      return  32767;
   if (v < -32768)
      return -32768;
   return (int16_t)v;
}

/* Apply a Q16 gain to an s16 sample, rounding toward zero (matches the
 * fixed-point volume applied on the core int16 audio path). */
static int32_t audio_mixer_gain_s16(int16_t s, int32_t gain_q16)
{
   int32_t p = (int32_t)s * gain_q16;
   return (p >= 0) ? (p >> 16) : -((-p) >> 16);
}

static enum sinc_int16_quality audio_mixer_i16_quality(enum resampler_quality q)
{
   switch (q)
   {
      case RESAMPLER_QUALITY_LOWEST:  return SINC_INT16_QUALITY_LOWEST;
      case RESAMPLER_QUALITY_LOWER:   return SINC_INT16_QUALITY_LOWER;
      case RESAMPLER_QUALITY_HIGHER:  return SINC_INT16_QUALITY_HIGHER;
      case RESAMPLER_QUALITY_HIGHEST: return SINC_INT16_QUALITY_HIGHEST;
      case RESAMPLER_QUALITY_NORMAL:
      case RESAMPLER_QUALITY_DONTCARE:
      default:                        return SINC_INT16_QUALITY_NORMAL;
   }
}

audio_mixer_sound_t* audio_mixer_load_wav(void *buffer, int32_t size,
      const char *resampler_ident, enum resampler_quality quality)
{
#ifdef HAVE_RWAV
   /* WAV data */
   rwav_t wav;
   /* WAV samples converted to float */
   float* pcm                 = NULL;
   size_t samples             = 0;
   /* Result */
   audio_mixer_sound_t* sound = NULL;

   wav.bitspersample          = 0;
   wav.numchannels            = 0;
   wav.samplerate             = 0;
   wav.numsamples             = 0;
   wav.subchunk2size          = 0;
   wav.samples                = NULL;

   if ((rwav_load(&wav, buffer, size)) != RWAV_ITERATE_DONE)
      return NULL;

   samples       = wav.numsamples * 2;

   if (!wav_to_float(&wav, &pcm, samples))
      return NULL;

   if (wav.samplerate != s_rate)
   {
      float* resampled           = NULL;

      if (!one_shot_resample(pcm, samples, wav.samplerate,
            resampler_ident, quality,
            &resampled, &samples))
         return NULL;

      memalign_free((void*)pcm);
      pcm = resampled;
   }

   sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
   {
      memalign_free((void*)pcm);
      return NULL;
   }

   sound->type             = AUDIO_MIXER_TYPE_WAV;
   sound->types.wav.frames = (unsigned)(samples / 2);
   sound->types.wav.pcm    = pcm;

   rwav_free(&wav);

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_ogg(void *buffer, int32_t size)
{
#ifdef HAVE_STB_VORBIS
   audio_mixer_sound_t* sound;

   if (!buffer || size <= 0)
      return NULL;

   sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_OGG;
   sound->types.ogg.size = size;
   sound->types.ogg.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_flac(void *buffer, int32_t size)
{
#ifdef HAVE_RFLAC
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_FLAC;
   sound->types.flac.size = size;
   sound->types.flac.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_mp3(void *buffer, int32_t size)
{
#ifdef HAVE_DR_MP3
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_MP3;
   sound->types.mp3.size = size;
   sound->types.mp3.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_mod(void *buffer, int32_t size)
{
#ifdef HAVE_IBXM
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_MOD;
   sound->types.mod.size = size;
   sound->types.mod.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

void audio_mixer_destroy(audio_mixer_sound_t* sound)
{
   void *handle = NULL;
   if (!sound)
      return;

   switch (sound->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle = (void*)sound->types.wav.pcm;
         if (handle)
            memalign_free(handle);
         break;
      case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_STB_VORBIS
         handle = (void*)sound->types.ogg.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
         handle = (void*)sound->types.mod.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
         handle = (void*)sound->types.flac.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
         handle = (void*)sound->types.mp3.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   free(sound);
}

static bool audio_mixer_play_wav(audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice, bool repeat, float volume,
      audio_mixer_stop_cb_t stop_cb)
{
   voice->types.wav.position = 0;
   return true;
}

#ifdef HAVE_STB_VORBIS
static bool audio_mixer_play_ogg(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   stb_vorbis_info info;
   int res                         = 0;
   float ratio                     = 1.0f;
   unsigned samples                = 0;
   void *ogg_buffer                = NULL;
   void *resampler_data            = NULL;
   const retro_resampler_t* resamp = NULL;
   stb_vorbis *stb_vorbis          = stb_vorbis_open_memory(
         (const unsigned char*)sound->types.ogg.data,
         sound->types.ogg.size, &res, NULL);

   if (!stb_vorbis)
      return false;

   info                    = stb_vorbis_get_info(stb_vorbis);

   if (info.sample_rate != s_rate)
   {
      ratio = (double)s_rate / (double)info.sample_rate;

      if (!retro_resampler_realloc(&resampler_data,
               &resamp, resampler_ident, quality,
               ratio))
         goto error;
   }

   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes. We
    * add 16 more samples in the formula below just as safeguard, because
    * resampler->process sometimes reports more output samples than the
    * formula below calculates. Ideally, audio resamplers should have a
    * function to return the number of samples they will output given a
    * count of input samples. */
   samples                         = (unsigned)(AUDIO_MIXER_TEMP_BUFFER * ratio);
   ogg_buffer                      = (float*)memalign_alloc(16,
         (((samples + 16) + 15) & ~15) * sizeof(float));

   if (!ogg_buffer)
   {
      if (resamp && resampler_data)
         resamp->free(resampler_data);
      goto error;
   }

   voice->types.ogg.resampler      = resamp;
   voice->types.ogg.resampler_data = resampler_data;
   voice->types.ogg.buffer         = (float*)ogg_buffer;
   voice->types.ogg.buf_samples    = samples;
   voice->types.ogg.ratio          = ratio;
   voice->types.ogg.stream         = stb_vorbis;
   voice->types.ogg.position       = 0;
   voice->types.ogg.samples        = 0;

   return true;

error:
   stb_vorbis_close(stb_vorbis);
   return false;
}

static void audio_mixer_release_ogg(audio_mixer_voice_t* voice)
{
   if (voice->types.ogg.stream)
      stb_vorbis_close(voice->types.ogg.stream);
   if (voice->types.ogg.resampler && voice->types.ogg.resampler_data)
      voice->types.ogg.resampler->free(voice->types.ogg.resampler_data);
   if (voice->types.ogg.buffer)
      memalign_free(voice->types.ogg.buffer);
}

#endif

#ifdef HAVE_IBXM
static bool audio_mixer_play_mod(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      audio_mixer_stop_cb_t stop_cb)
{
   struct data data;
   char message[64];
   int buf_samples               = 0;
   int samples                   = 0;
   void *mod_buffer              = NULL;
   struct module* module         = NULL;
   struct replay* replay         = NULL;

   data.buffer                   = (char*)sound->types.mod.data;
   data.length                   = sound->types.mod.size;
   module                        = module_load(&data, message);

   if (!module)
   {
      printf("audio_mixer_play_mod module_load() failed with error: %s\n", message);
      goto error;
   }

   if (voice->types.mod.module)
      dispose_module(voice->types.mod.module);

   voice->types.mod.module = module;

   replay = new_replay(module, s_rate, 1);

   if (!replay)
   {
      printf("audio_mixer_play_mod new_replay() failed\n");
      goto error;
   }

   buf_samples = calculate_mix_buf_len(s_rate);
   mod_buffer  = memalign_alloc(16, ((buf_samples + 15) & ~15) * sizeof(int));

   if (!mod_buffer)
   {
      printf("audio_mixer_play_mod cannot allocate mod_buffer !\n");
      goto error;
   }

   samples = replay_calculate_duration(replay);

   if (!samples)
   {
      printf("audio_mixer_play_mod cannot retrieve duration !\n");
      goto error;
   }

   voice->types.mod.buffer         = (int*)mod_buffer;
   voice->types.mod.buf_samples    = buf_samples;
   voice->types.mod.stream         = replay;
   voice->types.mod.position       = 0;
   voice->types.mod.samples        = 0; /* samples; */

   return true;

error:
   if (mod_buffer)
      memalign_free(mod_buffer);
   if (module)
      dispose_module(module);
   return false;

}

static void audio_mixer_release_mod(audio_mixer_voice_t* voice)
{
   if (voice->types.mod.stream)
      dispose_replay(voice->types.mod.stream);
   if (voice->types.mod.buffer)
      memalign_free(voice->types.mod.buffer);
}
#endif

#ifdef HAVE_RFLAC
static bool audio_mixer_play_flac(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   float ratio                     = 1.0f;
   unsigned samples                = 0;
   unsigned channels               = 0;
   unsigned rate                   = 0;
   void *flac_buffer               = NULL;
   void *resampler_data            = NULL;
   const retro_resampler_t* resamp = NULL;
   void *xfer                      = audio_transfer_new(AUDIO_TYPE_FLAC);

   if (!xfer)
      return false;

   audio_transfer_set_buffer_ptr(xfer, AUDIO_TYPE_FLAC,
         (void*)sound->types.flac.data, sound->types.flac.size);
   if (!audio_transfer_start(xfer, AUDIO_TYPE_FLAC))
   {
      audio_transfer_free(xfer, AUDIO_TYPE_FLAC);
      return false;
   }

   audio_transfer_info(xfer, AUDIO_TYPE_FLAC, &channels, &rate, NULL);

   /* The downstream mixer (audio_mixer_mix_flac) requests
    * AUDIO_MIXER_TEMP_BUFFER / 2 frames into a stack buffer
    * sized AUDIO_MIXER_TEMP_BUFFER floats.  rflac writes
    * frame_count * channel_count floats, so this only fits
    * exactly for stereo.  Mono fits but the downstream
    * accounting is wrong (per existing comment); >2 channels
    * overflows the stack buffer (e.g. 8-channel FLAC writes
    * 4 * AUDIO_MIXER_TEMP_BUFFER floats = 4x the buffer).
    * Reject anything that isn't stereo here rather than risk a
    * stack overflow during mix.  Mono should be fixed
    * separately by adjusting the mixer's per-channel
    * accounting. */
   if (channels != 2)
      goto error;

   if (rate != s_rate)
   {
      ratio = (double)s_rate / (double)rate;

      if (!retro_resampler_realloc(&resampler_data,
               &resamp, resampler_ident, quality,
               ratio))
         goto error;
   }

   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes. We
    * add 16 more samples in the formula below just as safeguard, because
    * resampler->process sometimes reports more output samples than the
    * formula below calculates. Ideally, audio resamplers should have a
    * function to return the number of samples they will output given a
    * count of input samples. */
   samples                         = (unsigned)(AUDIO_MIXER_TEMP_BUFFER * ratio);
   flac_buffer                     = (float*)memalign_alloc(16,
         (((samples + 16) + 15) & ~15) * sizeof(float));

   if (!flac_buffer)
   {
      if (resamp && resamp->free)
         resamp->free(resampler_data);
      goto error;
   }

   voice->types.flac.resampler      = resamp;
   voice->types.flac.resampler_data = resampler_data;
   voice->types.flac.buffer         = (float*)flac_buffer;
   voice->types.flac.buf_samples    = samples;
   voice->types.flac.ratio          = ratio;
   voice->types.flac.stream         = xfer;
   voice->types.flac.position       = 0;
   voice->types.flac.samples        = 0;

   return true;

error:
   audio_transfer_free(xfer, AUDIO_TYPE_FLAC);
   return false;
}

static void audio_mixer_release_flac(audio_mixer_voice_t* voice)
{
   if (voice->types.flac.stream)
      audio_transfer_free(voice->types.flac.stream, AUDIO_TYPE_FLAC);
   if (voice->types.flac.resampler && voice->types.flac.resampler_data)
      voice->types.flac.resampler->free(voice->types.flac.resampler_data);
   if (voice->types.flac.buffer)
      memalign_free(voice->types.flac.buffer);
   if (voice->types.flac.buffer_s16)
      memalign_free(voice->types.flac.buffer_s16);
   if (voice->types.flac.resampler_int16)
      sinc_resampler_int16_free(voice->types.flac.resampler_int16);
}

static bool audio_mixer_play_flac_s16(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   double   ratio       = 1.0;
   unsigned samples     = 0;
   unsigned channels    = 0;
   unsigned rate        = 0;
   void    *flac_buffer = NULL;
   void    *resamp_i16  = NULL;
   void    *xfer        = audio_transfer_new(AUDIO_TYPE_FLAC);
   (void)repeat;
   (void)volume;
   (void)stop_cb;

   if (!xfer)
      return false;
   audio_transfer_set_buffer_ptr(xfer, AUDIO_TYPE_FLAC,
         (void*)sound->types.flac.data, sound->types.flac.size);
   if (!audio_transfer_start(xfer, AUDIO_TYPE_FLAC))
   {
      audio_transfer_free(xfer, AUDIO_TYPE_FLAC);
      return false;
   }
   audio_transfer_info(xfer, AUDIO_TYPE_FLAC, &channels, &rate, NULL);

   /* Stereo-only, matching the float path's stack-buffer sizing. */
   if (channels != 2)
      goto error;

   if (rate != s_rate)
   {
      ratio      = (double)s_rate / (double)rate;
      resamp_i16 = sinc_resampler_int16_init(
            (ratio < 1.0) ? ratio : 1.0,
            audio_mixer_i16_quality(quality));
      if (!resamp_i16)
         goto error;
   }

   samples     = (unsigned)(AUDIO_MIXER_TEMP_BUFFER * ratio);
   flac_buffer = memalign_alloc(16,
         (((samples + 16) + 15) & ~15) * sizeof(int16_t));

   if (!flac_buffer)
   {
      if (resamp_i16)
         sinc_resampler_int16_free(resamp_i16);
      goto error;
   }

   voice->types.flac.resampler       = NULL;
   voice->types.flac.resampler_data  = NULL;
   voice->types.flac.buffer          = NULL;
   voice->types.flac.resampler_int16 = resamp_i16;
   voice->types.flac.buffer_s16      = (int16_t*)flac_buffer;
   voice->types.flac.buf_samples     = samples;
   voice->types.flac.ratio           = (float)ratio;
   voice->types.flac.stream          = xfer;
   voice->types.flac.position        = 0;
   voice->types.flac.samples         = 0;

   return true;

error:
   audio_transfer_free(xfer, AUDIO_TYPE_FLAC);
   return false;
}
#endif

#ifdef HAVE_DR_MP3
static bool audio_mixer_play_mp3(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   float ratio                     = 1.0f;
   unsigned samples                = 0;
   void *mp3_buffer                = NULL;
   void *resampler_data            = NULL;
   const retro_resampler_t* resamp = NULL;
   bool res;

   res = drmp3_init_memory(&voice->types.mp3.stream, (const unsigned char*)sound->types.mp3.data, sound->types.mp3.size, NULL);

   if (!res)
      return false;

   if (voice->types.mp3.stream.sampleRate != s_rate)
   {
      ratio = (double)s_rate / (double)(voice->types.mp3.stream.sampleRate);

      if (!retro_resampler_realloc(&resampler_data,
               &resamp, resampler_ident, quality,
               ratio))
         goto error;
   }

   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes. We
    * add 16 more samples in the formula below just as safeguard, because
    * resampler->process sometimes reports more output samples than the
    * formula below calculates. Ideally, audio resamplers should have a
    * function to return the number of samples they will output given a
    * count of input samples. */
   samples                         = (unsigned)(AUDIO_MIXER_TEMP_BUFFER * ratio);
   mp3_buffer                      = (float*)memalign_alloc(16,
         (((samples + 16) + 15) & ~15) * sizeof(float));

   if (!mp3_buffer)
   {
      if (resamp && resampler_data)
         resamp->free(resampler_data);
      goto error;
   }

   voice->types.mp3.resampler      = resamp;
   voice->types.mp3.resampler_data = resampler_data;
   voice->types.mp3.buffer         = (float*)mp3_buffer;
   voice->types.mp3.buf_samples    = samples;
   voice->types.mp3.ratio          = ratio;
   voice->types.mp3.position       = 0;
   voice->types.mp3.samples        = 0;

   return true;

error:
   drmp3_uninit(&voice->types.mp3.stream);
   return false;
}

static void audio_mixer_release_mp3(audio_mixer_voice_t* voice)
{
   if (voice->types.mp3.resampler && voice->types.mp3.resampler_data)
      voice->types.mp3.resampler->free(voice->types.mp3.resampler_data);
   if (voice->types.mp3.buffer)
      memalign_free(voice->types.mp3.buffer);
   if (voice->types.mp3.stream.pData)
      drmp3_uninit(&voice->types.mp3.stream);
}

#endif

audio_mixer_voice_t* audio_mixer_play(audio_mixer_sound_t* sound,
      bool repeat, float volume,
      const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   unsigned i;
   bool res                   = false;
   audio_mixer_voice_t* voice = s_voices;

   if (!sound)
      return NULL;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
   {
      if (voice->type != AUDIO_MIXER_TYPE_NONE)
         continue;

      AUDIO_MIXER_LOCK(voice);

      if (voice->type != AUDIO_MIXER_TYPE_NONE)
      {
         AUDIO_MIXER_UNLOCK(voice);
         continue;
      }

      /* claim the voice, also helps with cleanup on error */
      voice->type = sound->type;

      switch (sound->type)
      {
         case AUDIO_MIXER_TYPE_WAV:
            res = audio_mixer_play_wav(sound, voice, repeat, volume, stop_cb);
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_STB_VORBIS
            res = audio_mixer_play_ogg(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            res = audio_mixer_play_mod(sound, voice, repeat, volume, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
            res = audio_mixer_play_flac(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
            res = audio_mixer_play_mp3(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_NONE:
            break;
      }

      break;
   }

   if (res)
   {
      voice->repeat   = repeat;
      voice->volume   = volume;
      voice->sound    = sound;
      voice->stop_cb  = stop_cb;
      AUDIO_MIXER_UNLOCK(voice);
   }
   else
   {
      if (i < AUDIO_MIXER_MAX_VOICES)
      {
         audio_mixer_release(voice);
         AUDIO_MIXER_UNLOCK(voice);
      }
      voice = NULL;
   }

   return voice;
}

audio_mixer_voice_t* audio_mixer_play_s16(audio_mixer_sound_t* sound,
      bool repeat, float volume,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   unsigned i;
   bool res                   = false;
   audio_mixer_voice_t* voice = s_voices;

   if (!sound)
      return NULL;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
   {
      if (voice->type != AUDIO_MIXER_TYPE_NONE)
         continue;

      AUDIO_MIXER_LOCK(voice);

      if (voice->type != AUDIO_MIXER_TYPE_NONE)
      {
         AUDIO_MIXER_UNLOCK(voice);
         continue;
      }

      voice->type   = sound->type;
      voice->is_s16 = true;

      switch (sound->type)
      {
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
            res = audio_mixer_play_flac_s16(sound, voice, repeat, volume,
                  quality, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV:
         case AUDIO_MIXER_TYPE_OGG:
         case AUDIO_MIXER_TYPE_MOD:
         case AUDIO_MIXER_TYPE_MP3:
         case AUDIO_MIXER_TYPE_NONE:
            /* Remaining s16 voice types land in follow-up commits. */
            break;
      }

      break;
   }

   if (res)
   {
      voice->repeat   = repeat;
      voice->volume   = volume;
      voice->sound    = sound;
      voice->stop_cb  = stop_cb;
      AUDIO_MIXER_UNLOCK(voice);
   }
   else
   {
      if (i < AUDIO_MIXER_MAX_VOICES)
      {
         audio_mixer_release(voice);
         AUDIO_MIXER_UNLOCK(voice);
      }
      voice = NULL;
   }

   return voice;
}

/* Need to hold lock for voice.  */
static void audio_mixer_release(audio_mixer_voice_t* voice)
{
   if (!voice)
      return;

   switch (voice->type)
   {
#ifdef HAVE_STB_VORBIS
      case AUDIO_MIXER_TYPE_OGG:
         audio_mixer_release_ogg(voice);
         break;
#endif
#ifdef HAVE_IBXM
      case AUDIO_MIXER_TYPE_MOD:
         audio_mixer_release_mod(voice);
         break;
#endif
#ifdef HAVE_RFLAC
      case AUDIO_MIXER_TYPE_FLAC:
         audio_mixer_release_flac(voice);
         break;
#endif
#ifdef HAVE_DR_MP3
      case AUDIO_MIXER_TYPE_MP3:
         audio_mixer_release_mp3(voice);
         break;
#endif
      default:
         break;
   }

   memset(&voice->types, 0, sizeof(voice->types));
   voice->type   = AUDIO_MIXER_TYPE_NONE;
   voice->is_s16 = false;
}

void audio_mixer_stop(audio_mixer_voice_t* voice)
{
   audio_mixer_stop_cb_t stop_cb = NULL;
   audio_mixer_sound_t* sound    = NULL;

   if (voice)
   {
      AUDIO_MIXER_LOCK(voice);
      stop_cb     = voice->stop_cb;
      sound       = voice->sound;

      audio_mixer_release(voice);

      AUDIO_MIXER_UNLOCK(voice);

      if (stop_cb)
         stop_cb(sound, AUDIO_MIXER_SOUND_STOPPED);
   }
}

static void audio_mixer_mix_wav(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   const audio_mixer_sound_t* sound = voice->sound;
   unsigned pcm_available           = sound->types.wav.frames
      * 2 - voice->types.wav.position;
   const float* pcm                 = sound->types.wav.pcm +
      voice->types.wav.position;

again:
   if (pcm_available < buf_free)
   {
      for (i = pcm_available; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      if (voice->repeat)
      {
         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

         buf_free                  -= pcm_available;
         pcm_available              = sound->types.wav.frames * 2;
         pcm                        = sound->types.wav.pcm;
         voice->types.wav.position  = 0;
         goto again;
      }

      if (voice->stop_cb)
         voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

      audio_mixer_release(voice);
   }
   else
   {
      for (i = buf_free; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      voice->types.wav.position += buf_free;
   }
}

#ifdef HAVE_STB_VORBIS
static void audio_mixer_mix_ogg(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   float* temp_buffer = NULL;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float* pcm                       = NULL;

   if (!voice->types.ogg.stream)
      return;

   if (voice->types.ogg.position == voice->types.ogg.samples)
   {
again:
      if (temp_buffer == NULL)
         temp_buffer = (float*)malloc(AUDIO_MIXER_TEMP_BUFFER * sizeof(float));

      temp_samples = stb_vorbis_get_samples_float_interleaved(
            voice->types.ogg.stream, 2, temp_buffer,
            AUDIO_MIXER_TEMP_BUFFER) * 2;

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            stb_vorbis_seek_start(voice->types.ogg.stream);
            goto again;
         }

         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

         audio_mixer_release(voice);
         goto cleanup;
      }

      if (voice->types.ogg.resampler)
      {
         struct resampler_data info;
         info.data_in = temp_buffer;
         info.data_out = voice->types.ogg.buffer;
         info.input_frames = temp_samples / 2;
         info.output_frames = 0;
         info.ratio = voice->types.ogg.ratio;

         voice->types.ogg.resampler->process(
               voice->types.ogg.resampler_data, &info);
      }
      else
         memcpy(voice->types.ogg.buffer, temp_buffer,
               temp_samples * sizeof(float));

      voice->types.ogg.position = 0;
      voice->types.ogg.samples  = voice->types.ogg.buf_samples;
   }

   pcm = voice->types.ogg.buffer + voice->types.ogg.position;

   if (voice->types.ogg.samples < buf_free)
   {
      for (i = voice->types.ogg.samples; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      buf_free -= voice->types.ogg.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i )
      *buffer++ += *pcm++ * volume;

   voice->types.ogg.position += buf_free;
   voice->types.ogg.samples  -= buf_free;

cleanup:
   if (temp_buffer != NULL)
      free(temp_buffer);
}
#endif

#ifdef HAVE_IBXM
static void audio_mixer_mix_mod(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   float samplef                    = 0.0f;
   unsigned temp_samples            = 0;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   int* pcm                         = NULL;

   if (voice->types.mod.samples == 0)
   {
again:
      temp_samples = replay_get_audio(
            voice->types.mod.stream, voice->types.mod.buffer, 0 ) * 2;

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            replay_seek( voice->types.mod.stream, 0);
            goto again;
         }

         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

         audio_mixer_release(voice);
         return;
      }

      voice->types.mod.position = 0;
      voice->types.mod.samples  = temp_samples;
   }
   pcm = voice->types.mod.buffer + voice->types.mod.position;

   if (voice->types.mod.samples < buf_free)
   {
      /* ibxm emits fixed-point samples whose unity is FP_ONE (32768 ==
       * 0x8000), so scale by 1/0x8000 to match audio/conversion/
       * s16_to_float and the rest of the mixer. The previous
       * (s + 32768) / 65535 * 2 - 1 mapping added a small positive DC
       * offset (0 -> +1.5e-5) and a non-canonical scale; this keeps the
       * MOD voice consistent (and deterministic) with the other formats.
       * Samples beyond +/-FP_ONE (loud multi-channel mixes) exceed
       * +/-1.0 and are clamped downstream exactly as before. */
      for (i = voice->types.mod.samples; i != 0; i--)
      {
         samplef     = (float)(*pcm++) * (1.0f / 0x8000);
         *buffer++  += samplef * volume;
      }

      buf_free -= voice->types.mod.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i )
   {
      samplef     = (float)(*pcm++) * (1.0f / 0x8000);
      *buffer++  += samplef * volume;
   }

   voice->types.mod.position += buf_free;
   voice->types.mod.samples  -= buf_free;
}
#endif

#ifdef HAVE_RFLAC
static void audio_mixer_mix_flac(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   struct resampler_data info;
   float temp_buffer[AUDIO_MIXER_TEMP_BUFFER] = { 0 };
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float *pcm                       = NULL;

   if (voice->types.flac.position == voice->types.flac.samples)
   {
again:
      /* rflac_read_pcm_frames_f32 takes a frame count and
       * writes frame_count * channel_count floats into the
       * output buffer.  Request at most AUDIO_MIXER_TEMP_BUFFER
       * / 2 frames so a stereo FLAC fills temp_buffer[AUDIO_
       * MIXER_TEMP_BUFFER] exactly, then multiply the return
       * value by 2 to convert frame count to interleaved float
       * count.  This matches the convention the downstream code
       * (info.input_frames = temp_samples / 2, memcpy length
       * of temp_samples * sizeof(float)) and the sibling mp3
       * and ogg mix paths already use.
       *
       * Pre-patch this passed AUDIO_MIXER_TEMP_BUFFER as the
       * frame count without the '/ 2' and stored the return as
       * 'temp_samples' without the '* 2'.  For a stereo FLAC
       * (by far the most common case) rflac wrote 2 *
       * AUDIO_MIXER_TEMP_BUFFER = 16384 floats into a 8192-
       * float stack buffer - a 32 KiB stack overflow.  Any
       * stereo FLAC asset played through the mixer (cheevo
       * unlock sounds, menu BGM, user-loaded content via the
       * audio mixer playlist, etc.) corrupted the stack on
       * every mix tick.  The downstream 'temp_samples / 2'
       * and memcpy length were also off by 2x under the old
       * convention, but the stack overflow hit first.
       *
       * For mono FLAC: the '* 2' overstates the float count by
       * 2x, causing the memcpy / resampler to read
       * uninitialised stack past the actual data.  That matches
       * the pre-existing implicit stereo-only assumption of
       * the FLAC/MP3/OGG mixer paths (voice->types.flac.
       * buf_samples is sized as TEMP_BUFFER * ratio, with no
       * per-channel adjustment) rather than introducing new
       * mono handling here.  Fixing mono playback is a separate
       * change. */
      {
         size_t got = 0;
         audio_transfer_read_f32(voice->types.flac.stream, AUDIO_TYPE_FLAC,
               temp_buffer, AUDIO_MIXER_TEMP_BUFFER / 2, &got);
         temp_samples = (unsigned)(got * 2);
      }
      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            audio_transfer_seek(voice->types.flac.stream, AUDIO_TYPE_FLAC, 0);
            goto again;
         }

         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

         audio_mixer_release(voice);
         return;
      }

      info.data_in              = temp_buffer;
      info.data_out             = voice->types.flac.buffer;
      info.input_frames         = temp_samples / 2;
      info.output_frames        = 0;
      info.ratio                = voice->types.flac.ratio;

      if (voice->types.flac.resampler)
         voice->types.flac.resampler->process(
               voice->types.flac.resampler_data, &info);
      else
         memcpy(voice->types.flac.buffer, temp_buffer, temp_samples * sizeof(float));
      voice->types.flac.position = 0;
      voice->types.flac.samples  = voice->types.flac.buf_samples;
   }

   pcm = voice->types.flac.buffer + voice->types.flac.position;

   if (voice->types.flac.samples < buf_free)
   {
      for (i = voice->types.flac.samples; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      buf_free -= voice->types.flac.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i )
      *buffer++ += *pcm++ * volume;

   voice->types.flac.position += buf_free;
   voice->types.flac.samples  -= buf_free;
}

static void audio_mixer_mix_flac_s16(int16_t* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      int32_t gain_q16)
{
   int i;
   struct resampler_data_int16 info;
   int16_t  temp_buffer[AUDIO_MIXER_TEMP_BUFFER];
   unsigned buf_free     = (unsigned)(num_frames * 2);
   unsigned temp_samples = 0;
   int16_t *pcm          = NULL;

   if (voice->types.flac.position == voice->types.flac.samples)
   {
again:
      {
         size_t got = 0;
         audio_transfer_read_s16(voice->types.flac.stream, AUDIO_TYPE_FLAC,
               temp_buffer, AUDIO_MIXER_TEMP_BUFFER / 2, &got);
         temp_samples = (unsigned)(got * 2);
      }
      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);
            audio_transfer_seek(voice->types.flac.stream, AUDIO_TYPE_FLAC, 0);
            goto again;
         }
         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);
         audio_mixer_release(voice);
         return;
      }

      info.data_in       = temp_buffer;
      info.data_out      = voice->types.flac.buffer_s16;
      info.input_frames  = temp_samples / 2;
      info.output_frames = 0;
      info.ratio         = voice->types.flac.ratio;

      if (voice->types.flac.resampler_int16)
         sinc_resampler_int16_process(
               voice->types.flac.resampler_int16, &info);
      else
         memcpy(voice->types.flac.buffer_s16, temp_buffer,
               temp_samples * sizeof(int16_t));
      voice->types.flac.position = 0;
      voice->types.flac.samples  = voice->types.flac.buf_samples;
   }

   pcm = voice->types.flac.buffer_s16 + voice->types.flac.position;

   if (voice->types.flac.samples < buf_free)
   {
      for (i = voice->types.flac.samples; i != 0; i--)
      {
         *buffer = audio_mixer_sat_s16((int32_t)*buffer
               + audio_mixer_gain_s16(*pcm++, gain_q16));
         buffer++;
      }
      buf_free -= voice->types.flac.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i)
   {
      *buffer = audio_mixer_sat_s16((int32_t)*buffer
            + audio_mixer_gain_s16(*pcm++, gain_q16));
      buffer++;
   }

   voice->types.flac.position += buf_free;
   voice->types.flac.samples  -= buf_free;
}
#endif

#ifdef HAVE_DR_MP3
static void audio_mixer_mix_mp3(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume)
{
   int i;
   struct resampler_data info;
   float temp_buffer[AUDIO_MIXER_TEMP_BUFFER] = { 0 };
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float* pcm                       = NULL;

   if (voice->types.mp3.position == voice->types.mp3.samples)
   {
again:
      temp_samples = (unsigned)drmp3_read_f32(
            &voice->types.mp3.stream,
            AUDIO_MIXER_TEMP_BUFFER / 2, temp_buffer) * 2;

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            drmp3_seek_to_frame(&voice->types.mp3.stream,0);
            goto again;
         }

         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

         audio_mixer_release(voice);
         return;
      }

      info.data_in              = temp_buffer;
      info.data_out             = voice->types.mp3.buffer;
      info.input_frames         = temp_samples / 2;
      info.output_frames        = 0;
      info.ratio                = voice->types.mp3.ratio;

      if (voice->types.mp3.resampler)
         voice->types.mp3.resampler->process(
               voice->types.mp3.resampler_data, &info);
      else
         memcpy(voice->types.mp3.buffer, temp_buffer,
               temp_samples * sizeof(float));
      voice->types.mp3.position = 0;
      voice->types.mp3.samples  = voice->types.mp3.buf_samples;
   }

   pcm = voice->types.mp3.buffer + voice->types.mp3.position;

   if (voice->types.mp3.samples < buf_free)
   {
      for (i = voice->types.mp3.samples; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      buf_free -= voice->types.mp3.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i )
      *buffer++ += *pcm++ * volume;

   voice->types.mp3.position += buf_free;
   voice->types.mp3.samples  -= buf_free;
}
#endif

void audio_mixer_mix(float* buffer, size_t num_frames,
      float volume_override, bool override)
{
   unsigned i;
   size_t j                   = 0;
   float* sample              = NULL;
   audio_mixer_voice_t* voice = s_voices;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
   {
      float volume;

      AUDIO_MIXER_LOCK(voice);

      if (voice->is_s16)
      {
         AUDIO_MIXER_UNLOCK(voice);
         continue;
      }

      volume = (override) ? volume_override : voice->volume;

      switch (voice->type)
      {
         case AUDIO_MIXER_TYPE_WAV:
            audio_mixer_mix_wav(buffer, num_frames, voice, volume);
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_STB_VORBIS
            audio_mixer_mix_ogg(buffer, num_frames, voice, volume);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            audio_mixer_mix_mod(buffer, num_frames, voice, volume);
#endif
            break;
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
            audio_mixer_mix_flac(buffer, num_frames, voice, volume);
#endif
            break;
            case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
            audio_mixer_mix_mp3(buffer, num_frames, voice, volume);
#endif
            break;
         case AUDIO_MIXER_TYPE_NONE:
            break;
      }

      AUDIO_MIXER_UNLOCK(voice);
   }

   for (j = 0, sample = buffer; j < num_frames * 2; j++, sample++)
   {
      if (*sample < -1.0f)
         *sample = -1.0f;
      else if (*sample > 1.0f)
         *sample = 1.0f;
   }
}

void audio_mixer_mix_s16(int16_t* buffer, size_t num_frames,
      float volume_override, bool override)
{
   unsigned i;
   audio_mixer_voice_t* voice = s_voices;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
   {
      float   volume;
      int32_t gain_q16;

      AUDIO_MIXER_LOCK(voice);

      if (!voice->is_s16)
      {
         AUDIO_MIXER_UNLOCK(voice);
         continue;
      }

      volume   = (override) ? volume_override : voice->volume;
      gain_q16 = (int32_t)(volume * 65536.0f + 0.5f);

      switch (voice->type)
      {
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
            audio_mixer_mix_flac_s16(buffer, num_frames, voice, gain_q16);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV:
         case AUDIO_MIXER_TYPE_OGG:
         case AUDIO_MIXER_TYPE_MOD:
         case AUDIO_MIXER_TYPE_MP3:
         case AUDIO_MIXER_TYPE_NONE:
            /* Remaining s16 voice types land in follow-up commits. */
            break;
      }

      AUDIO_MIXER_UNLOCK(voice);
   }
   /* No final clamp: audio_mixer_mix_*_s16 saturate as they accumulate. */
}

float audio_mixer_voice_get_volume(audio_mixer_voice_t *voice)
{
   if (!voice)
      return 0.0f;

   return voice->volume;
}

void audio_mixer_voice_set_volume(audio_mixer_voice_t *voice, float val)
{
   if (!voice)
      return;

   AUDIO_MIXER_LOCK(voice);
   voice->volume = val;
   AUDIO_MIXER_UNLOCK(voice);
}
