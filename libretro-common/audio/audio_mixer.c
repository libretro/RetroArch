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

/* Which builds have stream voices - the shared play/mix/release path
 * over audio_transfer - asked in one place rather than spelled out at
 * each of the six sites that need it.  WAV joined the set when it
 * gained a streaming sound type, and a console build carrying WAV and
 * no compressed codec is exactly the configuration that finds a site
 * left behind. */
#if defined(HAVE_RWAV) || defined(HAVE_RVORBIS) || defined(HAVE_RFLAC) \
 || defined(HAVE_RMP3) || defined(HAVE_RMODTRACKER) || defined(HAVE_RAAC) \
 || defined(HAVE_ROPUS)
#define AUDIO_MIXER_HAS_STREAM 1
#include <formats/audio.h>
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
         /* Frame counts are kept per pipeline. The float and the
          * fixed-point resampler each report what they actually
          * produced, and nothing guarantees the two agree to the
          * sample; one shared count would let the s16 mixer read past
          * the end of a shorter buffer. */
         unsigned frames;
         unsigned frames_s16;
#ifdef HAVE_RWAV
         /* The WAV's own sample bytes, owned by the sound. Kept so
          * that whichever pipeline is asked for can be built from the
          * source, rather than derived from the other pipeline's
          * finished output - an s16 voice must never end up playing
          * audio that was resampled in float and quantised after. */
         uint8_t* src;
         rwav_t   hdr;
#endif
      } wav;

#ifdef AUDIO_MIXER_HAS_STREAM
      struct
      {
         /* shared streaming source (WAV / OGG / FLAC / MP3 / ...) */
         const void* data;
         unsigned size;
      } stream;
#endif


   } types;
   /* Borrowed compressed data: when owner is set, destroy releases
    * the source through release(owner) instead of free()ing it -
    * the load's caller lent the bytes from inside a larger owned
    * object (a file mapping, a data_transfer) and no copy was made. */
   void  *data_owner;
   void (*data_release)(void *owner);
   /* Windowed Ogg-Opus: the last-page granule supplied by the feeder,
    * injected into the decoder at play so it skips the full-file end
    * scan.  0 = not supplied (normal full scan). */
   int64_t end_granule;
   /* Windowed sources: bytes resident from the start of the buffer at
    * play time, bounding the decoder's header parse.  0 = all of it. */
   size_t avail;
};

struct audio_mixer_voice
{
   struct
   {
      struct
      {
         unsigned position;
      } wav;

#ifdef AUDIO_MIXER_HAS_STREAM
      /* Shared streaming voice state (WAV / OGG / FLAC / MP3). The codec is
       * identified by voice->type and passed to audio_transfer as an
       * enum audio_type_enum; the bookkeeping is identical across them. */
      struct
      {
         void       *stream;
         void       *resampler_data;
         const retro_resampler_t *resampler;
         float      *buffer;
         /* Decode scratch for the float mix path.  Owned by the voice
          * because the mix runs on the audio thread, where a malloc is
          * a stall waiting to happen. */
         float      *decode_buf;
         unsigned    position;
         unsigned    samples;
         unsigned    buf_samples;
         unsigned    channels;    /* source channels; mono is duplicated */
         /* Both resampler APIs take the ratio as a double, and the s16
          * path computes it as one. Holding it as a float here meant
          * that value was narrowed on the way in and widened again on
          * the way out, losing precision for nothing. The float path
          * computes a float and widens exactly, so it is unaffected. */
         double      ratio;
         /* s16 pipeline (parallel; used when voice->is_s16) */
         int16_t    *buffer_s16;
         /* Decode scratch, mirroring decode_buf above.  This lived on
          * the mix function's stack as int16_t[AUDIO_MIXER_TEMP_BUFFER]
          * (16 KB): a third of a 48 KB L1d evicted per voice per flush,
          * and past the stack-probe threshold on Windows, so every call
          * paid a chkstk page walk before doing any work. */
         int16_t    *decode_buf_s16;
         void       *resampler_int16;
      } stream;
#endif


   } types;
   audio_mixer_sound_t *sound;
   audio_mixer_stop_cb_t stop_cb;
   unsigned type;
   /* volume drives the float pipeline, gain the s16 one. They are held
    * separately rather than converted on demand so that an s16 voice
    * never needs a float operation on the audio thread. */
   float    volume;
   int32_t  gain;
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

#ifdef AUDIO_MIXER_HAS_STREAM
/* ---- folding a multichannel stream to the stereo a voice mixes ------
 *
 * Vorbis and Opus both decode more than two channels now, and so do
 * WAV and FLAC - but they do not agree on what order the channels
 * arrive in, so there are two tables and the caller says which.
 *
 * Vorbis's order (spec 4.3.9), which Opus mapping family 1 adopts:
 *
 *   3  L C R          4  L R RL RR        5  L C R RL RR
 *   6  L C R RL RR LFE                    7  L C R RL RR BC LFE
 *   8  L C R RL RR SL SR LFE
 *
 * Microsoft's, which WAV carries and FLAC's spec matches:
 *
 *   3  FL FR FC       4  FL FR BL BR      5  FL FR FC BL BR
 *   6  FL FR FC LFE BL BR                 7  FL FR FC LFE BC SL SR
 *   8  FL FR FC LFE BL BR SL SR
 *
 * Folding one with the other's table is not a subtle error: on 5.1 it
 * sends front-right to both sides, front-centre to the right alone,
 * LFE into the mix and back-right nowhere.
 *
 * A stream voice mixes stereo, so anything wider is folded here - the
 * arms deliberately do no channel conversion, leaving it to the
 * mixer.  Coefficients are the usual ITU-R BS.775 ones: centre and
 * surrounds enter both sides at -3 dB, a lone back centre at -6 dB
 * into each, and LFE is dropped, which is what it is for.
 *
 * Not normalised.  Scaling by the coefficient sum would put 5.1
 * content 7.7 dB below the same material in a stereo file, which is a
 * surprising thing for a file's channel count to do to its volume;
 * the sum only reaches full scale when the channels are correlated,
 * and the mixer already clamps at its s16 and float boundaries.  The
 * trade is level for the possibility of clipping on correlated
 * material, and it is the trade most players make.
 *
  * Q15 in int32 - unity is 32768, which does not fit the int16 the
 * coefficients would otherwise want, and wrapping it would invert a
 * channel rather than pass it.  The s16 fold stays clear of float. */
#define AMIX_DM_UNITY 32768
#define AMIX_DM_M3DB  23170
#define AMIX_DM_M6DB  16384

static int16_t audio_mixer_sat_s16_64(int64_t v)
{
   if (v >  32767)
      return  32767;
   if (v < -32768)
      return -32768;
   return (int16_t)v;
}

/* [channels][input channel][0 = left, 1 = right] */
static const int32_t audio_mixer_downmix_vorbis_q15[9][8][2] = {
   { {0,0} },                                              /* unused    */
   { {0,0} },                                              /* mono      */
   { {0,0} },                                              /* stereo    */
   /* 3: L C R */
   { {AMIX_DM_UNITY,0}, {AMIX_DM_M3DB,AMIX_DM_M3DB}, {0,AMIX_DM_UNITY} },
   /* 4: L R RL RR */
   { {AMIX_DM_UNITY,0}, {0,AMIX_DM_UNITY},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB} },
   /* 5: L C R RL RR */
   { {AMIX_DM_UNITY,0}, {AMIX_DM_M3DB,AMIX_DM_M3DB}, {0,AMIX_DM_UNITY},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB} },
   /* 6: L C R RL RR LFE */
   { {AMIX_DM_UNITY,0}, {AMIX_DM_M3DB,AMIX_DM_M3DB}, {0,AMIX_DM_UNITY},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB}, {0,0} },
   /* 7: L C R RL RR BC LFE */
   { {AMIX_DM_UNITY,0}, {AMIX_DM_M3DB,AMIX_DM_M3DB}, {0,AMIX_DM_UNITY},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB},
     {AMIX_DM_M6DB,AMIX_DM_M6DB}, {0,0} },
   /* 8: L C R RL RR SL SR LFE */
   { {AMIX_DM_UNITY,0}, {AMIX_DM_M3DB,AMIX_DM_M3DB}, {0,AMIX_DM_UNITY},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB}, {0,0} }
};


