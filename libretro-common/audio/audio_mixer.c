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

#ifdef HAVE_RVORBIS
#include <formats/audio.h>
#endif

#ifdef HAVE_RFLAC
#include <formats/audio.h>
#endif

#ifdef HAVE_RMP3
#include <formats/audio.h>
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
         const int16_t* pcm_s16;
         unsigned frames;
      } wav;

#if defined(HAVE_RVORBIS) || defined(HAVE_RFLAC) || defined(HAVE_RMP3)
      struct
      {
         /* shared streaming-codec source (OGG / FLAC / MP3) */
         const void* data;
         unsigned size;
      } stream;
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

#if defined(HAVE_RVORBIS) || defined(HAVE_RFLAC) || defined(HAVE_RMP3)
      /* Shared streaming-codec voice state (OGG / FLAC / MP3). The codec is
       * identified by voice->type and passed to audio_transfer as an
       * enum audio_type_enum; the bookkeeping is identical across them. */
      struct
      {
         void       *stream;
         void       *resampler_data;
         const retro_resampler_t *resampler;
         float      *buffer;
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         float       ratio;
         /* s16 pipeline (parallel; used when voice->is_s16) */
         int16_t    *buffer_s16;
         void       *resampler_int16;
      } stream;
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

#ifdef HAVE_RWAV
static bool wav_to_s16(const rwav_t* wav, int16_t** pcm, size_t len)
{
   size_t i;
   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes */
   int16_t *s = (int16_t*)memalign_alloc(16,
         ((len + 15) & ~15) * sizeof(int16_t));

   if (!s)
      return false;

   *pcm = s;

   /* Native s16 conversion (no float detour). 16-bit samples are copied
    * verbatim; 8-bit unsigned samples are centered and scaled to s16
    * ((u8 - 128) << 8, i.e. the same magnitude as wav_to_float's
    * (u8 - 128) / 128 mapped to full scale); mono is duplicated to
    * stereo, matching wav_to_float's channel handling. For the common
    * 16-bit stereo case this is a straight copy, so the s16 voice path
    * never touches float. */
   if (wav->bitspersample == 8)
   {
      const uint8_t *u8 = (const uint8_t*)wav->samples;

      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            int16_t v = (int16_t)(((int)*u8++ - 128) << 8);
            *s++      = v;
            *s++      = v;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            *s++ = (int16_t)(((int)*u8++ - 128) << 8);
            *s++ = (int16_t)(((int)*u8++ - 128) << 8);
         }
      }
   }
   else
   {
      const int16_t *s16 = (const int16_t*)wav->samples;

      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            int16_t v = *s16++;
            *s++      = v;
            *s++      = v;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--)
         {
            *s++ = *s16++;
            *s++ = *s16++;
         }
      }
   }

   return true;
}

static bool one_shot_resample_s16(const int16_t* in, size_t samples_in,
      unsigned rate, enum resampler_quality quality,
      int16_t** out, size_t* samples_out)
{
   struct resampler_data_int16 info;
   size_t alloc_samples;
   void  *re    = NULL;
   double ratio = (double)s_rate / (double)rate;

   re = sinc_resampler_int16_init((ratio < 1.0) ? ratio : 1.0,
         audio_mixer_i16_quality(quality));

   if (!re)
      return false;

   /* Size by the predicted output count plus a 16-sample safeguard, exactly
    * like one_shot_resample, so the s16 buffer carries the same frame count
    * as the float buffer and audio_mixer_sound.wav.frames stays valid for
    * both. The buffer is zeroed so any undershoot tail reads as silence. */
   *samples_out  = (size_t)(samples_in * ratio);
   alloc_samples = ((*samples_out + 16) + 15) & ~15;
   *out          = (int16_t*)memalign_alloc(16,
         alloc_samples * sizeof(int16_t));

   if (*out == NULL)
   {
      sinc_resampler_int16_free(re);
      return false;
   }

   memset(*out, 0, alloc_samples * sizeof(int16_t));

   info.data_in       = in;
   info.data_out      = *out;
   info.input_frames  = samples_in / 2;
   info.output_frames = 0;
   info.ratio         = ratio;

   sinc_resampler_int16_process(re, &info);
   sinc_resampler_int16_free(re);
   return true;
}
#endif