/* Microsoft/FLAC order. */
static const int32_t audio_mixer_downmix_wav_q15[9][8][2] = {
   { {0,0} },                                              /* unused    */
   { {0,0} },                                              /* mono      */
   { {0,0} },                                              /* stereo    */
   /* 3: FL FR FC */
   { {AMIX_DM_UNITY,0}, {0,AMIX_DM_UNITY}, {AMIX_DM_M3DB,AMIX_DM_M3DB} },
   /* 4: FL FR BL BR */
   { {AMIX_DM_UNITY,0}, {0,AMIX_DM_UNITY},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB} },
   /* 5: FL FR FC BL BR */
   { {AMIX_DM_UNITY,0}, {0,AMIX_DM_UNITY}, {AMIX_DM_M3DB,AMIX_DM_M3DB},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB} },
   /* 6: FL FR FC LFE BL BR */
   { {AMIX_DM_UNITY,0}, {0,AMIX_DM_UNITY}, {AMIX_DM_M3DB,AMIX_DM_M3DB},
     {0,0}, {AMIX_DM_M3DB,0}, {0,AMIX_DM_M3DB} },
   /* 7: FL FR FC LFE BC SL SR */
   { {AMIX_DM_UNITY,0}, {0,AMIX_DM_UNITY}, {AMIX_DM_M3DB,AMIX_DM_M3DB},
     {0,0}, {AMIX_DM_M6DB,AMIX_DM_M6DB},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB} },
   /* 8: FL FR FC LFE BL BR SL SR */
   { {AMIX_DM_UNITY,0}, {0,AMIX_DM_UNITY}, {AMIX_DM_M3DB,AMIX_DM_M3DB},
     {0,0}, {AMIX_DM_M3DB,0}, {0,AMIX_DM_M3DB},
     {AMIX_DM_M3DB,0},  {0,AMIX_DM_M3DB} }
};

/* Which order an arm hands its channels over in. */
static const int32_t (*audio_mixer_downmix_table(
      enum audio_type_enum type, unsigned ch))[2]
{
   switch (type)
   {
      case AUDIO_TYPE_VORBIS:
      case AUDIO_TYPE_OPUS:
         return audio_mixer_downmix_vorbis_q15[ch];
      default:
         break;
   }
   return audio_mixer_downmix_wav_q15[ch];
}

/* Frames a temp buffer of 'samples' holds at 'ch' channels: what a
 * read may ask for, which is not the stereo figure once ch exceeds
 * two. */
static size_t audio_mixer_frames_for(unsigned ch, size_t samples)
{
   return ch ? samples / ch : 0;
}

/* Fold 'frames' interleaved frames of 'ch' channels down to stereo,
 * in place and ascending: the destination index never overtakes the
 * source while ch is at least two. */
static void audio_mixer_downmix_s16(int16_t *buf, size_t frames,
      unsigned ch, const int32_t (*co)[2])
{
   size_t i;
   for (i = 0; i < frames; i++)
   {
      /* 64-bit: a full-scale sample times unity is already 2^30, and
       * eight channels of it summed reaches 5.9e9 - an int32 would
       * wrap on loud surround rather than clamp. */
      int64_t  l = 0, r = 0;
      unsigned c;
      for (c = 0; c < ch; c++)
      {
         int32_t v = buf[i * ch + c];
         l += (int64_t)v * co[c][0];
         r += (int64_t)v * co[c][1];
      }
      /* Round half away from zero rather than truncating, which the
       * shift alone would do and which costs half an LSB of bias. */
      buf[2 * i]     = audio_mixer_sat_s16_64(
            (l >= 0 ? l + 16384 : l - 16384) >> 15);
      buf[2 * i + 1] = audio_mixer_sat_s16_64(
            (r >= 0 ? r + 16384 : r - 16384) >> 15);
   }
}

static void audio_mixer_downmix_f32(float *buf, size_t frames,
      unsigned ch, const int32_t (*co)[2])
{
   size_t i;
   for (i = 0; i < frames; i++)
   {
      float    l = 0.0f, r = 0.0f;
      unsigned c;
      for (c = 0; c < ch; c++)
      {
         float v = buf[i * ch + c];
         l += v * (float)co[c][0] * (1.0f / 32768.0f);
         r += v * (float)co[c][1] * (1.0f / 32768.0f);
      }
      buf[2 * i]     = l;
      buf[2 * i + 1] = r;
   }
}
#endif /* AUDIO_MIXER_HAS_STREAM */

#ifdef HAVE_RWAV
/* One sample of a WAV frame, whatever width the file stores, in the
 * unit scale the float pipeline mixes at.  Only the multichannel fold
 * below needs it; the one- and two-channel paths stay specialised. */
static float wav_sample_unit(const rwav_t *wav, const uint8_t *src,
      size_t frame, unsigned c)
{
   size_t         w = (size_t)wav->bitspersample / 8;
   const uint8_t *p = src + (frame * wav->numchannels + c) * w;
   switch (wav->bitspersample)
   {
      case 32:
         return rwav_f32(p);
      case 24:
         return rwav_s24_to_float(p);
      case 8:
         return ((float)*p - 128.0f) / 128.0f;
      default:
         return (float)rwav_s16(p) / 32768.0f;
   }
}

/* Fold a WAV of more than two channels straight to the stereo the
 * mixer wants.  Without this the converters below, which only know
 * one channel and two, leave the output buffer untouched - and it
 * comes from memalign_alloc, so what played was uninitialised memory
 * rather than even silence. */
static void wav_fold_to_stereo(const rwav_t *wav, const uint8_t *src,
      float *dst, size_t frames)
{
   const int32_t (*co)[2] = audio_mixer_downmix_wav_q15[wav->numchannels];
   size_t i;
   for (i = 0; i < frames; i++)
   {
      float    l = 0.0f, r = 0.0f;
      unsigned c;
      for (c = 0; c < wav->numchannels; c++)
      {
         float v = wav_sample_unit(wav, src, i, c);
         l += v * (float)co[c][0] * (1.0f / 32768.0f);
         r += v * (float)co[c][1] * (1.0f / 32768.0f);
      }
      dst[2 * i]     = l;
      dst[2 * i + 1] = r;
   }
}

static bool wav_to_float(const rwav_t* wav, const uint8_t* src,
      float** pcm, size_t len)
{
   size_t i;
   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes */
   float *f           = (float*)memalign_alloc(16,
         ((len + 15) & ~15) * sizeof(float));

   if (!f)
      return false;

   *pcm = f;

   /* The companded and block-coded payloads are not readable a sample
    * at a time - rwav decodes those, natively to s16, and the fold or
    * the scale below works from that.  Everything else is read in
    * place as it always was. */
   if (wav->format != RWAV_FORMAT_PCM && wav->format != RWAV_FORMAT_FLOAT)
   {
      rwav_t   h   = *wav;
      int16_t *tmp = (int16_t*)malloc(wav->numsamples
            * wav->numchannels * sizeof(int16_t));
      size_t   i, got;
      h.dataoffset = 0;
      if (!tmp)
      {
         memalign_free(f);
         *pcm = NULL;
         return false;
      }
      got = rwav_decode_s16(&h, src, 0, wav->numsamples, tmp);
      if (wav->numchannels == 1)
         for (i = 0; i < got; i++)
         {
            f[2 * i]     = (float)tmp[i] * (1.0f / 32768.0f);
            f[2 * i + 1] = f[2 * i];
         }
      else if (wav->numchannels == 2)
         for (i = 0; i < got * 2; i++)
            f[i] = (float)tmp[i] * (1.0f / 32768.0f);
      else
      {
         const int32_t (*co)[2] =
               audio_mixer_downmix_wav_q15[wav->numchannels];
         unsigned c;
         for (i = 0; i < got; i++)
         {
            float l = 0.0f, r = 0.0f;
            for (c = 0; c < wav->numchannels; c++)
            {
               float v = (float)tmp[i * wav->numchannels + c]
                       * (1.0f / 32768.0f);
               l += v * (float)co[c][0] * (1.0f / 32768.0f);
               r += v * (float)co[c][1] * (1.0f / 32768.0f);
            }
            f[2 * i]     = l;
            f[2 * i + 1] = r;
         }
      }
      free(tmp);
      return true;
   }

   /* More channels than the specialised paths below know: fold. */
   if (wav->numchannels > 2 && wav->numchannels <= 8)
   {
      wav_fold_to_stereo(wav, src, f, wav->numsamples);
      return true;
   }
   if (wav->numchannels < 1 || wav->numchannels > 8)
   {
      memalign_free(f);
      *pcm = NULL;
      return false;
   }

   /* Canonical PCM->float scaling, matching audio/conversion/s16_to_float
    * (s16 / 0x8000) and audio_mix's 8-bit path ((u8 - 128) / 128). The
    * previous (s + 32768) / 65535 * 2 - 1 mapping introduced a small
    * positive DC offset (0 -> +1.5e-5) and a non-canonical scale; using
    * the same factor as the rest of the pipeline keeps the mixer's float
    * representation consistent (and the result deterministic) across the
    * s16/float boundaries the voices are mixed and clamped at. The mono
    * channel-duplication below is why the audio/conversion helpers can't
    * be called verbatim here.
    *
    * src points into the caller's file bytes, so every read goes
    * through rwav.h's accessors rather than a typed pointer: the data
    * is little-endian whatever the host is, and a data chunk is only
    * guaranteed even-aligned. */
   if (wav->bitspersample == 32)
   {
      /* IEEE-float samples are already in the mixer's own unit scale:
       * pass them through (duplicating mono), converting nothing -
       * this is the quantisation-free path float producers target. */
      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--, src += 4)
         {
            float sample = rwav_f32(src);
            *f++ = sample;
            *f++ = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--, src += 8)
         {
            *f++ = rwav_f32(src);
            *f++ = rwav_f32(src + 4);
         }
      }
   }
   else if (wav->bitspersample == 8)
   {
      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--, src++)
         {
            float sample = ((float)*src - 128.0f) * (1.0f / 128.0f);
            *f++         = sample;
            *f++         = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--, src += 2)
         {
            *f++ = ((float)src[0] - 128.0f) * (1.0f / 128.0f);
            *f++ = ((float)src[1] - 128.0f) * (1.0f / 128.0f);
         }
      }
   }
   else if (wav->bitspersample == 24)
   {
      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--, src += 3)
         {
            float sample = rwav_s24_to_float(src);
            *f++         = sample;
            *f++         = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--, src += 6)
         {
            *f++ = rwav_s24_to_float(src);
            *f++ = rwav_s24_to_float(src + 3);
         }
      }
   }
   else
   {
      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--, src += 2)
         {
            float sample = (float)rwav_s16(src) * (1.0f / 0x8000);
            *f++         = sample;
            *f++         = sample;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--, src += 4)
         {
            *f++ = (float)rwav_s16(src)     * (1.0f / 0x8000);
            *f++ = (float)rwav_s16(src + 2) * (1.0f / 0x8000);
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

   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes.
    * The pad has to scale with the ratio: resampler->process reports
    * more output than the estimate below, and the excess grows with
    * upsampling, so a fixed pad is enough at 44.1->48 and not enough
    * at 8->96, where the clamp underneath silently took the tail back
    * off again. Ideally, audio resamplers should have a function to
    * return the number of samples they will output given a count of
    * input samples. */
   {
      size_t alloc_samples;
      size_t pad                      = 2 * (size_t)(ratio + 1.0) + 32;
      *samples_out                    = (size_t)(samples_in * ratio);
      alloc_samples                   = ((*samples_out + pad) + 15) & ~15;
      *out                            = (float*)memalign_alloc(16,
            alloc_samples * sizeof(float));

      if (*out == NULL)
         return false;

      info.data_in                    = in;
      info.data_out                   = *out;
      info.input_frames               = samples_in / 2;
      info.output_frames              = 0;
      info.ratio                      = ratio;

      resampler->process(data, &info);
      resampler->free(data);

      /* Take the count the resampler reports rather than the estimate
       * it was sized by.  The estimate truncates, so the last frames
       * the filter had to give were being left in the buffer and not
       * counted - a resampled sound ended a fraction early, and did not
       * match the same audio played through a stream voice, which does
       * hand them over.  The 16-sample pad above is what the overshoot
       * the comment describes lands in; clamp to it. */
      *samples_out                    = info.output_frames * 2;
      if (*samples_out > alloc_samples)
         *samples_out                 = alloc_samples;
   }
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

/* Apply a Q16 gain to an s16 sample, rounding half away from zero and
 * accumulating in 64 bits (matches the fixed-point volume applied on the
 * core int16 audio path in audio_driver_flush).
 *
 * The product must not be accumulated in int32: voice volume ranges over
 * -80..+12 dB, so gain_q16 reaches 260904, and 32768 * 260904 needs 34
 * bits. Anything above 0 dB overflows on loud input and wraps to the
 * wrong sign, turning peaks into clicks rather than clamping them.
 *
 * Rounding rather than truncating halves the quantisation error and
 * mirrors the bias across the sign, which keeps the transform
 * odd-symmetric and therefore DC-free on symmetric signals. */
static int32_t audio_mixer_gain_s16(int16_t s, int32_t gain_q16)
{
   int64_t p = (int64_t)s * gain_q16;
   return (int32_t)((p >= 0)
         ?  ((  p + 0x8000) >> 16)
         : -(((-p + 0x8000) >> 16)));
}

#ifdef AUDIO_MIXER_HAS_STREAM
/* Only the WAV and streaming s16 resample paths consult this; a MOD-only or
 * no-codec build would otherwise flag it as unused. */
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
#endif

#ifdef HAVE_RWAV
static bool wav_to_s16(const rwav_t* wav, const uint8_t* src,
      int16_t** pcm, size_t len)
{
   size_t i;
   /* Allocate on a 16-byte boundary, and pad to a multiple of 16 bytes */
   int16_t *s = (int16_t*)memalign_alloc(16,
         ((len + 15) & ~15) * sizeof(int16_t));

   if (!s)
      return false;

   *pcm = s;

   /* The companded and block-coded payloads are not readable a sample
    * at a time - rwav decodes those, natively to s16.  The header is
    * copied with its data offset zeroed because the caller hands over
    * the payload alone, not the file it came from. */
   if (wav->format != RWAV_FORMAT_PCM && wav->format != RWAV_FORMAT_FLOAT)
   {
      rwav_t   h   = *wav;
      int16_t *tmp = (int16_t*)malloc(wav->numsamples
            * wav->numchannels * sizeof(int16_t));
      size_t   i, got;
      h.dataoffset = 0;
      if (!tmp)
      {
         memalign_free(s);
         *pcm = NULL;
         return false;
      }
      got = rwav_decode_s16(&h, src, 0, wav->numsamples, tmp);
      if (wav->numchannels == 1)
         for (i = 0; i < got; i++)
         {
            s[2 * i]     = tmp[i];
            s[2 * i + 1] = tmp[i];
         }
      else if (wav->numchannels == 2)
         memcpy(s, tmp, got * 2 * sizeof(int16_t));
      else
      {
         const int32_t (*co)[2] =
               audio_mixer_downmix_wav_q15[wav->numchannels];
         unsigned c;
         for (i = 0; i < got; i++)
         {
            int64_t l = 0, r = 0;
            for (c = 0; c < wav->numchannels; c++)
            {
               int32_t v = tmp[i * wav->numchannels + c];
               l += (int64_t)v * co[c][0];
               r += (int64_t)v * co[c][1];
            }
            s[2 * i]     = audio_mixer_sat_s16_64(
                  (l >= 0 ? l + 16384 : l - 16384) >> 15);
            s[2 * i + 1] = audio_mixer_sat_s16_64(
                  (r >= 0 ? r + 16384 : r - 16384) >> 15);
         }
      }
      free(tmp);
      return true;
   }

   /* See wav_to_float: more channels than the paths below know about
    * get folded, and a count no fold covers fails rather than leaving
    * the buffer as memalign_alloc left it. */
   if (wav->numchannels > 2 && wav->numchannels <= 8)
   {
      size_t   i2;
      unsigned c;
      const int32_t (*co)[2] =
            audio_mixer_downmix_wav_q15[wav->numchannels];
      for (i2 = 0; i2 < wav->numsamples; i2++)
      {
         int64_t l = 0, r = 0;
         for (c = 0; c < wav->numchannels; c++)
         {
            int32_t v = (int32_t)(wav_sample_unit(wav, src, i2, c)
                  * 32768.0f);
            l += (int64_t)v * co[c][0];
            r += (int64_t)v * co[c][1];
         }
         s[2 * i2]     = audio_mixer_sat_s16_64(
               (l >= 0 ? l + 16384 : l - 16384) >> 15);
         s[2 * i2 + 1] = audio_mixer_sat_s16_64(
               (r >= 0 ? r + 16384 : r - 16384) >> 15);
      }
      return true;
   }
   if (wav->numchannels < 1 || wav->numchannels > 8)
   {
      memalign_free(s);
      *pcm = NULL;
      return false;
   }

   /* Native s16 conversion (no float detour). 16-bit samples are taken
    * verbatim; 8-bit unsigned samples are centered and scaled to s16
    * ((u8 - 128) * 256, i.e. the same magnitude as wav_to_float's
    * (u8 - 128) / 128 mapped to full scale - a multiply rather than a
    * shift because the centered value is negative for half the input
    * range and shifting a negative left is undefined); 24-bit is
    * rounded through
    * the shared accessor; mono is duplicated to stereo, matching
    * wav_to_float's channel handling. Only a float source brings float
    * into this path.
    *
    * src points into the caller's file bytes, so the reads go through
    * rwav.h's accessors: little-endian whatever the host is, and a data
    * chunk is only guaranteed even-aligned. */
   if (wav->bitspersample == 32)
   {
      /* float source on the float-free voice path: the one place a
       * float wav is quantised, rounded and saturated in the float
       * domain (casting out-of-range or non-finite values is
       * undefined; non-finite pins to zero). */
      size_t n = wav->numsamples * ((wav->numchannels == 2) ? 2 : 1);
      int16_t *d = s;

      for (i = 0; i < n; i++)
      {
         float v = rwav_f32(src + i * 4) * 32768.0f;
         int16_t q;
         if (!(v > -1e9f && v < 1e9f))
            v = 0.0f;
         v += (v >= 0.0f) ? 0.5f : -0.5f;
         if (v >  32767.0f) v =  32767.0f;
         if (v < -32768.0f) v = -32768.0f;
         q = (int16_t)(int)v;
         if (wav->numchannels == 1)
         {
            *d++ = q;
            *d++ = q;
         }
         else
            *d++ = q;
      }
   }
   else if (wav->bitspersample == 8)
   {
      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--, src++)
         {
            int16_t v = (int16_t)(((int)*src - 128) * 256);
            *s++      = v;
            *s++      = v;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--, src += 2)
         {
            *s++ = (int16_t)(((int)src[0] - 128) * 256);
            *s++ = (int16_t)(((int)src[1] - 128) * 256);
         }
      }
   }
   else if (wav->bitspersample == 24)
   {
      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--, src += 3)
         {
            int16_t v = rwav_s24_to_s16(src);
            *s++      = v;
            *s++      = v;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--, src += 6)
         {
            *s++ = rwav_s24_to_s16(src);
            *s++ = rwav_s24_to_s16(src + 3);
         }
      }
   }
   else
   {
      if (wav->numchannels == 1)
      {
         for (i = wav->numsamples; i != 0; i--, src += 2)
         {
            int16_t v = rwav_s16(src);
            *s++      = v;
            *s++      = v;
         }
      }
      else if (wav->numchannels == 2)
      {
         for (i = wav->numsamples; i != 0; i--, src += 4)
         {
            *s++ = rwav_s16(src);
            *s++ = rwav_s16(src + 2);
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
   size_t pad;
   void  *re    = NULL;
   double ratio = (double)s_rate / (double)rate;

   re = sinc_resampler_int16_init((ratio < 1.0) ? ratio : 1.0,
         audio_mixer_i16_quality(quality));

   if (!re)
      return false;

   /* Size by the predicted output count plus a ratio-scaled safeguard,
    * exactly like one_shot_resample, so the s16 buffer carries the same
    * frame count as the float buffer. The excess the resampler reports
    * over the estimate grows with upsampling, so the pad has to grow
    * with it too; a fixed pad let the clamp below truncate the tail at
    * large ratios. The buffer is zeroed so any undershoot tail reads as
    * silence. */
   pad           = 2 * (size_t)(ratio + 1.0) + 32;
   *samples_out  = (size_t)(samples_in * ratio);
   alloc_samples = ((*samples_out + pad) + 15) & ~15;
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

   /* As in one_shot_resample: report what came out, not what was
    * predicted, so the tail is not dropped. */
   *samples_out = info.output_frames * 2;
   if (*samples_out > alloc_samples)
      *samples_out = alloc_samples;
   return true;
}
#endif

/* Defined below, next to each other so the two pipelines stay
 * visibly parallel; declared here because load_wav builds whichever
 * one the caller asked for. */
static bool wav_build_float(audio_mixer_sound_t* sound,
      const char *resampler_ident, enum resampler_quality quality);
static bool wav_build_s16(audio_mixer_sound_t* sound,
      enum resampler_quality quality);

audio_mixer_sound_t* audio_mixer_load_wav(void *buffer, int32_t size,
      const char *resampler_ident, enum resampler_quality quality,
      bool want_s16)
{
#ifdef HAVE_RWAV
   /* WAV data */
   rwav_t wav;
   /* Result */
   audio_mixer_sound_t* sound = NULL;
   uint8_t* src               = NULL;

   wav.bitspersample          = 0;
   wav.numchannels            = 0;
   wav.samplerate             = 0;
   wav.numsamples             = 0;
   wav.subchunk2size          = 0;
   wav.samples                = NULL;
   wav.dataoffset             = 0;

   /* Header only: the converters read the samples straight out of the
    * buffer handed here, so the decoded copy rwav_load would have made
    * - as large as the file, and thrown away as soon as it had been
    * converted - never exists. */
   if (rwav_parse(&wav, buffer, size) != RWAV_ITERATE_DONE)
      return NULL;

   if (!wav.subchunk2size)
      return NULL;

   /* Keep the source samples. The caller owns the buffer it passed and
    * frees it as soon as this returns, so the sound cannot borrow
    * them; and without them the only way to produce the second
    * pipeline later would be to derive it from the first pipeline's
    * output, which is exactly what this replaces. Cost is the data
    * chunk, held for the life of the sound. */
   if (!(src = (uint8_t*)malloc(wav.subchunk2size)))
      return NULL;

   memcpy(src, (const uint8_t*)buffer + wav.dataoffset,
         wav.subchunk2size);

   if (!(sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound))))
   {
      free(src);
      return NULL;
   }

   sound->type          = AUDIO_MIXER_TYPE_WAV;
   sound->types.wav.src = src;
   sound->types.wav.hdr = wav;
   /* dataoffset addressed the caller's buffer; the copy starts at the
    * samples, so nothing may use it again */
   sound->types.wav.hdr.dataoffset = 0;
   sound->types.wav.hdr.samples    = NULL;

   /* Build the pipeline the caller asked for now, so that triggering
    * the sound later allocates nothing. The other one is built only
    * if a mode flip actually asks for it. */
   if (!(want_s16 ? wav_build_s16(sound, quality)
                  : wav_build_float(sound, resampler_ident, quality)))
   {
      audio_mixer_destroy(sound);
      return NULL;
   }

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_wav_stream(void *buffer, int32_t size)
{
#ifdef HAVE_RWAV
   audio_mixer_sound_t* sound;

   if (!buffer || size <= 0)
      return NULL;

   if (!(sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound))))
      return NULL;

   /* nothing is decoded here: the voice reads frames out of these
    * bytes as it mixes them */
   sound->type              = AUDIO_MIXER_TYPE_WAV_STREAM;
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;

   return sound;