audio_mixer_sound_t* audio_mixer_load_wav(void *buffer, int32_t size,
      const char *resampler_ident, enum resampler_quality quality)
{
#ifdef HAVE_RWAV
   /* WAV data */
   rwav_t wav;
   /* WAV samples converted to float */
   float* pcm                 = NULL;
   size_t samples             = 0;
   /* WAV samples converted natively to s16 (parallel float-free path) */
   int16_t* pcm16             = NULL;
   size_t samples16           = 0;
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
   samples16     = samples;

   if (!wav_to_float(&wav, &pcm, samples))
      return NULL;

   if (!wav_to_s16(&wav, &pcm16, samples16))
   {
      memalign_free((void*)pcm);
      return NULL;
   }

   if (wav.samplerate != s_rate)
   {
      float* resampled           = NULL;
      int16_t* resampled16       = NULL;

      if (!one_shot_resample(pcm, samples, wav.samplerate,
            resampler_ident, quality,
            &resampled, &samples))
      {
         memalign_free((void*)pcm);
         memalign_free((void*)pcm16);
         return NULL;
      }

      memalign_free((void*)pcm);
      pcm = resampled;

      if (!one_shot_resample_s16(pcm16, samples16, wav.samplerate,
            quality, &resampled16, &samples16))
      {
         memalign_free((void*)pcm);
         memalign_free((void*)pcm16);
         return NULL;
      }

      memalign_free((void*)pcm16);
      pcm16 = resampled16;
   }

   sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
   {
      memalign_free((void*)pcm);
      memalign_free((void*)pcm16);
      return NULL;
   }

   sound->type              = AUDIO_MIXER_TYPE_WAV;
   sound->types.wav.frames  = (unsigned)(samples / 2);
   sound->types.wav.pcm     = pcm;
   sound->types.wav.pcm_s16 = pcm16;

   rwav_free(&wav);

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_ogg(void *buffer, int32_t size)
{
#ifdef HAVE_RVORBIS
   audio_mixer_sound_t* sound;

   if (!buffer || size <= 0)
      return NULL;

   sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_OGG;
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;

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
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_mp3(void *buffer, int32_t size)
{
#ifdef HAVE_RMP3
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_MP3;
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;

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
         handle = (void*)sound->types.wav.pcm_s16;
         if (handle)
            memalign_free(handle);
         break;
      case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_RVORBIS
         handle = (void*)sound->types.stream.data;
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
         handle = (void*)sound->types.stream.data;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
         handle = (void*)sound->types.stream.data;
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

#if defined(HAVE_RVORBIS) || defined(HAVE_RFLAC) || defined(HAVE_RMP3)
/* Shared streaming-codec path (OGG / FLAC / MP3). audio_transfer already
 * abstracts the codec, so one set of play/mix/release functions serves all
 * three; the caller passes the matching enum audio_type_enum. */
static bool audio_mixer_play_stream(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb,
      enum audio_type_enum type)
{
   unsigned rate                   = 0;
   float ratio                     = 1.0f;
   unsigned samples                = 0;
   void *sbuf                      = NULL;
   void *resampler_data            = NULL;
   const retro_resampler_t* resamp = NULL;
   void *xfer                      = audio_transfer_new(type);

   if (!xfer)
      return false;

   audio_transfer_set_buffer_ptr(xfer, type,
         (void*)sound->types.stream.data, sound->types.stream.size);

   if (!audio_transfer_start(xfer, type))
      goto error;

   audio_transfer_info(xfer, type, NULL, &rate, NULL);

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
   sbuf                            = (float*)memalign_alloc(16,
         (((samples + 16) + 15) & ~15) * sizeof(float));

   if (!sbuf)
   {
      if (resamp && resampler_data)
         resamp->free(resampler_data);
      goto error;
   }

   voice->types.stream.resampler      = resamp;
   voice->types.stream.resampler_data = resampler_data;
   voice->types.stream.buffer         = (float*)sbuf;
   voice->types.stream.buf_samples    = samples;
   voice->types.stream.ratio          = ratio;
   voice->types.stream.stream         = xfer;
   voice->types.stream.position       = 0;
   voice->types.stream.samples        = 0;

   return true;

error:
   audio_transfer_free(xfer, type);
   return false;
}

static void audio_mixer_release_stream(audio_mixer_voice_t* voice,
      enum audio_type_enum type)
{
   if (voice->types.stream.stream)
      audio_transfer_free(voice->types.stream.stream, type);
   if (voice->types.stream.resampler && voice->types.stream.resampler_data)
      voice->types.stream.resampler->free(voice->types.stream.resampler_data);
   if (voice->types.stream.buffer)
      memalign_free(voice->types.stream.buffer);
   if (voice->types.stream.buffer_s16)
      memalign_free(voice->types.stream.buffer_s16);
   if (voice->types.stream.resampler_int16)
      sinc_resampler_int16_free(voice->types.stream.resampler_int16);
}

static bool audio_mixer_play_stream_s16(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, float volume,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb,
      enum audio_type_enum type)
{
   double   ratio       = 1.0;
   unsigned samples     = 0;
   unsigned channels    = 0;
   unsigned rate        = 0;
   void    *sbuf        = NULL;
   void    *resamp_i16  = NULL;
   void    *xfer        = audio_transfer_new(type);
   (void)repeat;
   (void)volume;
   (void)stop_cb;

   if (!xfer)
      return false;
   audio_transfer_set_buffer_ptr(xfer, type,
         (void*)sound->types.stream.data, sound->types.stream.size);
   if (!audio_transfer_start(xfer, type))
   {
      audio_transfer_free(xfer, type);
      return false;
   }
   audio_transfer_info(xfer, type, &channels, &rate, NULL);

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
   sbuf        = memalign_alloc(16,
         (((samples + 16) + 15) & ~15) * sizeof(int16_t));

   if (!sbuf)
   {
      if (resamp_i16)
         sinc_resampler_int16_free(resamp_i16);
      goto error;
   }

   voice->types.stream.resampler       = NULL;
   voice->types.stream.resampler_data  = NULL;
   voice->types.stream.buffer          = NULL;
   voice->types.stream.resampler_int16 = resamp_i16;
   voice->types.stream.buffer_s16      = (int16_t*)sbuf;
   voice->types.stream.buf_samples     = samples;
   voice->types.stream.ratio           = (float)ratio;
   voice->types.stream.stream          = xfer;
   voice->types.stream.position        = 0;
   voice->types.stream.samples         = 0;

   return true;

error:
   audio_transfer_free(xfer, type);
   return false;
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
#ifdef HAVE_RVORBIS
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_VORBIS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            res = audio_mixer_play_mod(sound, voice, repeat, volume, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_FLAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_MP3);
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
            res = audio_mixer_play_stream_s16(sound, voice, repeat, volume,
                  quality, stop_cb, AUDIO_TYPE_FLAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_RVORBIS
            res = audio_mixer_play_stream_s16(sound, voice, repeat, volume,
                  quality, stop_cb, AUDIO_TYPE_VORBIS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
            res = audio_mixer_play_stream_s16(sound, voice, repeat, volume,
                  quality, stop_cb, AUDIO_TYPE_MP3);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            res = audio_mixer_play_mod(sound, voice, repeat, volume, stop_cb);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV:
            res = audio_mixer_play_wav(sound, voice, repeat, volume, stop_cb);
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

/* Need to hold lock for voice.  */
static void audio_mixer_release(audio_mixer_voice_t* voice)
{
   if (!voice)
      return;

   switch (voice->type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_MIXER_TYPE_OGG:
         audio_mixer_release_stream(voice, AUDIO_TYPE_VORBIS);
         break;
#endif
#ifdef HAVE_IBXM
      case AUDIO_MIXER_TYPE_MOD:
         audio_mixer_release_mod(voice);
         break;
#endif
#ifdef HAVE_RFLAC
      case AUDIO_MIXER_TYPE_FLAC:
         audio_mixer_release_stream(voice, AUDIO_TYPE_FLAC);
         break;
#endif
#ifdef HAVE_RMP3
      case AUDIO_MIXER_TYPE_MP3:
         audio_mixer_release_stream(voice, AUDIO_TYPE_MP3);
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

static void audio_mixer_mix_wav_s16(int16_t* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      int32_t gain_q16)
{
   int i;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   const audio_mixer_sound_t* sound = voice->sound;
   unsigned pcm_available           = sound->types.wav.frames
      * 2 - voice->types.wav.position;
   const int16_t* pcm               = sound->types.wav.pcm_s16 +
      voice->types.wav.position;

again:
   if (pcm_available < buf_free)
   {
      for (i = pcm_available; i != 0; i--)
      {
         *buffer = audio_mixer_sat_s16((int32_t)*buffer
               + audio_mixer_gain_s16(*pcm++, gain_q16));
         buffer++;
      }

      if (voice->repeat)
      {
         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

         buf_free                  -= pcm_available;
         pcm_available              = sound->types.wav.frames * 2;
         pcm                        = sound->types.wav.pcm_s16;
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
      {
         *buffer = audio_mixer_sat_s16((int32_t)*buffer
               + audio_mixer_gain_s16(*pcm++, gain_q16));
         buffer++;
      }

      voice->types.wav.position += buf_free;
   }
}

#if defined(HAVE_RVORBIS) || defined(HAVE_RFLAC) || defined(HAVE_RMP3)
static void audio_mixer_mix_stream(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume,
      enum audio_type_enum type)
{
   int i;
   float* temp_buffer = NULL;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float* pcm                       = NULL;

   if (!voice->types.stream.stream)
      return;

   if (voice->types.stream.samples == 0)
   {
again:
      if (temp_buffer == NULL)
         temp_buffer = (float*)malloc(AUDIO_MIXER_TEMP_BUFFER * sizeof(float));

      {
         size_t got = 0;
         audio_transfer_read_f32(voice->types.stream.stream, type,
               temp_buffer, AUDIO_MIXER_TEMP_BUFFER / 2, &got);
         temp_samples = (unsigned)(got * 2);
      }

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);

            audio_transfer_seek(voice->types.stream.stream, type, 0);
            goto again;
         }

         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);

         audio_mixer_release(voice);
         goto cleanup;
      }

      if (voice->types.stream.resampler)
      {
         struct resampler_data info;
         info.data_in = temp_buffer;
         info.data_out = voice->types.stream.buffer;
         info.input_frames = temp_samples / 2;
         info.output_frames = 0;
         info.ratio = voice->types.stream.ratio;

         voice->types.stream.resampler->process(
               voice->types.stream.resampler_data, &info);
         voice->types.stream.samples = (unsigned)(info.output_frames * 2);
      }
      else
      {
         memcpy(voice->types.stream.buffer, temp_buffer,
               temp_samples * sizeof(float));
         voice->types.stream.samples = temp_samples;
      }

      voice->types.stream.position = 0;
   }

   pcm = voice->types.stream.buffer + voice->types.stream.position;

   if (voice->types.stream.samples < buf_free)
   {
      for (i = voice->types.stream.samples; i != 0; i--)
         *buffer++ += *pcm++ * volume;

      buf_free -= voice->types.stream.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i )
      *buffer++ += *pcm++ * volume;

   voice->types.stream.position += buf_free;
   voice->types.stream.samples  -= buf_free;

cleanup:
   if (temp_buffer != NULL)
      free(temp_buffer);
}

static void audio_mixer_mix_stream_s16(int16_t* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      int32_t gain_q16,
      enum audio_type_enum type)
{
   int i;
   struct resampler_data_int16 info;
   int16_t  temp_buffer[AUDIO_MIXER_TEMP_BUFFER];
   unsigned buf_free     = (unsigned)(num_frames * 2);
   unsigned temp_samples = 0;
   int16_t *pcm          = NULL;

   if (!voice->types.stream.stream)
      return;

   if (voice->types.stream.samples == 0)
   {
again:
      {
         size_t got = 0;
         audio_transfer_read_s16(voice->types.stream.stream, type,
               temp_buffer, AUDIO_MIXER_TEMP_BUFFER / 2, &got);
         temp_samples = (unsigned)(got * 2);
      }
      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);
            audio_transfer_seek(voice->types.stream.stream, type, 0);
            goto again;
         }
         if (voice->stop_cb)
            voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_FINISHED);
         audio_mixer_release(voice);
         return;
      }

      info.data_in       = temp_buffer;
      info.data_out      = voice->types.stream.buffer_s16;
      info.input_frames  = temp_samples / 2;
      info.output_frames = 0;
      info.ratio         = voice->types.stream.ratio;

      if (voice->types.stream.resampler_int16)
      {
         sinc_resampler_int16_process(
               voice->types.stream.resampler_int16, &info);
         voice->types.stream.samples = (unsigned)(info.output_frames * 2);
      }
      else
      {
         memcpy(voice->types.stream.buffer_s16, temp_buffer,
               temp_samples * sizeof(int16_t));
         voice->types.stream.samples = temp_samples;
      }
      voice->types.stream.position = 0;
   }

   pcm = voice->types.stream.buffer_s16 + voice->types.stream.position;

   if (voice->types.stream.samples < buf_free)
   {
      for (i = voice->types.stream.samples; i != 0; i--)
      {
         *buffer = audio_mixer_sat_s16((int32_t)*buffer
               + audio_mixer_gain_s16(*pcm++, gain_q16));
         buffer++;
      }
      buf_free -= voice->types.stream.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i)
   {
      *buffer = audio_mixer_sat_s16((int32_t)*buffer
            + audio_mixer_gain_s16(*pcm++, gain_q16));
      buffer++;
   }

   voice->types.stream.position += buf_free;
   voice->types.stream.samples  -= buf_free;
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

static void audio_mixer_mix_mod_s16(int16_t* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      int32_t gain_q16)
{
   int i;
   unsigned temp_samples = 0;
   unsigned buf_free     = (unsigned)(num_frames * 2);
   int     *pcm          = NULL;

   if (voice->types.mod.samples == 0)
   {
again:
      temp_samples = replay_get_audio(
            voice->types.mod.stream, voice->types.mod.buffer, 0) * 2;

      if (temp_samples == 0)
      {
         if (voice->repeat)
         {
            if (voice->stop_cb)
               voice->stop_cb(voice->sound, AUDIO_MIXER_SOUND_REPEATED);
            replay_seek(voice->types.mod.stream, 0);
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

   /* ibxm emits fixed-point samples whose unity is FP_ONE (0x8000),
    * which is exactly the s16 full-scale magnitude, so the rendered
    * int sample needs no rescale: clamp it into s16 range (loud
    * multi-channel mixes can exceed +/-FP_ONE, matching the downstream
    * clamp on the float path), apply the Q16 gain, and accumulate with
    * saturation.  No int16<->float round-trip and no resampler -- ibxm
    * already renders at the mixer rate. */
   if (voice->types.mod.samples < buf_free)
   {
      for (i = voice->types.mod.samples; i != 0; i--)
      {
         *buffer = audio_mixer_sat_s16((int32_t)*buffer
               + audio_mixer_gain_s16(audio_mixer_sat_s16(*pcm++), gain_q16));
         buffer++;
      }
      buf_free -= voice->types.mod.samples;
      goto again;
   }

   for (i = buf_free; i != 0; --i)
   {
      *buffer = audio_mixer_sat_s16((int32_t)*buffer
            + audio_mixer_gain_s16(audio_mixer_sat_s16(*pcm++), gain_q16));
      buffer++;
   }

   voice->types.mod.position += buf_free;
   voice->types.mod.samples  -= buf_free;
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
#ifdef HAVE_RVORBIS
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_VORBIS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            audio_mixer_mix_mod(buffer, num_frames, voice, volume);
#endif
            break;
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_FLAC);
#endif
            break;
            case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_MP3);
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
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_FLAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_RVORBIS
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_VORBIS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_MP3);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_IBXM
            audio_mixer_mix_mod_s16(buffer, num_frames, voice, gain_q16);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV:
            audio_mixer_mix_wav_s16(buffer, num_frames, voice, gain_q16);
            break;
         case AUDIO_MIXER_TYPE_NONE:
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