#else
   (void)buffer;
   (void)size;
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_ogg(void *buffer, int32_t size)
{
#if defined(HAVE_RVORBIS) || defined(HAVE_ROPUS) || defined(HAVE_RFLAC)
   audio_mixer_sound_t* sound;
   enum audio_mixer_type mt = AUDIO_MIXER_TYPE_OGG;

   if (!buffer || size <= 0)
      return NULL;

#ifdef HAVE_RFLAC
   /* And FLAC (RFC 5334), which .oga usually carries and .ogg may. */
   if (audio_transfer_ogg_audio_type(buffer, (size_t)size)
         == AUDIO_TYPE_FLAC)
      mt = AUDIO_MIXER_TYPE_FLAC;
#endif
#ifdef HAVE_ROPUS
   /* An .ogg file legitimately wraps Opus as well as Vorbis; route by
    * the identification header, not the extension.  The Opus arm's
    * Ogg buffer mode takes the whole file as-is. */
   if (audio_transfer_ogg_audio_type(buffer, (size_t)size)
         == AUDIO_TYPE_OPUS)
      mt = AUDIO_MIXER_TYPE_OPUS;
#endif
#ifndef HAVE_RVORBIS
   if (mt == AUDIO_MIXER_TYPE_OGG)
      return NULL;
#endif

   sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = mt;
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

audio_mixer_sound_t* audio_mixer_load_m4a(void *buffer, int32_t size)
{
#ifdef HAVE_RAAC
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_M4A;
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_opus(void *buffer, int32_t size)
{
#ifdef HAVE_ROPUS
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type           = AUDIO_MIXER_TYPE_OPUS;
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_weba(void *buffer, int32_t size)
{
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS) \
 || defined(HAVE_RAAC) || defined(HAVE_RFLAC))
   audio_mixer_sound_t* sound;
   enum audio_type_enum ty = audio_transfer_webm_audio_type(buffer,
         (size_t)size);
   enum audio_mixer_type mt;

   /* Resolve to the existing sound type whose streaming arm accepts
    * the whole WebM buffer; nothing downstream ever sees WEBA. */
   if (ty == AUDIO_TYPE_OPUS)
      mt = AUDIO_MIXER_TYPE_OPUS;
   else if (ty == AUDIO_TYPE_VORBIS)
      mt = AUDIO_MIXER_TYPE_OGG;
#ifdef HAVE_RAAC
   else if (ty == AUDIO_TYPE_AAC)
      mt = AUDIO_MIXER_TYPE_M4A;
#endif
#ifdef HAVE_RFLAC
   else if (ty == AUDIO_TYPE_FLAC)
      mt = AUDIO_MIXER_TYPE_FLAC;
#endif
   else
      return NULL;

   if (!(sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound))))
      return NULL;
   sound->type           = mt;
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;
   return sound;
#else
   return NULL;
#endif
}

audio_mixer_sound_t* audio_mixer_load_mod(void *buffer, int32_t size)
{
#ifdef HAVE_RMODTRACKER
   audio_mixer_sound_t* sound = (audio_mixer_sound_t*)calloc(1, sizeof(*sound));

   if (!sound)
      return NULL;

   sound->type              = AUDIO_MIXER_TYPE_MOD;
   sound->types.stream.size = size;
   sound->types.stream.data = buffer;

   return sound;
#else
   return NULL;
#endif
}

void audio_mixer_sound_set_data_owner(audio_mixer_sound_t *sound,
      void *owner, void (*release)(void *owner))
{
   if (!sound)
   {
      /* ownership transfers in every outcome */
      if (owner && release)
         release(owner);
      return;
   }
   sound->data_owner   = owner;
   sound->data_release = release;
}

#ifdef HAVE_ROPUS
void audio_mixer_sound_set_avail(audio_mixer_sound_t *sound, size_t avail)
{
   if (sound)
      sound->avail = avail;
}

void audio_mixer_sound_set_end_granule(audio_mixer_sound_t *sound,
      int64_t end_granule)
{
   if (sound)
      sound->end_granule = end_granule;
}
#endif

/* Compressed-byte read position of a stream voice's decoder within
 * its source buffer - the windowed-source feeder's input.  Returns
 * 0 for anything that is not a live buffer-mode stream voice.  Takes
 * the voice lock: safe against the mixing thread. */
/* Raise a live stream voice's resident prefix - the windowed feeder's
 * output, the mirror of audio_mixer_voice_buffer_tell's input.  Only
 * the WebM container arms act on it; a no-op for every other type and
 * for anything that is not a live buffer-mode stream voice.  Takes the
 * voice lock: safe against the mixing thread. */
void audio_mixer_voice_set_avail(audio_mixer_voice_t *voice, size_t avail)
{
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
   if (!voice)
      return;
#ifdef AUDIO_MIXER_HAS_STREAM
   AUDIO_MIXER_LOCK(voice);
   switch (voice->type)
   {
#ifdef HAVE_RVORBIS
      case AUDIO_MIXER_TYPE_OGG:
         audio_transfer_set_avail(voice->types.stream.stream,
               AUDIO_TYPE_VORBIS, avail);
         break;
#endif
#ifdef HAVE_ROPUS
      case AUDIO_MIXER_TYPE_OPUS:
         audio_transfer_set_avail(voice->types.stream.stream,
               AUDIO_TYPE_OPUS, avail);
         break;
#endif
      default:
         break;
   }
   AUDIO_MIXER_UNLOCK(voice);
#endif
#else
   (void)voice; (void)avail;
#endif
}

size_t audio_mixer_voice_buffer_tell(audio_mixer_voice_t *voice)
{
   size_t r = 0;
   if (!voice)
      return 0;
#ifdef AUDIO_MIXER_HAS_STREAM
   AUDIO_MIXER_LOCK(voice);
   switch (voice->type)
   {
#ifdef HAVE_RWAV
      case AUDIO_MIXER_TYPE_WAV_STREAM:
         r = audio_transfer_buffer_tell(voice->types.stream.stream,
               AUDIO_TYPE_WAV);
         break;
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_MIXER_TYPE_OGG:
         r = audio_transfer_buffer_tell(voice->types.stream.stream,
               AUDIO_TYPE_VORBIS);
         break;
#endif
#ifdef HAVE_RMP3
      case AUDIO_MIXER_TYPE_MP3:
         r = audio_transfer_buffer_tell(voice->types.stream.stream,
               AUDIO_TYPE_MP3);
         break;
#endif
#ifdef HAVE_RFLAC
      case AUDIO_MIXER_TYPE_FLAC:
         r = audio_transfer_buffer_tell(voice->types.stream.stream,
               AUDIO_TYPE_FLAC);
         break;
#endif
#ifdef HAVE_RAAC
      case AUDIO_MIXER_TYPE_M4A:
         r = audio_transfer_buffer_tell(voice->types.stream.stream,
               AUDIO_TYPE_AAC);
         break;
#endif
      default:
         break;
   }
   AUDIO_MIXER_UNLOCK(voice);
#endif
   return r;
}

void audio_mixer_destroy(audio_mixer_sound_t* sound)
{
   void *handle = NULL;
   if (!sound)
      return;

   if (sound->data_owner)
      /* the compressed source was borrowed: hand it back; the
       * per-type paths below leave borrowed data alone */
      sound->data_release(sound->data_owner);

   switch (sound->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle = (void*)sound->types.wav.pcm;
         if (handle)
            memalign_free(handle);
         handle = (void*)sound->types.wav.pcm_s16;
         if (handle)
            memalign_free(handle);
#ifdef HAVE_RWAV
         handle = (void*)sound->types.wav.src;
         if (handle)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_WAV_STREAM:
#ifdef HAVE_RWAV
         handle = (void*)sound->types.stream.data;
         if (handle && !sound->data_owner)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_RVORBIS
         handle = (void*)sound->types.stream.data;
         if (handle && !sound->data_owner)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_RMODTRACKER
         handle = (void*)sound->types.stream.data;
         if (handle && !sound->data_owner)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
         handle = (void*)sound->types.stream.data;
         if (handle && !sound->data_owner)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
         handle = (void*)sound->types.stream.data;
         if (handle && !sound->data_owner)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_M4A:
#ifdef HAVE_RAAC
         handle = (void*)sound->types.stream.data;
         if (handle && !sound->data_owner)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_OPUS:
#ifdef HAVE_ROPUS
         handle = (void*)sound->types.stream.data;
         if (handle && !sound->data_owner)
            free(handle);
#endif
         break;
      case AUDIO_MIXER_TYPE_WEBA: /* resolved at load; never stored */
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   free(sound);
}

/* Build one pipeline's PCM from the WAV's own source samples.
 *
 * Each pipeline runs its own converter and its own resampler over the
 * source; neither is ever derived from the other's finished output.
 * That is the whole point: the previous wav_ensure_* pair produced an
 * s16 buffer by quantising the float resampler's result, so an s16
 * voice on a float-free platform could play audio that had been
 * through a float resampler after all.
 *
 * Called at load for the mode the caller asked for, so triggering a
 * sound allocates nothing. The only path that builds at play time is
 * a mode flip with the sound still loaded (a core switch), and it now
 * builds from the source instead of from the other pipeline. */
#ifdef HAVE_RWAV
static bool wav_build_float(audio_mixer_sound_t* sound,
      const char *resampler_ident, enum resampler_quality quality)
{
   float  *pcm     = NULL;
   size_t  samples;

   if (sound->types.wav.pcm)
      return true;
   if (!sound->types.wav.src)
      return false;

   samples = sound->types.wav.hdr.numsamples * 2;

   if (!wav_to_float(&sound->types.wav.hdr, sound->types.wav.src,
         &pcm, samples))
      return false;

   if (sound->types.wav.hdr.samplerate != s_rate)
   {
      float *resampled = NULL;

      if (!one_shot_resample(pcm, samples,
            sound->types.wav.hdr.samplerate, resampler_ident, quality,
            &resampled, &samples))
      {
         memalign_free((void*)pcm);
         return false;
      }

      memalign_free((void*)pcm);
      pcm = resampled;
   }

   sound->types.wav.pcm    = pcm;
   sound->types.wav.frames = (unsigned)(samples / 2);
   return true;
}

static bool wav_build_s16(audio_mixer_sound_t* sound,
      enum resampler_quality quality)
{
   int16_t *pcm    = NULL;
   size_t   samples;

   if (sound->types.wav.pcm_s16)
      return true;
   if (!sound->types.wav.src)
      return false;

   samples = sound->types.wav.hdr.numsamples * 2;

   if (!wav_to_s16(&sound->types.wav.hdr, sound->types.wav.src,
         &pcm, samples))
      return false;

   if (sound->types.wav.hdr.samplerate != s_rate)
   {
      int16_t *resampled = NULL;

      if (!one_shot_resample_s16(pcm, samples,
            sound->types.wav.hdr.samplerate, quality,
            &resampled, &samples))
      {
         memalign_free((void*)pcm);
         return false;
      }

      memalign_free((void*)pcm);
      pcm = resampled;
   }

   sound->types.wav.pcm_s16    = pcm;
   sound->types.wav.frames_s16 = (unsigned)(samples / 2);
   return true;
}
#else
/* Without RWAV a decoded WAV sound can never be created, so these are
 * unreachable. They exist so the play switch needs no guard of its
 * own. */
static bool wav_build_float(audio_mixer_sound_t* sound,
      const char *resampler_ident, enum resampler_quality quality)
{
   (void)sound;
   (void)resampler_ident;
   (void)quality;
   return false;
}

static bool wav_build_s16(audio_mixer_sound_t* sound,
      enum resampler_quality quality)
{
   (void)sound;
   (void)quality;
   return false;
}
#endif

/* No volume parameter: it was never read, and taking a float here put
 * one in the s16 play path for nothing. */
static bool audio_mixer_play_wav(audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice, bool repeat,
      audio_mixer_stop_cb_t stop_cb)
{
   voice->types.wav.position = 0;
   return true;
}

#ifdef AUDIO_MIXER_HAS_STREAM
/* Shared streaming path (WAV / OGG / FLAC / MP3). audio_transfer already
 * abstracts the codec, so one set of play/mix/release functions serves
 * them all; the caller passes the matching enum audio_type_enum. */
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

#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
   /* Windowed WebM: bound the container header parse to the resident
    * head, so opening does not walk the segment to the end of a file
    * whose middle is reserved rather than populated. */
   if (sound->avail)
      audio_transfer_set_avail(xfer, type, sound->avail);
#endif
#ifdef HAVE_ROPUS
   /* Windowed Ogg-Opus: hand the decoder the last-page granule the
    * feeder found, so its buffer setup skips the full-file end scan
    * (the tail is not resident under windowing).  No-op for 0 (not
    * supplied) and for every non-Opus type. */
   if (sound->end_granule > 0)
      audio_transfer_set_end_granule(xfer, type, sound->end_granule);
#endif

   /* Say what rate we mix at: an arm that can synthesise there will,
    * and the resampler below then has nothing to do. */
   audio_transfer_set_output_rate(xfer, type, (unsigned)s_rate);

   if (!audio_transfer_start(xfer, type))
      goto error;

   {
      unsigned ch = 0;
      audio_transfer_info(xfer, type, &ch, &rate, NULL);
      /* Wider than stereo is folded at fill time rather than refused:
       * the decoders reach eight channels (Opus) and sixteen
       * (Vorbis), and a voice mixes stereo.  Past the table's reach
       * there is no sensible fold, so those are still turned away. */
      if (ch < 1 || ch > 8)
         goto error;
      voice->types.stream.channels = ch;
   }

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
   voice->types.stream.decode_buf  = (float*)memalign_alloc(16,
         AUDIO_MIXER_TEMP_BUFFER * sizeof(float));

   if (!sbuf || !voice->types.stream.decode_buf)
   {
      /* error: only frees the transfer, and neither buffer is on the
       * voice yet in the sbuf case, so release them here.  One of the
       * two having succeeded is newly possible now that there are two
       * of them. */
      memalign_free(sbuf);
      memalign_free(voice->types.stream.decode_buf);
      voice->types.stream.decode_buf = NULL;
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
   if (voice->types.stream.decode_buf)
      memalign_free(voice->types.stream.decode_buf);
   if (voice->types.stream.buffer_s16)
      memalign_free(voice->types.stream.buffer_s16);
   if (voice->types.stream.decode_buf_s16)
      memalign_free(voice->types.stream.decode_buf_s16);
   if (voice->types.stream.resampler_int16)
      sinc_resampler_int16_free(voice->types.stream.resampler_int16);
}

static bool audio_mixer_play_stream_s16(
      audio_mixer_sound_t* sound,
      audio_mixer_voice_t* voice,
      bool repeat, int32_t gain,
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
   (void)gain;
   (void)stop_cb;

   if (!xfer)
      return false;
   audio_transfer_set_buffer_ptr(xfer, type,
         (void*)sound->types.stream.data, sound->types.stream.size);
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
   /* Windowed WebM: bound the header parse to the head (see the f32
    * path). */
   if (sound->avail)
      audio_transfer_set_avail(xfer, type, sound->avail);
#endif
#ifdef HAVE_ROPUS
   /* Windowed Ogg-Opus: skip the decoder's full-file end scan by
    * handing it the feeder's last-page granule (see the f32 path). */
   if (sound->end_granule > 0)
      audio_transfer_set_end_granule(xfer, type, sound->end_granule);
#endif
   /* Say what rate we mix at: an arm that can synthesise there will,
    * and the resampler below then has nothing to do. */
   audio_transfer_set_output_rate(xfer, type, (unsigned)s_rate);

   if (!audio_transfer_start(xfer, type))
   {
      audio_transfer_free(xfer, type);
      return false;
   }
   audio_transfer_info(xfer, type, &channels, &rate, NULL);

   /* Mono is expanded and anything above stereo folded at fill time,
    * as on the float path; past the downmix table's reach there is no
    * sensible fold. */
   if (channels < 1 || channels > 8)
      goto error;
   /* The fill reads this to size its read and pick its fold.  Only the
    * float path used to set it, which was harmless while this one was
    * stereo-only and its fill assumed as much. */
   voice->types.stream.channels = channels;

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
   voice->types.stream.decode_buf_s16 = (int16_t*)memalign_alloc(16,
         AUDIO_MIXER_TEMP_BUFFER * sizeof(int16_t));

   if (!sbuf || !voice->types.stream.decode_buf_s16)
   {
      /* Neither buffer is on the voice yet in the sbuf case, and either
       * one of the two may have succeeded; release both here. */
      memalign_free(sbuf);
      memalign_free(voice->types.stream.decode_buf_s16);
      voice->types.stream.decode_buf_s16 = NULL;
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
   voice->types.stream.ratio           = ratio;
   voice->types.stream.stream          = xfer;
   voice->types.stream.position        = 0;
   voice->types.stream.samples         = 0;

   return true;

error:
   audio_transfer_free(xfer, type);
   return false;
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
            /* float voice: build the float pipeline from the source
             * if it is not there yet (the sound was loaded for s16
             * before a mode flip) */
            res = wav_build_float(sound, resampler_ident, quality)
               && audio_mixer_play_wav(sound, voice, repeat, stop_cb);
            break;
         case AUDIO_MIXER_TYPE_WAV_STREAM:
#ifdef HAVE_RWAV
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_WAV);
#endif
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_RVORBIS
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_VORBIS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_RMODTRACKER
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_MOD);
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
         case AUDIO_MIXER_TYPE_M4A:
#ifdef HAVE_RAAC
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_AAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_OPUS:
#ifdef HAVE_ROPUS
            res = audio_mixer_play_stream(sound, voice, repeat, volume,
                  resampler_ident, quality, stop_cb, AUDIO_TYPE_OPUS);
#endif
            break;
         case AUDIO_MIXER_TYPE_WEBA: /* resolved at load; never stored */
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
      bool repeat, int32_t gain,
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
            res = audio_mixer_play_stream_s16(sound, voice, repeat, gain,
                  quality, stop_cb, AUDIO_TYPE_FLAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV_STREAM:
#ifdef HAVE_RWAV
            res = audio_mixer_play_stream_s16(sound, voice, repeat, gain,
                  quality, stop_cb, AUDIO_TYPE_WAV);
#endif
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_RVORBIS
            res = audio_mixer_play_stream_s16(sound, voice, repeat, gain,
                  quality, stop_cb, AUDIO_TYPE_VORBIS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
            res = audio_mixer_play_stream_s16(sound, voice, repeat, gain,
                  quality, stop_cb, AUDIO_TYPE_MP3);
#endif
            break;
         case AUDIO_MIXER_TYPE_M4A:
#ifdef HAVE_RAAC
            res = audio_mixer_play_stream_s16(sound, voice, repeat, gain,
                  quality, stop_cb, AUDIO_TYPE_AAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_OPUS:
#ifdef HAVE_ROPUS
            res = audio_mixer_play_stream_s16(sound, voice, repeat, gain,
                  quality, stop_cb, AUDIO_TYPE_OPUS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_RMODTRACKER
            res = audio_mixer_play_stream_s16(sound, voice, repeat, gain,
                  quality, stop_cb, AUDIO_TYPE_MOD);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV:
            /* s16 voice: build the s16 pipeline from the source if it
             * is not there yet (the sound was loaded for float before
             * a mode flip) */
            res = wav_build_s16(sound, quality)
               && audio_mixer_play_wav(sound, voice, repeat, stop_cb);
            break;
         case AUDIO_MIXER_TYPE_WEBA: /* resolved at load; never stored */
         case AUDIO_MIXER_TYPE_NONE:
            break;
      }

      break;
   }

   if (res)
   {
      voice->repeat   = repeat;
      voice->gain     = gain;
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
#ifdef HAVE_RWAV
      case AUDIO_MIXER_TYPE_WAV_STREAM:
         audio_mixer_release_stream(voice, AUDIO_TYPE_WAV);
         break;
#endif
#ifdef HAVE_RVORBIS
      case AUDIO_MIXER_TYPE_OGG:
         audio_mixer_release_stream(voice, AUDIO_TYPE_VORBIS);
         break;
#endif
#ifdef HAVE_RMODTRACKER
      case AUDIO_MIXER_TYPE_MOD:
         audio_mixer_release_stream(voice, AUDIO_TYPE_MOD);
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
#ifdef HAVE_RAAC
      case AUDIO_MIXER_TYPE_M4A:
         audio_mixer_release_stream(voice, AUDIO_TYPE_AAC);
         break;
#endif
#ifdef HAVE_ROPUS
      case AUDIO_MIXER_TYPE_OPUS:
         audio_mixer_release_stream(voice, AUDIO_TYPE_OPUS);
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
   unsigned pcm_available           = sound->types.wav.frames_s16
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
         pcm_available              = sound->types.wav.frames_s16 * 2;
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

#ifdef AUDIO_MIXER_HAS_STREAM



static void audio_mixer_mix_stream(float* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      float volume,
      enum audio_type_enum type)
{
   int i;
   float* temp_buffer               = voice->types.stream.decode_buf;
   unsigned buf_free                = (unsigned)(num_frames * 2);
   unsigned temp_samples            = 0;
   float* pcm                       = NULL;

   if (!voice->types.stream.stream)
      return;

   if (voice->types.stream.samples == 0)
   {
again:
      {
         size_t got = 0;
         if (voice->types.stream.channels == 1)
         {
            /* mono source: read into the front, then expand to
             * interleaved stereo in place, descending - sample n-1
             * is read before any destination at or above it is
             * written, so the source is never clobbered */
            unsigned n;
            audio_transfer_read_f32(voice->types.stream.stream, type,
                  temp_buffer, AUDIO_MIXER_TEMP_BUFFER / 2, &got);
            for (n = (unsigned)got; n > 0; n--)
            {
               float s            = temp_buffer[n - 1];
               temp_buffer[2*n-2] = s;
               temp_buffer[2*n-1] = s;
            }
         }
         else if (voice->types.stream.channels == 2)
            audio_transfer_read_f32(voice->types.stream.stream, type,
                  temp_buffer, AUDIO_MIXER_TEMP_BUFFER / 2, &got);
         else
         {
            /* Wider than stereo: the frames a read may ask for follow
             * the channel count, not the stereo figure, or the buffer
             * overruns.  Folded in place afterwards. */
            unsigned sch = voice->types.stream.channels;
            audio_transfer_read_f32(voice->types.stream.stream, type,
                  temp_buffer,
                  audio_mixer_frames_for(sch, AUDIO_MIXER_TEMP_BUFFER),
                  &got);
            audio_mixer_downmix_f32(temp_buffer, got, sch,
                  audio_mixer_downmix_table(type, sch));
         }
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
}

static void audio_mixer_mix_stream_s16(int16_t* buffer, size_t num_frames,
      audio_mixer_voice_t* voice,
      int32_t gain_q16,
      enum audio_type_enum type)
{
   int i;
   struct resampler_data_int16 info;
   int16_t *temp_buffer  = voice->types.stream.decode_buf_s16;
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
         unsigned sch = voice->types.stream.channels;
         if (sch == 1)
         {
            /* Mono: the resampler downstream reads input_frames as
             * stereo pairs, so a mono read leaves the upper half of
             * every frame stale.  Expand in place, descending, so a
             * sample is read before anything at or above it is
             * written - the f32 path has always done this and this
             * one never did. */
            unsigned n;
            audio_transfer_read_s16(voice->types.stream.stream, type,
                  temp_buffer, AUDIO_MIXER_TEMP_BUFFER / 2, &got);
            for (n = (unsigned)got; n > 0; n--)
            {
               int16_t v          = temp_buffer[n - 1];
               temp_buffer[2*n-2] = v;
               temp_buffer[2*n-1] = v;
            }
         }
         else if (sch > 2)
         {
            /* See the f32 path: read what the buffer holds at this
             * channel count, then fold to stereo in place. */
            audio_transfer_read_s16(voice->types.stream.stream, type,
                  temp_buffer,
                  audio_mixer_frames_for(sch, AUDIO_MIXER_TEMP_BUFFER),
                  &got);
            audio_mixer_downmix_s16(temp_buffer, got, sch,
                  audio_mixer_downmix_table(type, sch));
         }
         else
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
         case AUDIO_MIXER_TYPE_WAV_STREAM:
#ifdef HAVE_RWAV
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_WAV);
#endif
            break;
         case AUDIO_MIXER_TYPE_OGG:
#ifdef HAVE_RVORBIS
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_VORBIS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_RMODTRACKER
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_MOD);
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
         case AUDIO_MIXER_TYPE_M4A:
#ifdef HAVE_RAAC
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_AAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_OPUS:
#ifdef HAVE_ROPUS
            audio_mixer_mix_stream(buffer, num_frames, voice, volume, AUDIO_TYPE_OPUS);
#endif
            break;
         case AUDIO_MIXER_TYPE_WEBA: /* resolved at load; never stored */
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
      int32_t gain_override, bool override)
{
   unsigned i;
   audio_mixer_voice_t* voice = s_voices;

   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
   {
      int32_t gain_q16;

      AUDIO_MIXER_LOCK(voice);

      if (!voice->is_s16)
      {
         AUDIO_MIXER_UNLOCK(voice);
         continue;
      }

      /* Already Q16 on both sides, so nothing is computed here. This
       * used to convert a float per voice per mix call, on the audio
       * thread, in the pipeline that exists to avoid float. */
      gain_q16 = (override) ? gain_override : voice->gain;

      switch (voice->type)
      {
         case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_FLAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV_STREAM:
#ifdef HAVE_RWAV
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_WAV);
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
         case AUDIO_MIXER_TYPE_M4A:
#ifdef HAVE_RAAC
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_AAC);
#endif
            break;
         case AUDIO_MIXER_TYPE_OPUS:
#ifdef HAVE_ROPUS
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_OPUS);
#endif
            break;
         case AUDIO_MIXER_TYPE_MOD:
#ifdef HAVE_RMODTRACKER
            audio_mixer_mix_stream_s16(buffer, num_frames, voice, gain_q16, AUDIO_TYPE_MOD);
#endif
            break;
         case AUDIO_MIXER_TYPE_WAV:
            audio_mixer_mix_wav_s16(buffer, num_frames, voice, gain_q16);
            break;
         case AUDIO_MIXER_TYPE_WEBA: /* resolved at load; never stored */
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

/* Whether any active voice would be handled by audio_mixer_mix (float) /
 * audio_mixer_mix_s16 (int16).  The frontend uses these to skip the
 * cross-format fold when every active voice already matches the buffer it
 * is mixing into. */
bool audio_mixer_has_float_voices(void)
{
   unsigned i;
   const audio_mixer_voice_t *voice = s_voices;
   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
      if (voice->type != AUDIO_MIXER_TYPE_NONE && !voice->is_s16)
         return true;
   return false;
}

bool audio_mixer_has_s16_voices(void)
{
   unsigned i;
   const audio_mixer_voice_t *voice = s_voices;
   for (i = 0; i < AUDIO_MIXER_MAX_VOICES; i++, voice++)
      if (voice->type != AUDIO_MIXER_TYPE_NONE && voice->is_s16)
         return true;
   return false;
}

int32_t audio_mixer_voice_get_gain(audio_mixer_voice_t *voice)
{
   if (!voice)
      return 0;

   return voice->gain;
}

void audio_mixer_voice_set_gain(audio_mixer_voice_t *voice, int32_t gain)
{
   if (!voice)
      return;

   AUDIO_MIXER_LOCK(voice);
   voice->gain = gain;
   AUDIO_MIXER_UNLOCK(voice);
}

void audio_mixer_voice_set_volume(audio_mixer_voice_t *voice, float val)
{
   if (!voice)
      return;

   AUDIO_MIXER_LOCK(voice);
   voice->volume = val;
   /* Keep the s16 gain in step, so a caller that only knows the float
    * form still drives an s16 voice. The conversion happens here, on
    * the control path, and not on the audio thread. */
   voice->gain   = (int32_t)(val * 65536.0f + 0.5f);
   AUDIO_MIXER_UNLOCK(voice);
}
